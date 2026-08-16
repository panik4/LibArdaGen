#include "simulation/Simulation.h"
#include "simulation/SimulationDevelopment.h"
#include "simulation/SimulationEvents.h"
#include "simulation/SimulationGeography.h"
#include "simulation/SimulationInternal.h"
#include "simulation/SimulationState.h"
#include "simulation/SimulationValidation.h"
#include "simulation/SimulationWarfare.h"

#include "RandNum.h"
#include "rendering/Png.h"
#include "utils/Cfg.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <numeric>
#include <set>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace Arda::Simulation {
namespace {

using detail::NormalizedInput;
using development::capacityEraMultiplier;
using development::capacityExpansionMultiplier;
using development::phaseMultiplier;
using development::polityCapacityAt;
using events::apply;
using geography::normalize;
using state::refreshDominantCulture;
using state::relocateCapital;
using state::territoriesByPolity;
using warfare::resolveWars;

std::vector<PolityId> contiguousSuccessorAssignments(
    const std::vector<ProvinceId> &provinceIds, size_t successorCount,
    const std::map<ProvinceId, std::vector<ProvinceId>> &neighbours) {
  std::vector<PolityId> assignments(provinceIds.size(), -1);
  std::map<ProvinceId, size_t> indices;
  for (size_t index = 0; index < provinceIds.size(); ++index)
    indices.emplace(provinceIds[index], index);

  std::vector<std::vector<size_t>> frontiers(successorCount);
  std::set<size_t> unassigned;
  for (size_t index = 0; index < provinceIds.size(); ++index)
    unassigned.insert(index);
  for (size_t successor = 0; successor < successorCount; ++successor) {
    const auto seed =
        provinceIds[(successor * provinceIds.size()) / successorCount];
    const auto seedIndex = indices.at(seed);
    assignments[seedIndex] = static_cast<PolityId>(successor);
    unassigned.erase(seedIndex);
    frontiers[successor].push_back(seedIndex);
  }

  while (!unassigned.empty()) {
    bool assigned = false;
    for (size_t successor = 0;
         successor < successorCount && !unassigned.empty(); ++successor) {
      auto &frontier = frontiers[successor];
      for (size_t frontierIndex = 0; frontierIndex < frontier.size();
           ++frontierIndex) {
        const auto provinceId = provinceIds[frontier[frontierIndex]];
        for (const auto neighbourId : neighbours.at(provinceId)) {
          const auto candidate = indices.find(neighbourId);
          if (candidate == indices.end() ||
              !unassigned.contains(candidate->second))
            continue;
          assignments[candidate->second] = static_cast<PolityId>(successor);
          unassigned.erase(candidate->second);
          frontier.push_back(candidate->second);
          assigned = true;
          break;
        }
        if (assigned)
          break;
      }
    }
    if (assigned)
      continue;
    const auto orphan = *unassigned.begin();
    size_t successor = 0;
    size_t smallestTerritory = provinceIds.size();
    for (size_t candidate = 0; candidate < successorCount; ++candidate) {
      const auto territorySize =
          std::count(assignments.begin(), assignments.end(),
                     static_cast<PolityId>(candidate));
      if (territorySize < smallestTerritory) {
        smallestTerritory = territorySize;
        successor = candidate;
      }
    }
    assignments[orphan] = static_cast<PolityId>(successor);
    unassigned.erase(orphan);
    frontiers[successor].push_back(orphan);
  }
  return assignments;
}

bool chance(double probability) {
  return probability > 0.0 && RandNum::getRandom<double>(1.0) < probability;
}

PolityId strongestPolityOf(const State &state) {
  const auto strongest = std::max_element(
      state.polityStrengths.begin(), state.polityStrengths.end(),
      [](const auto &left, const auto &right) {
        return left.second.score < right.second.score;
      });
  return strongest == state.polityStrengths.end() ? NoPolity : strongest->first;
}

Fwg::Gfx::Colour randomPolityColour() {
  return {static_cast<unsigned char>(RandNum::getRandom<int>(256)),
          static_cast<unsigned char>(RandNum::getRandom<int>(256)),
          static_cast<unsigned char>(RandNum::getRandom<int>(256))};
}

int stepFor(const Configuration &configuration, Year year) {
  if (year < configuration.classicalStartYear)
    return configuration.ancientStepYears;
  if (year < configuration.medievalStartYear)
    return configuration.classicalStepYears;
  if (year < configuration.modernStartYear)
    return configuration.medievalStepYears;
  return configuration.modernStepYears;
}

const char *eventTypeName(EventType type) {
  switch (type) {
  case EventType::InitializeProvince:
    return "InitializeProvince";
  case EventType::CreatePolity:
    return "CreatePolity";
  case EventType::CreateSuccessorPolity:
    return "CreateSuccessorPolity";
  case EventType::TransferProvince:
    return "TransferProvince";
  case EventType::DissolvePolity:
    return "DissolvePolity";
  case EventType::CreateCulture:
    return "CreateCulture";
  case EventType::SetCulture:
    return "SetCulture";
  case EventType::SetPrimaryCulture:
    return "SetPrimaryCulture";
  case EventType::CreateReligion:
    return "CreateReligion";
  case EventType::SetReligion:
    return "SetReligion";
  case EventType::UpdatePopulation:
    return "UpdatePopulation";
  case EventType::UpdatePolityStrength:
    return "UpdatePolityStrength";
  case EventType::UpdateDevelopment:
    return "UpdateDevelopment";
  case EventType::UpdateCarryingCapacity:
    return "UpdateCarryingCapacity";
  case EventType::MigratePopulation:
    return "MigratePopulation";
  case EventType::SetRegionalPhase:
    return "SetRegionalPhase";
  case EventType::SetSuperRegion:
    return "SetSuperRegion";
  case EventType::CreateSuperRegion:
    return "CreateSuperRegion";
  case EventType::SetSuperRegionPhase:
    return "SetSuperRegionPhase";
  case EventType::MigrateCulturePopulation:
    return "MigrateCulturePopulation";
  case EventType::ConvertCulturePopulation:
    return "ConvertCulturePopulation";
  case EventType::ColonizeProvince:
    return "ColonizeProvince";
  case EventType::ConsolidateRegion:
    return "ConsolidateRegion";
  case EventType::SetCapital:
    return "SetCapital";
  }
  return "Unknown";
}

using detail::AppendEvent;
using warfare::capitalsShareLandMass;
using warfare::expansionBorderScore;
using warfare::expansionTargetWeakness;
using warfare::hasMaritimeWarConnection;
using warfare::maritimeRangeForYear;
using warfare::polityDistance;
using warfare::remainsContiguousAfterConquest;

void logYearProgress(Year year, Year targetYear,
                     const std::map<EventType, std::size_t> &eventCounts) {
  std::ostringstream eventProgress;
  eventProgress << "Simulating year " << year << " of " << targetYear
                << " | cumulative events: ";
  bool firstEventCount = true;
  for (const auto &[eventType, count] : eventCounts) {
    if (!firstEventCount)
      eventProgress << ", ";
    eventProgress << eventTypeName(eventType) << '=' << count;
    firstEventCount = false;
  }
  Fwg::Utils::Logging::logLine(eventProgress.str());
}

void integrateCultures(const Year nextYear, double centuries,
                       const Configuration &configuration, State &state,
                       const AppendEvent &append) {
  double stabilityThreshold = 0.0;
  for (const auto &[polityId, strength] : state.polityStrengths)
    stabilityThreshold = std::max(stabilityThreshold, strength.score * 0.5);
  for (const auto &[provinceId, province] : state.provinces) {
    if (state.polityStrengths.at(province.owner).score < stabilityThreshold ||
        province.culturePopulations.size() < 2)
      continue;
    const auto dominantCulture = state.dominantCultureOf(provinceId);
    for (const auto &[cultureId, culturalPopulation] :
         province.culturePopulations) {
      if (cultureId == dominantCulture)
        continue;
      const auto conversion = culturalPopulation *
                              configuration.culturalIntegrationRatePerCentury *
                              centuries;
      if (conversion > 0.0)
        append({nextYear, EventType::ConvertCulturePopulation, provinceId, -1,
                NoPolity, NoPolity, dominantCulture, NoReligion, conversion,
                0.0, "stable polity cultural integration", cultureId});
    }
  }
}

void colonize(const Year nextYear, double centuries,
              const Configuration &configuration,
              const NormalizedInput &normalized, State &state,
              const AppendEvent &append, PolityId strongestPolity) {
  if (nextYear < configuration.colonizationStartYear ||
      nextYear >= configuration.regionOwnershipYear ||
      strongestPolity == NoPolity)
    return;
  ProvinceId sourceProvince = -1;
  for (const auto &[provinceId, province] : state.provinces) {
    if (province.owner == strongestPolity && province.coastal &&
        (sourceProvince < 0 ||
         province.development > state.provinces.at(sourceProvince).development))
      sourceProvince = provinceId;
  }
  ProvinceId targetProvince = -1;
  if (sourceProvince < 0)
    return;
  for (const auto &[provinceId, province] : state.provinces) {
    const auto targetRegion = normalized.provinceRegions.at(provinceId);
    const auto targetSuperRegion = state.regionSuperRegions.at(targetRegion);
    const bool lagging = state.superRegions.at(targetSuperRegion).phase ==
                         SuperRegionPhase::Lagging;
    const bool reachableByLand =
        normalized.provinceContinents.at(provinceId) ==
        normalized.provinceContinents.at(sourceProvince);
    const auto maritimeRange = maritimeRangeForYear(nextYear, configuration);
    const auto routesToProvince = normalized.seaRoutesTo.find(provinceId);
    const bool reachableBySea =
        routesToProvince != normalized.seaRoutesTo.end() &&
        std::any_of(routesToProvince->second.begin(),
                    routesToProvince->second.end(), [&](const auto &route) {
                      return route.distance <= maritimeRange &&
                             state.provinces.at(route.provinceId).coastal &&
                             state.provinces.at(route.provinceId).owner ==
                                 strongestPolity;
                    });
    if (province.owner != strongestPolity && lagging &&
        (reachableByLand || reachableBySea) &&
        (targetProvince < 0 ||
         province.development < state.provinces.at(targetProvince).development))
      targetProvince = provinceId;
  }
  if (targetProvince < 0 ||
      !chance(configuration.colonialSettlementRatePerCentury * centuries *
              capacityExpansionMultiplier(
                  configuration, nextYear,
                  static_cast<size_t>(std::count_if(
                      state.provinces.begin(), state.provinces.end(),
                      [strongestPolity](const auto &entry) {
                        return entry.second.owner == strongestPolity;
                      })))))
    return;
  append({nextYear, EventType::ColonizeProvince, targetProvince,
          normalized.provinceRegions.at(targetProvince), strongestPolity,
          state.provinces.at(targetProvince).owner, -1, NoReligion, 1.0, 0.0,
          "colonial settlement"});
}

void maritimeExpansion(
    const Year nextYear, double centuries, const Configuration &configuration,
    const NormalizedInput &normalized, State &state, const AppendEvent &append,
    const std::map<PolityId, std::vector<ProvinceId>> &territories) {
  if (nextYear < configuration.maritimeExpansionStartYear)
    return;

  const auto maritimeRange = maritimeRangeForYear(nextYear, configuration);
  for (const auto &[polityId, provinceIds] : territories) {
    const auto strength = state.polityStrengths.find(polityId);
    if (strength == state.polityStrengths.end())
      continue;
    const auto coastalProvinceCount = static_cast<size_t>(std::count_if(
        provinceIds.begin(), provinceIds.end(), [&](const auto provinceId) {
          return state.provinces.at(provinceId).coastal;
        }));
    const auto coastalProvinceShare =
        provinceIds.empty()
            ? 0.0
            : static_cast<double>(coastalProvinceCount) / provinceIds.size();
    const auto maritimePressure =
        coastalProvinceShare * configuration.islandPolityMaritimeMultiplier;
    const auto capacityMultiplier = capacityExpansionMultiplier(
        configuration, nextYear, provinceIds.size());
    if (!chance(configuration.maritimeExpansionChance * centuries *
                capacityMultiplier * 2.0 * maritimePressure))
      continue;

    ProvinceId sourceProvince = -1;
    ProvinceId targetProvince = -1;
    double targetDistance = std::numeric_limits<double>::max();
    double targetScore = -std::numeric_limits<double>::max();
    for (const auto sourceId : provinceIds) {
      if (!state.provinces.at(sourceId).coastal)
        continue;
      const auto routesFromSource = normalized.seaRoutesFrom.find(sourceId);
      if (routesFromSource == normalized.seaRoutesFrom.end())
        continue;
      for (const auto &route : routesFromSource->second) {
        if (route.distance > maritimeRange)
          continue;
        const auto candidateId = route.provinceId;
        const auto &candidate = state.provinces.at(candidateId);
        if (candidate.owner == polityId)
          continue;
        if (candidate.owner != NoPolity &&
            !remainsContiguousAfterConquest(candidate.owner, candidateId,
                                            normalized.neighbours,
                                            state.provinces))
          continue;
        const auto weakness = expansionTargetWeakness(polityId, candidate.owner,
                                                      state.polityStrengths);
        const auto candidateScore =
            weakness - route.distance / std::max(1.0, maritimeRange);
        if (candidateScore > targetScore) {
          sourceProvince = sourceId;
          targetProvince = candidateId;
          targetDistance = route.distance;
          targetScore = candidateScore;
        }
      }
    }
    if (sourceProvince < 0 || targetProvince < 0)
      continue;

    const auto distanceMultiplier =
        std::exp(-targetDistance / std::max(1.0, maritimeRange));
    const auto &target = state.provinces.at(targetProvince);
    const auto attacker = std::max(1.0, strength->second.score);
    const auto defender = state.polityStrengths.contains(target.owner)
                              ? state.polityStrengths.at(target.owner).score
                              : 0.0;
    if (target.owner != NoPolity) {
      if (chance(std::clamp(configuration.maritimeConquestMultiplier * 2.0 *
                                maritimePressure * centuries *
                                distanceMultiplier * attacker /
                                std::max(1.0, attacker + defender),
                            0.0, 1.0)))
        append({nextYear, EventType::TransferProvince, targetProvince,
                normalized.provinceRegions.at(targetProvince), polityId,
                target.owner, -1, NoReligion, 0.0, 0.0,
                "maritime island conquest"});
    } else if (nextYear >= configuration.renaissanceStartYear &&
               chance(configuration.maritimeColonizationMultiplier *
                      maritimePressure * centuries * distanceMultiplier)) {
      append({nextYear, EventType::ColonizeProvince, targetProvince,
              normalized.provinceRegions.at(targetProvince), polityId, NoPolity,
              -1, NoReligion, 1.0, 0.0, "maritime island colonization"});
    }
  }
}

void implodePolities(const Year nextYear, double centuries,
                     const Configuration &configuration,
                     const NormalizedInput &normalized, State &state,
                     const AppendEvent &append, PolityId &nextPolity) {
  std::map<PolityId, std::vector<ProvinceId>> territories;
  for (const auto &[provinceId, province] : state.provinces)
    if (province.owner != NoPolity)
      territories[province.owner].push_back(provinceId);

  const auto currentPolityCount = static_cast<double>(territories.size());
  const auto initialPolityCount =
      static_cast<double>(normalized.provinces.size());
  const double simulationProgress =
      std::clamp(static_cast<double>(nextYear - configuration.startYear) /
                     static_cast<double>(configuration.targetYear -
                                         configuration.startYear),
                 0.0, 1.0);
  const double desiredPolityCount =
      initialPolityCount +
      (static_cast<double>(configuration.targetEndPolityCount) -
       initialPolityCount) *
          simulationProgress;
  const double consolidationPressure =
      initialPolityCount == configuration.targetEndPolityCount
          ? 0.0
          : std::clamp(
                (currentPolityCount - desiredPolityCount) /
                    (initialPolityCount - configuration.targetEndPolityCount),
                0.0, 1.0);

  const bool aggressive = nextYear >= configuration.polityAggressionStartYear;
  const double expansionMultiplier =
      aggressive ? configuration.aggressiveExpansionMultiplier : 1.0;
  const double fragmentationMultiplier =
      aggressive ? configuration.aggressiveFragmentationMultiplier : 1.0;
  const double postStabilizationProgress =
      configuration.polityStabilizationStartYear >= configuration.targetYear
          ? 0.0
          : std::clamp(
                static_cast<double>(
                    nextYear - configuration.polityStabilizationStartYear) /
                    static_cast<double>(
                        configuration.targetYear -
                        configuration.polityStabilizationStartYear),
                0.0, 1.0);
  const double stabilizationMultiplier =
      1.0 - postStabilizationProgress *
                (1.0 - configuration.postStabilizationFragmentationMultiplier);
  const auto strongestPolity = strongestPolityOf(state);
  const auto strongestScore =
      strongestPolity == NoPolity ? 0.0
      : state.polityStrengths.contains(strongestPolity)
          ? state.polityStrengths.at(strongestPolity).score
          : 0.0;
  for (const auto &[polityId, provinceIds] : territories) {
    if (provinceIds.size() < 2 || !state.polities.contains(polityId) ||
        !state.polityStrengths.contains(polityId))
      continue;
    const auto polityScore = state.polityStrengths.at(polityId).score;
    const auto foundedYear = state.polities.at(polityId).foundedYear;
    const auto maturationProgress =
        configuration.successorMaturationYears <= 0
            ? 1.0
            : std::clamp(static_cast<double>(nextYear - foundedYear) /
                             static_cast<double>(
                                 configuration.successorMaturationYears),
                         0.0, 1.0);
    const auto maturationMultiplier =
        configuration.successorFragmentationMultiplier +
        maturationProgress *
            (1.0 - configuration.successorFragmentationMultiplier);
    const auto smallPolityProgress =
        configuration.smallPolityProtectionSize == 0 ||
                provinceIds.size() <= configuration.smallPolityProtectionSize
            ? 0.0
            : std::clamp(
                  static_cast<double>(provinceIds.size() -
                                      configuration.smallPolityProtectionSize) /
                      static_cast<double>(
                          configuration.smallPolityProtectionSize),
                  0.0, 1.0);
    const auto smallPolityMultiplier =
        configuration.smallPolityFragmentationMultiplier +
        smallPolityProgress *
            (1.0 - configuration.smallPolityFragmentationMultiplier);
    const bool decaying =
        strongestScore > 0.0 &&
        polityScore <= strongestScore * configuration.decayStrengthRatio;
    const bool decaySplit =
        decaying && chance(configuration.decayFragmentationChance * centuries *
                           stabilizationMultiplier * maturationMultiplier *
                           smallPolityMultiplier);
    const bool ordinarySplit = chance(
        configuration.splitOffChance * fragmentationMultiplier * centuries *
        std::max(0.25, 1.0 - consolidationPressure) * stabilizationMultiplier *
        maturationMultiplier * smallPolityMultiplier);
    if (!decaySplit && !ordinarySplit)
      continue;
    if (decaySplit) {
      const auto configuredMaximum = static_cast<size_t>(
          std::clamp(configuration.maximumImplosionSuccessors, 1, 10));
      const auto desiredSuccessorCount = provinceIds.size() >= 8 ? 4u : 3u;
      const auto successorCount =
          std::min(provinceIds.size(),
                   std::min(configuredMaximum,
                            static_cast<size_t>(desiredSuccessorCount)));
      std::vector<PolityId> successors;
      successors.reserve(successorCount);
      for (size_t index = 0; index < successorCount; ++index) {
        const auto successorId = nextPolity++;
        successors.push_back(successorId);
        append({nextYear, EventType::CreateSuccessorPolity, -1, -1, successorId,
                NoPolity, -1, NoReligion, 0.0, 0.0, "successor state", polityId,
                randomPolityColour()});
      }
      const auto assignments = contiguousSuccessorAssignments(
          provinceIds, successors.size(), normalized.neighbours);
      for (size_t index = 0; index < provinceIds.size(); ++index) {
        const auto successorId =
            successors[static_cast<size_t>(assignments[index])];
        append({nextYear, EventType::TransferProvince, provinceIds[index],
                normalized.provinceRegions.at(provinceIds[index]), successorId,
                polityId, -1, NoReligion, 0.0, 0.0,
                "successor state partition"});
      }
      append({nextYear, EventType::DissolvePolity, -1, -1, polityId, NoPolity,
              -1, NoReligion, 0.0, 0.0, "polity implosion"});
      continue;
    }
    const auto splitPolity = nextPolity++;
    append({nextYear, EventType::CreatePolity, -1, -1, splitPolity, NoPolity,
            -1, NoReligion, 0.0, 0.0, "tribe", -1, randomPolityColour()});
    const auto splitProvince = provinceIds.back();
    append({nextYear, EventType::TransferProvince, splitProvince,
            normalized.provinceRegions.at(splitProvince), splitPolity, polityId,
            -1, NoReligion, 0.0, 0.0, "fragmentation"});
  }
}

void evolveCultureAndReligion(const Year nextYear, double centuries,
                              const Configuration &configuration,
                              const NormalizedInput &normalized, State &state,
                              const AppendEvent &append, CultureId &nextCulture,
                              ReligionId &nextReligion) {
  if (nextYear < configuration.classicalStartYear)
    return;
  for (const auto &[provinceId, province] : state.provinces) {
    if (province.religion == NoReligion &&
        chance(configuration.religionEmergenceChance * centuries)) {
      const auto religionId = nextReligion++;
      append({nextYear, EventType::CreateReligion, provinceId, -1, NoPolity,
              NoPolity, -1, religionId, 0.0, 0.0, "religion emergence", -1,
              randomPolityColour()});
      append({nextYear, EventType::SetReligion, provinceId, -1, NoPolity,
              NoPolity, -1, religionId, 0.0, 0.0, "religion adoption"});
    }
    if (province.religion != NoReligion &&
        chance(configuration.religionSplitChance * centuries)) {
      const auto religionId = nextReligion++;
      append({nextYear, EventType::CreateReligion, provinceId, -1, NoPolity,
              NoPolity, -1, religionId, 0.0, 0.0, "religion split",
              province.religion, randomPolityColour()});
      append({nextYear, EventType::SetReligion, provinceId, -1, NoPolity,
              NoPolity, -1, religionId, 0.0, 0.0, "religion schism"});
    }
    for (const auto neighbourId : normalized.neighbours.at(provinceId)) {
      const auto &neighbour = state.provinces.at(neighbourId);
      if (province.religion == NoReligion && neighbour.religion != NoReligion &&
          chance(configuration.religionSpreadChance * centuries)) {
        append({nextYear, EventType::SetReligion, provinceId, -1, NoPolity,
                NoPolity, -1, neighbour.religion, 0.0, 0.0, "religion spread"});
        break;
      }
      if (province.religion != NoReligion && neighbour.religion != NoReligion &&
          province.religion != neighbour.religion &&
          chance(configuration.religionConversionChance * centuries)) {
        append({nextYear, EventType::SetReligion, provinceId, -1, NoPolity,
                NoPolity, -1, neighbour.religion, 0.0, 0.0,
                "religious consolidation"});
        break;
      }
      if (province.culture == neighbour.culture &&
          chance(configuration.cultureSplitChance * centuries)) {
        const auto cultureId = nextCulture++;
        append({nextYear, EventType::CreateCulture, provinceId, -1, NoPolity,
                NoPolity, cultureId, NoReligion, 0.0, 0.0, "culture split",
                province.culture, randomPolityColour()});
        append({nextYear, EventType::SetCulture, provinceId, -1, NoPolity,
                NoPolity, cultureId, NoReligion, 0.0, 0.0,
                "cultural differentiation"});
        break;
      }
    }

    const auto polity = state.polities.find(province.owner);
    if (polity == state.polities.end() || polity->second.primaryCulture < 0)
      continue;
    const auto primaryCulture = polity->second.primaryCulture;
    bool primaryCultureNeighbour = false;
    for (const auto neighbourId : normalized.neighbours.at(provinceId)) {
      if (state.provinces.at(neighbourId).culture == primaryCulture) {
        primaryCultureNeighbour = true;
        break;
      }
    }

    bool overseasAdoption = false;
    if (!primaryCultureNeighbour && state.provinces.at(provinceId).island &&
        nextYear >= configuration.maritimeExpansionStartYear) {
      double closestSeaDistance = std::numeric_limits<double>::max();
      const auto routesToProvince = normalized.seaRoutesTo.find(provinceId);
      if (routesToProvince != normalized.seaRoutesTo.end())
        for (const auto &route : routesToProvince->second) {
          if (route.distance >= closestSeaDistance)
            continue;
          const auto source = state.provinces.find(route.provinceId);
          if (source != state.provinces.end() &&
              source->second.owner == province.owner && source->second.coastal)
            closestSeaDistance = route.distance;
        }
      const auto maritimeRange = maritimeRangeForYear(nextYear, configuration);
      if (closestSeaDistance <= maritimeRange &&
          chance(configuration.overseasCultureAdoptionChance * centuries *
                 std::exp(-closestSeaDistance / maritimeRange)))
        overseasAdoption = true;
    }

    if ((primaryCultureNeighbour || overseasAdoption) &&
        province.culture != primaryCulture &&
        chance(configuration.cultureAssimilationChance * centuries)) {
      append({nextYear, EventType::SetCulture, provinceId, -1, NoPolity,
              NoPolity, primaryCulture, NoReligion, 0.0, 0.0,
              overseasAdoption ? "overseas primary culture adoption"
                               : "polity primary culture expansion"});
    } else if (province.culture != primaryCulture && !primaryCultureNeighbour &&
               chance(configuration.primaryCultureChangeChance * centuries)) {
      for (const auto neighbourId : normalized.neighbours.at(provinceId)) {
        const auto neighbourCulture = state.provinces.at(neighbourId).culture;
        if (neighbourCulture != primaryCulture && neighbourCulture >= 0) {
          append({nextYear, EventType::SetPrimaryCulture, -1, -1,
                  province.owner, NoPolity, neighbourCulture, NoReligion, 0.0,
                  0.0, "rare polity primary culture change"});
          break;
        }
      }
    }
  }
}

void consolidateRegions(const Year nextYear, const Configuration &configuration,
                        const NormalizedInput &normalized, const State &state,
                        const AppendEvent &append) {
  for (const auto &[regionId, provinceIds] : normalized.regions) {
    std::map<PolityId, int> ownership;
    for (const auto provinceId : provinceIds)
      ++ownership[state.provinces.at(provinceId).owner];
    const auto dominant =
        std::max_element(ownership.begin(), ownership.end(),
                         [](const auto &left, const auto &right) {
                           return left.second != right.second
                                      ? left.second < right.second
                                      : left.first > right.first;
                         })
            ->first;
    for (const auto provinceId : provinceIds) {
      const auto previous = state.provinces.at(provinceId).owner;
      if (previous != dominant)
        append({nextYear, EventType::ConsolidateRegion, provinceId, regionId,
                dominant, previous, -1, NoReligion, 0.0, 0.0,
                "regional consolidation"});
    }
  }
}

void dissolveEmptyPolities(const Year nextYear, State &state,
                           const AppendEvent &append) {
  const auto territories = territoriesByPolity(state);
  for (const auto &[polityId, polity] : state.polities) {
    if (!polity.dissolvedYear && !territories.contains(polityId))
      append({nextYear, EventType::DissolvePolity, -1, -1, polityId, NoPolity,
              -1, NoReligion, 0.0, 0.0, "annexed or dissolved"});
  }
}

} // namespace

HistorySimulation::HistorySimulation(Configuration configuration)
    : configuration(std::move(configuration)) {}

Result HistorySimulation::runSimulation(const Input &input) {
  Result result;
  if (configuration.targetYear <= configuration.startYear) {
    result.errors.push_back({configuration.startYear,
                             "The target year must be after the start year",
                             {},
                             {}});
    return result;
  }
  auto normalized = normalize(input);
  result.errors = normalized.errors;
  if (!result.errors.empty())
    return result;

  provinceRegions = normalized.provinceRegions;
  provinceNeighbours = normalized.neighbours;
  if (normalized.provinces.empty()) {
    result.errors.push_back({configuration.startYear,
                             "No eligible land provinces were supplied",
                             {},
                             {}});
    return result;
  }
  if (configuration.targetEndPolityCount < 1 ||
      configuration.targetEndPolityCount >
          static_cast<int>(normalized.provinces.size())) {
    result.errors.push_back({configuration.startYear,
                             "The target end polity count must be between one "
                             "and the number of eligible land provinces",
                             {},
                             {}});
    return result;
  }

  State state;
  state.year = configuration.startYear;
  PolityId nextPolity = 0;
  CultureId nextCulture = 0;
  ReligionId nextReligion = 0;
  int nextWarId = 0;
  std::map<ProvinceId, double> growthPotential;
  std::map<ProvinceId, double> baseCapacity;
  auto append = [&](Event &&event) {
    result.events.emplace_back(std::move(event));
    if (result.events.back().type == EventType::InitializeProvince) {
      const auto provinceId = result.events.back().provinceId;
      result.events.back().coastal =
          normalized.environments.at(provinceId).coastal;
      result.events.back().island =
          normalized.environments.at(provinceId).island;
    }
    ++result.eventCounts[result.events.back().type];
    const auto &appliedEvent = result.events.back();
    events::apply(state, appliedEvent);
    if (appliedEvent.type == EventType::InitializeProvince ||
        appliedEvent.type == EventType::TransferProvince ||
        appliedEvent.type == EventType::ColonizeProvince ||
        appliedEvent.type == EventType::ConsolidateRegion) {
      if (appliedEvent.previousPolityId != NoPolity)
        result.polityHistory.currentProvinceIds[appliedEvent.previousPolityId]
            .erase(appliedEvent.provinceId);
      if (appliedEvent.polityId != NoPolity) {
        auto &current =
            result.polityHistory.currentProvinceIds[appliedEvent.polityId];
        current.insert(appliedEvent.provinceId);
        auto &peak =
            result.polityHistory.peakProvinceIds[appliedEvent.polityId];
        if (current.size() > peak.size())
          peak.assign(current.begin(), current.end());
        if (appliedEvent.regionId >= 0)
          result.polityHistory.historicalRegionOwners[appliedEvent.regionId]
              .insert(appliedEvent.polityId);
      }
    }
  };
  const auto totalRegions = std::max<size_t>(1, normalized.regions.size());
  SuperRegionId nextSuperRegion = 0;
  std::map<int, std::vector<RegionId>> continentRegions;
  for (const auto &[regionId, continentId] : normalized.regionContinents)
    if (normalized.regions.contains(regionId))
      continentRegions[continentId].push_back(regionId);
  for (auto &[continentId, regionIds] : continentRegions) {
    std::set<RegionId> unassigned(regionIds.begin(), regionIds.end());
    const auto groupCount = std::max<size_t>(
        1, std::min(regionIds.size(),
                    static_cast<size_t>(std::round(
                        static_cast<double>(
                            configuration.targetDevelopmentSuperRegionCount) *
                        regionIds.size() / totalRegions))));
    const auto groupSize =
        std::max<size_t>(1, (regionIds.size() + groupCount - 1) / groupCount);
    while (!unassigned.empty()) {
      const auto superRegionId = nextSuperRegion++;
      append({configuration.startYear, EventType::CreateSuperRegion, -1,
              superRegionId, continentId, NoPolity, -1, NoReligion, 0.0, 0.0,
              "fixed development superregion"});
      std::vector<RegionId> frontier{*unassigned.begin()};
      size_t assigned = 0;
      while (!frontier.empty() && assigned < groupSize) {
        const auto regionId = frontier.back();
        frontier.pop_back();
        if (!unassigned.erase(regionId))
          continue;
        append({configuration.startYear, EventType::SetSuperRegion, -1,
                regionId, NoPolity, NoPolity, -1, NoReligion,
                static_cast<double>(superRegionId), 0.0,
                "fixed superregion assignment"});
        state.superRegions[superRegionId].regions.push_back(regionId);
        ++assigned;
        for (const auto neighbourId : normalized.regionNeighbours[regionId])
          if (unassigned.contains(neighbourId))
            frontier.push_back(neighbourId);
      }
    }
  }
  for (const auto &[regionId, neighbours] : normalized.regionNeighbours) {
    const auto source = state.regionSuperRegions.find(regionId);
    if (source == state.regionSuperRegions.end())
      continue;
    for (const auto neighbourId : neighbours) {
      const auto target = state.regionSuperRegions.find(neighbourId);
      if (target != state.regionSuperRegions.end() &&
          target->second != source->second)
        state.superRegions[source->second].neighbours.push_back(target->second);
    }
  }
  for (auto &[id, superRegion] : state.superRegions) {
    std::sort(superRegion.neighbours.begin(), superRegion.neighbours.end());
    superRegion.neighbours.erase(std::unique(superRegion.neighbours.begin(),
                                             superRegion.neighbours.end()),
                                 superRegion.neighbours.end());
  }

  for (const auto &[provinceId, province] : normalized.provinces) {
    const auto polityId = nextPolity++;
    const auto cultureId = nextCulture++;
    const auto habitability =
        province->habitability > 0.0f
            ? std::clamp(static_cast<double>(province->habitability), 0.0, 1.0)
            : 0.5;
    const auto connectivity =
        std::min(normalized.neighbours.at(provinceId).size(), size_t{6});
    const auto potential = 0.65 + habitability * 0.45 +
                           static_cast<double>(connectivity) * 0.04 +
                           RandNum::getRandom<double>(0.25);
    growthPotential.emplace(provinceId, potential);
    const auto &environment = normalized.environments.at(provinceId);
    const double environmentalCapacity =
        std::max(configuration.minimumPopulation,
                 configuration.defaultPopulation *
                     (0.5 + environment.habitability + environment.arableLand +
                      (environment.coastal ? 0.25 : 0.0)) *
                     std::sqrt(std::max(
                         1.0, static_cast<double>(environment.area) / 100.0)) /
                     (1.0 + environment.inclination));
    baseCapacity.emplace(provinceId, environmentalCapacity);
    const double population = 1.0;
    const double development = (province->averageDevelopment > 0.0
                                    ? province->averageDevelopment
                                    : configuration.defaultDevelopment) *
                               potential;
    append({configuration.startYear, EventType::CreatePolity, -1, -1, polityId,
            NoPolity, -1, NoReligion, 0.0, 0.0, "tribe", -1,
            randomPolityColour()});
    auto initializeProvince = Event{configuration.startYear,
                                    EventType::InitializeProvince,
                                    provinceId,
                                    normalized.provinceRegions.at(provinceId),
                                    polityId,
                                    NoPolity,
                                    cultureId,
                                    NoReligion,
                                    population,
                                    development,
                                    "initial tribe and culture",
                                    -1,
                                    randomPolityColour()};
    initializeProvince.coastal = environment.coastal;
    initializeProvince.island = environment.island;
    append(std::move(initializeProvince));
    append({configuration.startYear, EventType::UpdateCarryingCapacity,
            provinceId, normalized.provinceRegions.at(provinceId), NoPolity,
            NoPolity, -1, NoReligion, environmentalCapacity, 0.0,
            "initial carrying capacity"});
  }
  for (const auto &[regionId, provinces] : normalized.regions)
    append({configuration.startYear, EventType::SetRegionalPhase, -1, regionId,
            NoPolity, NoPolity, -1, NoReligion,
            static_cast<double>(static_cast<int>(RegionalPhase::Neutral)), 0.0,
            "initial regional phase"});

  for (Year year = configuration.startYear; year < configuration.targetYear;) {
    const Year nextYear =
        std::min(year + stepFor(configuration, year), configuration.targetYear);
    logYearProgress(year, configuration.targetYear, result.eventCounts);
    const double centuries = static_cast<double>(nextYear - year) / 100.0;
    if (!(configuration.superRegionCycleYears <= 0 ||
          (nextYear - configuration.startYear) /
                  configuration.superRegionCycleYears <=
              (year - configuration.startYear) /
                  configuration.superRegionCycleYears)) {
      development::updateSuperRegionPhases(year, nextYear, configuration, state,
                                           append);
    }
    if (!(configuration.regionPhaseDurationYears <= 0 ||
          (nextYear - configuration.startYear) /
                  configuration.regionPhaseDurationYears <=
              (year - configuration.startYear) /
                  configuration.regionPhaseDurationYears)) {
      development::updateRegionalPhases(year, nextYear, configuration,
                                        normalized, append);
    }
    auto territories = territoriesByPolity(state);
    development::updateProvinceGrowth(nextYear, centuries, configuration,
                                      normalized, state, growthPotential,
                                      baseCapacity, append, territories);
    development::updatePolityStrengths(nextYear, state, append);
    integrateCultures(nextYear, centuries, configuration, state, append);
    /*colonize(nextYear, centuries, configuration, normalized, state, append,
             strongestPolityOf(state));*/
    warfare::resolveWars(nextYear, centuries, configuration, normalized, state,
                         append, result.wars, nextWarId, result.events,
                         normalized.provinces);
    // implodePolities(nextYear, centuries, configuration, normalized, state,
    // append,
    //                 nextPolity);
    evolveCultureAndReligion(nextYear, centuries, configuration, normalized,
                             state, append, nextCulture, nextReligion);
    if (nextYear >= configuration.regionOwnershipYear) {
      consolidateRegions(nextYear, configuration, normalized, state, append);
    }
    dissolveEmptyPolities(nextYear, state, append);
    year = nextYear;
  }

  result.finalState = state;
  result.errors = validate(state, configuration.targetYear, true);
  State eventState;
  Year previousYear = configuration.startYear;
  for (const auto &event : result.events) {
    if (event.year < configuration.startYear ||
        event.year > configuration.targetYear)
      result.errors.push_back(
          {event.year, "Event year is outside the configured simulation range",
           event.provinceId >= 0 ? std::optional<ProvinceId>(event.provinceId)
                                 : std::nullopt,
           event.regionId >= 0 ? std::optional<RegionId>(event.regionId)
                               : std::nullopt});
    if (event.year < previousYear)
      result.errors.push_back(
          {event.year, "Events are not ordered chronologically",
           event.provinceId >= 0 ? std::optional<ProvinceId>(event.provinceId)
                                 : std::nullopt,
           event.regionId >= 0 ? std::optional<RegionId>(event.regionId)
                               : std::nullopt});
    previousYear = event.year;
    events::apply(eventState, event);
  }
  return result;
}

State HistorySimulation::reconstruct(const std::vector<Event> &events,
                                     Year year) const {
  State state;
  for (const auto &event : events) {
    if (event.year > year)
      break;
    events::apply(state, event);
    state.year = event.year;
  }
  return state;
}

std::vector<ValidationError>
HistorySimulation::validate(const State &state, Year year,
                            bool requireWholeRegions) const {
  return validation::validateState(state, configuration, provinceRegions, year,
                                   requireWholeRegions);
}

std::optional<ArtifactPaths>
HistorySimulation::writeArtifacts(const Input &input, const Result &result,
                                  const Fwg::Gfx::Image &baseMap,
                                  const std::filesystem::path &outputDirectory,
                                  std::vector<ValidationError> &errors) const {
  // if (!result.succeeded()) {
  // errors.push_back({configuration.targetYear, "Cannot export artifacts from a
  // failed simulation", {}, {}}); return std::nullopt;
  // }
  if (!baseMap.initialised()) {
    errors.push_back(
        {configuration.startYear,
         "Cannot export ownership maps without an initialized world map",
         {},
         {}});
    return std::nullopt;
  }

  const auto normalized = normalize(input);
  if (!normalized.errors.empty()) {
    errors.insert(errors.end(), normalized.errors.begin(),
                  normalized.errors.end());
    return std::nullopt;
  }

  try {
    std::filesystem::create_directories(outputDirectory);
    const auto developmentDirectory = outputDirectory / "development";
    const auto populationDirectory = outputDirectory / "population";
    const auto cultureDirectory = outputDirectory / "culture";
    const auto religionDirectory = outputDirectory / "religion";
    std::filesystem::create_directories(developmentDirectory);
    std::filesystem::create_directories(populationDirectory);
    std::filesystem::create_directories(cultureDirectory);
    std::filesystem::create_directories(religionDirectory);
    ArtifactPaths paths;
    paths.eventLog = outputDirectory / "events.txt";
    std::ofstream eventLog(paths.eventLog);
    if (!eventLog) {
      errors.push_back({configuration.startYear,
                        "Unable to open simulation event log",
                        {},
                        {}});
      return std::nullopt;
    }
    // Keep TSV schema stable: successor lineage is encoded via "type" and
    // "parent" without adding new columns.
    eventLog << "year\ttype\tprovince\tregion\tpolity\tprevious_"
                "polity\tculture\treligion\tparent\tvalue\tsecondary_"
                "value\tscore\tred\tgreen\tblue\tdescription\n";
    for (const auto &event : result.events) {
      eventLog << event.year << '\t' << eventTypeName(event.type) << '\t'
               << event.provinceId << '\t' << event.regionId << '\t'
               << event.polityId << '\t' << event.previousPolityId << '\t'
               << event.cultureId << '\t' << event.religionId << '\t'
               << event.parentId << '\t' << event.value << '\t'
               << event.secondaryValue << '\t' << event.score << '\t'
               << static_cast<int>(event.colour.getRed()) << '\t'
               << static_cast<int>(event.colour.getGreen()) << '\t'
               << static_cast<int>(event.colour.getBlue()) << '\t'
               << event.description << '\n';
    }
    paths.developmentLog = developmentDirectory / "development.tsv";
    paths.populationLog = populationDirectory / "population.tsv";
    paths.cultureLog = cultureDirectory / "culture.tsv";
    paths.religionLog = religionDirectory / "religion.tsv";
    paths.superRegionLog = outputDirectory / "superregions.tsv";
    std::ofstream developmentLog(paths.developmentLog);
    std::ofstream populationLog(paths.populationLog);
    std::ofstream cultureLog(paths.cultureLog);
    std::ofstream religionLog(paths.religionLog);
    std::ofstream superRegionLog(paths.superRegionLog);
    if (!developmentLog || !populationLog || !cultureLog || !religionLog ||
        !superRegionLog) {
      errors.push_back({configuration.startYear,
                        "Unable to open one or more category simulation logs",
                        {},
                        {}});
      return std::nullopt;
    }
    developmentLog << "year\tprovince\tdevelopment\n";
    populationLog
        << "year\tprovince\tpopulation\tcarrying_capacity\tregion_phase\n";
    cultureLog
        << "year\tprovince\tculture\tculture_population\tdominant\torigin_"
           "province\tfounded_year\tparent_culture\n";
    religionLog << "year\tprovince\treligion\torigin_province\tfounded_"
                   "year\tparent_religion\n";
    superRegionLog << "year\tsuperregion\tcontinent\tphase\tdevelopment\n";

    std::vector<Year> years;
    for (Year year = configuration.startYear; year <= configuration.targetYear;
         year += 100)
      years.push_back(year);
    if (years.empty() || years.back() != configuration.targetYear)
      years.push_back(configuration.targetYear);

    for (size_t index = 0; index < years.size(); ++index) {
      const auto year = years[index];
      const auto state = reconstruct(result.events, year);
      for (const auto &[provinceId, provinceState] : state.provinces) {
        // developmentLog << year << '\t' << provinceId << '\t'
        //                << provinceState.development << '\n';
        // const auto regionId = normalized.provinceRegions.at(provinceId);
        // populationLog << year << '\t' << provinceId << '\t'
        //               << provinceState.population << '\t'
        //               << provinceState.carryingCapacity << '\t'
        //               << static_cast<int>(
        //                      state.regionalPhases.contains(regionId)
        //                          ? state.regionalPhases.at(regionId)
        //                          : RegionalPhase::Neutral)
        //               << '\n';
        // for (const auto &[cultureId, culturePopulation] :
        //      provinceState.culturePopulations) {
        //   const auto &culture = state.cultures.at(cultureId);
        //   cultureLog << year << '\t' << provinceId << '\t' << cultureId <<
        //   '\t'
        //              << culturePopulation << '\t'
        //              << (cultureId == state.dominantCultureOf(provinceId) ? 1
        //                                                                   :
        //                                                                   0)
        //              << '\t' << culture.originProvinceId << '\t'
        //              << culture.foundedYear << '\t'
        //              << culture.parentId.value_or(-1) << '\n';
        // }
        // if (provinceState.religion != NoReligion) {
        //   const auto &religion = state.religions.at(provinceState.religion);
        //   religionLog << year << '\t' << provinceId << '\t'
        //               << provinceState.religion << '\t'
        //               << religion.originProvinceId << '\t'
        //               << religion.foundedYear << '\t'
        //               << religion.parentId.value_or(-1) << '\n';
        // } else {
        //   religionLog << year << '\t' << provinceId << "\t-1\t-1\t-1\t-1\n";
        // }
        // for (const auto &[superRegionId, superRegion] : state.superRegions)
        //   superRegionLog << year << '\t' << superRegionId << '\t'
        //                  << superRegion.continentId << '\t'
        //                  << static_cast<int>(superRegion.phase) << '\t'
        //                  << superRegion.development << '\n';
      }
      auto frame = baseMap;
      auto developmentFrame = baseMap;
      auto populationFrame = baseMap;
      auto cultureFrame = baseMap;
      auto religionFrame = baseMap;
      double maximumDevelopment = 0.0;
      double maximumPopulation = 0.0;
      for (const auto &[provinceId, provinceState] : state.provinces) {
        maximumDevelopment =
            std::max(maximumDevelopment, provinceState.development);
        maximumPopulation =
            std::max(maximumPopulation, provinceState.population);
      }
      for (const auto &[provinceId, province] : normalized.provinces) {
        const auto provinceState = state.findProvince(provinceId);
        if (!provinceState)
          continue;
        const auto colour = state.polities.at(provinceState->owner).colour;
        const auto development =
            maximumDevelopment > 0.0
                ? static_cast<unsigned char>(
                      255.0 * provinceState->development / maximumDevelopment)
                : static_cast<unsigned char>(0);
        const auto population =
            maximumPopulation > 0.0
                ? static_cast<unsigned char>(
                      255.0 * std::log1p(provinceState->population) /
                      std::log1p(maximumPopulation))
                : static_cast<unsigned char>(0);
        const auto cultureColour =
            state.cultures.at(provinceState->culture).colour;
        const auto religionColour =
            provinceState->religion == NoReligion
                ? Fwg::Gfx::Colour{0, 0, 0}
                : state.religions.at(provinceState->religion).colour;
        for (const auto pixel : province->pixels) {
          if (pixel >= 0 && static_cast<std::size_t>(pixel) < frame.size())
            frame.setColourAtIndex(pixel, colour);
          developmentFrame.setColourAtIndex(
              pixel, {development, development, development});
          populationFrame.setColourAtIndex(
              pixel, {population, population, population});
          cultureFrame.setColourAtIndex(pixel, cultureColour);
          religionFrame.setColourAtIndex(pixel, religionColour);
        }
      }
      std::ostringstream filename;
      filename << "ownership_" << std::setw(3) << std::setfill('0') << index
               << '_' << year << ".png";
      const auto mapPath = outputDirectory / filename.str();
      Fwg::Gfx::Png::save(frame, mapPath.string(), true);
      paths.ownershipMaps.push_back(mapPath);
      const auto developmentPath = developmentDirectory / filename.str();
      const auto populationPath = populationDirectory / filename.str();
      const auto culturePath = cultureDirectory / filename.str();
      const auto religionPath = religionDirectory / filename.str();
      Fwg::Gfx::Png::save(developmentFrame, developmentPath.string(), true);
      Fwg::Gfx::Png::save(populationFrame, populationPath.string(), true);
      // Fwg::Gfx::Png::save(cultureFrame, culturePath.string(), true);
      // Fwg::Gfx::Png::save(religionFrame, religionPath.string(), true);
      paths.developmentMaps.push_back(developmentPath);
      paths.populationMaps.push_back(populationPath);
      paths.cultureMaps.push_back(culturePath);
      paths.religionMaps.push_back(religionPath);
    }
    return paths;
  } catch (const std::exception &exception) {
    errors.push_back({configuration.startYear,
                      std::string("Unable to export simulation artifacts: ") +
                          exception.what(),
                      {},
                      {}});
    return std::nullopt;
  }
}

} // namespace Arda::Simulation