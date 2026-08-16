#include "simulation/Simulation.h"

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

struct NormalizedInput {
  struct Environment {
    double habitability = 0.5;
    double arableLand = 0.5;
    double inclination = 0.0;
    bool coastal = false;
    bool island = false;
    size_t area = 0;
  };

  std::map<ProvinceId, std::shared_ptr<ArdaProvince>> provinces;
  std::map<ProvinceId, RegionId> provinceRegions;
  std::map<RegionId, std::vector<ProvinceId>> regions;
  std::map<ProvinceId, std::vector<ProvinceId>> neighbours;
  std::map<ProvinceId, int> provinceContinents;
  std::set<RegionId> islandRegions;
  SeaRouteMap seaRoutesFrom;
  SeaRouteMap seaRoutesTo;
  std::map<RegionId, int> regionContinents;
  std::map<RegionId, std::vector<RegionId>> regionNeighbours;
  std::map<ProvinceId, Environment> environments;
  std::vector<ValidationError> errors;
};

SeaRouteMap buildWeightedSeaRoutes(
    const std::vector<std::shared_ptr<ArdaContinent>> &continents,
    const Fwg::Terrain::TerrainData *terrainData) {
  std::map<ProvinceId, std::shared_ptr<ArdaProvince>> provinces;
  for (const auto &continent : continents)
    if (continent)
      for (const auto &province : continent->ardaProvinces)
        if (province)
          provinces.emplace(province->ID, province);
  std::map<ProvinceId, double> depths;
  const auto seaLevel = static_cast<double>(Fwg::Cfg::Values().seaLevel);
  for (const auto &[provinceId, province] : provinces) {
    double depth = 0.0;
    size_t samples = 0;
    if (terrainData) {
      for (const auto pixel : province->pixels) {
        if (pixel < 0 ||
            pixel >= static_cast<int>(terrainData->detailedHeightMap.size()))
          continue;
        depth += std::max(
            0.0, seaLevel -
                      static_cast<double>(terrainData->detailedHeightMap[pixel]));
        ++samples;
      }
    }
    depths[provinceId] = samples == 0 ? 0.0 : depth / samples;
  }

  SeaRouteMap routes;
  for (const auto &[sourceId, source] : provinces) {
    if (!source->isCoastalToOcean())
      continue;
    std::priority_queue<std::pair<double, ProvinceId>,
                        std::vector<std::pair<double, ProvinceId>>,
                        std::greater<>> queue;
    std::map<ProvinceId, double> distances;
    distances[sourceId] = 0.0;
    queue.push({0.0, sourceId});
    while (!queue.empty()) {
      const auto [distance, provinceId] = queue.top();
      queue.pop();
      if (distance > distances.at(provinceId))
        continue;
      const auto currentIt = provinces.find(provinceId);
      if (currentIt == provinces.end())
        continue;
      const auto &current = currentIt->second;
      for (const auto &neighbour : current->provinceNeighbours) {
        if (!neighbour || neighbour->isLake() ||
            (!neighbour->isSea() && !neighbour->isCoastalToOcean()))
          continue;
        const auto neighbourId = neighbour->ID;
        const auto neighbourIt = provinces.find(neighbourId);
        if (neighbourIt == provinces.end())
          continue;
        const auto dx = static_cast<double>(
            current->position.widthCenter - neighbour->position.widthCenter);
        const auto dy = static_cast<double>(
            current->position.heightCenter - neighbour->position.heightCenter);
        const auto transitionDistance = std::sqrt(dx * dx + dy * dy);
        const auto depthIt = depths.find(neighbourId);
        const auto normalizedDepth =
            seaLevel > 0.0 && depthIt != depths.end()
                ? depthIt->second / seaLevel
                : 0.0;
        const auto cost = transitionDistance *
                          (neighbour->isSea()
                               ? 1.0 + normalizedDepth
                               : 1.0);
        const auto candidateDistance = distance + cost;
        if (const auto existing = distances.find(neighbourId);
            existing != distances.end() && existing->second <= candidateDistance)
          continue;
        distances[neighbourId] = candidateDistance;
        queue.push({candidateDistance, neighbourId});
      }
    }
    for (const auto &[targetId, distance] : distances) {
      const auto targetIt = provinces.find(targetId);
      if (targetIt == provinces.end())
        continue;
      const auto &target = targetIt->second;
      if (targetId != sourceId && target->isCoastalToOcean() && !target->isSea())
        routes[sourceId].push_back({targetId, distance});
    }
    std::sort(routes[sourceId].begin(), routes[sourceId].end(),
              [](const auto &left, const auto &right) {
                if (left.distance != right.distance)
                  return left.distance < right.distance;
                return left.provinceId < right.provinceId;
              });
  }
  return routes;
}

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

bool isEligible(const std::shared_ptr<ArdaProvince> &province) {
  return province && !province->isSea() && !province->isLake() &&
         !province->topographyTypes.contains(
             Civilization::TopographyType::WASTELAND);
}

void refreshDominantCulture(ProvinceState &province) {
  if (province.culturePopulations.empty()) {
    province.culture = -1;
    return;
  }
  province.culture = std::max_element(province.culturePopulations.begin(),
                                      province.culturePopulations.end(),
                                      [](const auto &left, const auto &right) {
                                        return left.second < right.second;
                                      })
                         ->first;
}

double capacityEraMultiplier(const Configuration &configuration, Year year) {
  const auto compound = [](double multiplier, double rate, Year years) {
    return multiplier *
           std::pow(1.0 + rate,
                    static_cast<double>(std::max(0, years)) / 100.0);
  };
  double multiplier =
      compound(1.0, configuration.ancientCapacityGrowthPerCentury,
               std::min(year, configuration.renaissanceStartYear) -
                   configuration.startYear);
  if (year > configuration.renaissanceStartYear) {
    multiplier =
        compound(multiplier, configuration.renaissanceCapacityGrowthPerCentury,
                 std::min(year, configuration.industrialRevolutionStartYear) -
                     configuration.renaissanceStartYear);
  }
  if (year > configuration.industrialRevolutionStartYear)
    multiplier =
        compound(multiplier, configuration.industrialCapacityGrowthPerCentury,
                 year - configuration.industrialRevolutionStartYear);
  return multiplier;
}

double phaseMultiplier(RegionalPhase phase) {
  switch (phase) {
  case RegionalPhase::Boom:
    return 1.20;
  case RegionalPhase::Bust:
    return 0.80;
  case RegionalPhase::Neutral:
    return 1.0;
  }
  return 1.0;
}

NormalizedInput normalize(const Input &input) {
  NormalizedInput normalized;
  std::map<ProvinceId, RegionId> allProvinceRegions;

  for (const auto &continent : input.continents) {
    if (!continent) {
      normalized.errors.push_back(
          {StartYear, "Input contains a null continent", {}, {}});
      continue;
    }
    for (const auto &ardaRegion : continent->ardaRegions) {
      if (!ardaRegion) {
        normalized.errors.push_back(
            {StartYear, "Input contains a null region", {}, {}});
        continue;
      }
      normalized.regionContinents[ardaRegion->ID] = continent->ID;
      if (ardaRegion->areaSubType == Fwg::Areas::AreaSubType::Island ||
          ardaRegion->areaSubType == Fwg::Areas::AreaSubType::CoastalIsland ||
          ardaRegion->areaSubType == Fwg::Areas::AreaSubType::LakeIsland)
        normalized.islandRegions.insert(ardaRegion->ID);
      for (const auto &neighbour : ardaRegion->neighbourRegions) {
        if (neighbour)
          normalized.regionNeighbours[ardaRegion->ID].push_back(neighbour->ID);
      }
      for (const auto &province : ardaRegion->ardaProvinces) {
        if (!province) {
          normalized.errors.push_back({StartYear,
                                       "Region contains a null province",
                                       {},
                                       ardaRegion->ID});
          continue;
        }
        const auto [it, inserted] =
            allProvinceRegions.emplace(province->ID, ardaRegion->ID);
        if (!inserted && it->second != ardaRegion->ID) {
          normalized.errors.push_back({StartYear,
                                       "Province belongs to multiple regions",
                                       province->ID, ardaRegion->ID});
          continue;
        }
        if (!isEligible(province))
          continue;
        const auto [provinceIt, provinceInserted] =
            normalized.provinces.emplace(province->ID, province);
        if (!provinceInserted && provinceIt->second != province) {
          normalized.errors.push_back({StartYear,
                                       "Input contains duplicate province IDs",
                                       province->ID, ardaRegion->ID});
          continue;
        }
        normalized.provinceRegions[province->ID] = ardaRegion->ID;
        normalized.provinceContinents[province->ID] = continent->ID;
        normalized.regions[ardaRegion->ID].push_back(province->ID);
      }
    }
  }

  for (auto &[regionId, provinceIds] : normalized.regions)
    std::sort(provinceIds.begin(), provinceIds.end());
  if (input.climateData && (input.climateData->habitabilities.empty() ||
                            input.climateData->arableLand.empty())) {
    normalized.errors.push_back(
        {StartYear,
         "Climate input must provide habitability and arable-land data",
         {},
         {}});
  }
  if (input.terrainData && input.terrainData->inclination.empty()) {
    normalized.errors.push_back(
        {StartYear, "Terrain input must provide inclination data", {}, {}});
  }

  for (const auto &[provinceId, province] : normalized.provinces) {
    auto &neighbours = normalized.neighbours[provinceId];
    for (const auto &baseNeighbour : province->provinceNeighbours) {
      auto neighbour = std::dynamic_pointer_cast<ArdaProvince>(baseNeighbour);
      if (neighbour && normalized.provinces.contains(neighbour->ID))
        neighbours.push_back(neighbour->ID);
    }
    std::sort(neighbours.begin(), neighbours.end());
    neighbours.erase(std::unique(neighbours.begin(), neighbours.end()),
                     neighbours.end());
    NormalizedInput::Environment environment;
    environment.coastal = province->isCoastalToOcean();
    const auto regionId = normalized.provinceRegions.at(provinceId);
    const auto regionIsIsland = normalized.islandRegions.contains(regionId);
    const auto provinceIsIslandSubtype =
        province->areaSubType == Fwg::Areas::AreaSubType::Island ||
        province->areaSubType == Fwg::Areas::AreaSubType::CoastalIsland ||
        province->areaSubType == Fwg::Areas::AreaSubType::LakeIsland;
    environment.island = regionIsIsland || provinceIsIslandSubtype;
    environment.area = province->pixels.size();
    if (input.climateData || input.terrainData) {
      double habitability = 0.0;
      double arableLand = 0.0;
      double inclination = 0.0;
      size_t samples = 0;
      for (const auto pixel : province->pixels) {
        if (pixel < 0)
          continue;
        const auto index = static_cast<size_t>(pixel);
        if (input.climateData &&
            index < input.climateData->habitabilities.size())
          habitability += input.climateData->habitabilities[index];
        if (input.climateData && index < input.climateData->arableLand.size())
          arableLand += input.climateData->arableLand[index];
        if (input.terrainData && index < input.terrainData->inclination.size())
          inclination += input.terrainData->inclination[index];
        ++samples;
      }
      if (samples > 0) {
        environment.habitability = std::clamp(habitability / samples, 0.0, 1.0);
        environment.arableLand = std::clamp(arableLand / samples, 0.0, 1.0);
        environment.inclination = std::max(0.0, inclination / samples);
      }
    }
    if (!input.climateData && province->habitability > 0.0f)
      environment.habitability =
          std::clamp(static_cast<double>(province->habitability), 0.0, 1.0);
    normalized.environments[provinceId] = environment;
  }

  const auto weightedRoutes =
      buildWeightedSeaRoutes(input.continents, input.terrainData);
  for (const auto &[sourceId, routes] : weightedRoutes) {
    const auto sourceContinent = normalized.provinceContinents.find(sourceId);
    if (sourceContinent == normalized.provinceContinents.end())
      continue;
    for (const auto &route : routes) {
      const auto environment = normalized.environments.find(route.provinceId);
      const auto targetContinent =
          normalized.provinceContinents.find(route.provinceId);
      if (environment == normalized.environments.end() ||
          targetContinent == normalized.provinceContinents.end())
        continue;
      if (!environment->second.island &&
          sourceContinent->second == targetContinent->second)
        continue;
      normalized.seaRoutesFrom[sourceId].push_back(route);
      normalized.seaRoutesTo[route.provinceId].push_back(
          {sourceId, route.distance});
    }
  }

  return normalized;
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

double polityCapacityAt(const Configuration &configuration, Year year) {
  const auto compound = [](double multiplier, double rate, Year years) {
    return multiplier *
           std::pow(1.0 + rate,
                    static_cast<double>(std::max(0, years)) / 100.0);
  };
  const auto renaissanceStart =
      std::max(configuration.renaissanceStartYear, configuration.startYear);
  const auto industrialStart =
      std::max(configuration.industrialRevolutionStartYear, renaissanceStart);
  auto multiplier =
      compound(1.0, configuration.ancientPolityCapacityGrowthPerCentury,
               std::min(year, renaissanceStart) - configuration.startYear);
  multiplier = compound(multiplier,
                        configuration.renaissancePolityCapacityGrowthPerCentury,
                        std::min(year, industrialStart) - renaissanceStart);
  return static_cast<double>(configuration.initialPolityCapacity) *
         compound(multiplier,
                  configuration.industrialPolityCapacityGrowthPerCentury,
                  year - industrialStart);
}

double capacityExpansionMultiplier(const Configuration &configuration,
                                   Year year, size_t territorySize) {
  const auto capacity = polityCapacityAt(configuration, year);
  if (capacity <= 0.0 || territorySize <= capacity)
    return 1.0;
  const auto overCapacity =
      (static_cast<double>(territorySize) - capacity) / capacity;
  return 1.0 /
         (1.0 + configuration.overCapacityExpansionPenalty * overCapacity);
}

double expansionBorderScore(
    ProvinceId targetProvince, PolityId attacker,
    const std::map<ProvinceId, std::vector<ProvinceId>> &neighbours,
    const std::map<ProvinceId, ProvinceState> &provinces) {
  const auto &targetNeighbours = neighbours.at(targetProvince);
  double score = static_cast<double>(targetNeighbours.size());
  for (const auto neighbourId : targetNeighbours) {
    const auto &neighbour = provinces.at(neighbourId);
    if (neighbour.owner == attacker)
      score += 3.0;

    size_t attackerNeighbours = 0;
    size_t foreignNeighbours = 0;
    for (const auto surroundingId : neighbours.at(neighbourId)) {
      const auto &surrounding = provinces.at(surroundingId);
      if (surrounding.owner == attacker)
        ++attackerNeighbours;
      else if (surrounding.owner == neighbour.owner)
        ++foreignNeighbours;
    }
    if (foreignNeighbours > 0 && attackerNeighbours + 1 >= foreignNeighbours)
      score -= 8.0;
    if (foreignNeighbours == 1 && attackerNeighbours > 0)
      score -= 12.0;
  }
  return score;
}

double expansionTargetWeakness(PolityId attacker, PolityId defender,
                               const std::map<PolityId, PolityStrength> &strengths) {
  if (defender == NoPolity)
    return 1.0;
  const auto attackerIt = strengths.find(attacker);
  const auto defenderIt = strengths.find(defender);
  if (attackerIt == strengths.end() || defenderIt == strengths.end())
    return 0.0;
  const auto attackerScore = std::max(1.0, attackerIt->second.score);
  const auto defenderScore = std::max(0.0, defenderIt->second.score);
  return std::clamp(attackerScore /
                        std::max(1.0, attackerScore + defenderScore),
                    0.0, 1.0);
}

bool remainsContiguousAfterConquest(
    PolityId owner, ProvinceId removedProvince,
    const std::map<ProvinceId, std::vector<ProvinceId>> &neighbours,
    const std::map<ProvinceId, ProvinceState> &provinces) {
  std::vector<ProvinceId> remaining;
  remaining.reserve(provinces.size());
  for (const auto &[provinceId, province] : provinces)
    if (province.owner == owner && provinceId != removedProvince)
      remaining.push_back(provinceId);
  if (remaining.size() < 2)
    return true;

  std::set<ProvinceId> unvisited(remaining.begin(), remaining.end());
  std::vector<ProvinceId> frontier{remaining.front()};
  unvisited.erase(remaining.front());
  while (!frontier.empty()) {
    const auto provinceId = frontier.back();
    frontier.pop_back();
    for (const auto neighbourId : neighbours.at(provinceId)) {
      if (unvisited.erase(neighbourId) > 0)
        frontier.push_back(neighbourId);
    }
  }
  return unvisited.empty();
}

void applyEvent(State &state, const Event &event) {
  state.year = event.year;
  switch (event.type) {
  case EventType::CreatePolity:
    state.polities[event.polityId] = {
        event.polityId, event.year, std::nullopt,
        std::nullopt,   {},         event.description == "tribe",
        event.colour,    -1};
    break;
  case EventType::CreateSuccessorPolity:
    state.polities[event.polityId] = {
        event.polityId,
        event.year,
        std::nullopt,
        event.parentId >= 0 ? std::optional<PolityId>(event.parentId)
                            : std::nullopt,
        {},
        false,
        event.colour,
        event.parentId >= 0 && state.polities.contains(event.parentId)
            ? state.polities.at(event.parentId).primaryCulture
            : -1};
    if (event.parentId >= 0 && state.polities.contains(event.parentId))
      state.polities.at(event.parentId).successorIds.push_back(event.polityId);
    break;
  case EventType::ConvertCulturePopulation:
    if (auto province = state.provinces.find(event.provinceId);
        province != state.provinces.end()) {
      const auto sourceCulture = static_cast<CultureId>(event.parentId);
      province->second.culturePopulations[sourceCulture] =
          std::max(0.0, province->second.culturePopulations[sourceCulture] -
                            event.value);
      province->second.culturePopulations[event.cultureId] += event.value;
      refreshDominantCulture(province->second);
    }
    break;
  case EventType::UpdatePolityStrength:
    state.polityStrengths[event.polityId] = {event.value, event.secondaryValue,
                                             event.score};
    break;
  case EventType::InitializeProvince:
    state.provinces[event.provinceId] = {event.polityId, event.cultureId,
                                         event.religionId, event.value,
                                         event.secondaryValue};
    state.provinces[event.provinceId].culturePopulations[event.cultureId] =
        event.value;
    state.provinces[event.provinceId].coastal = event.coastal;
    state.provinces[event.provinceId].island = event.island;
    state.cultures.try_emplace(event.cultureId,
                               CultureLineage{event.cultureId, event.provinceId,
                                              event.year, std::nullopt,
                                              event.colour});
    if (event.polityId != NoPolity && state.polities.contains(event.polityId) &&
        state.polities.at(event.polityId).primaryCulture < 0)
      state.polities.at(event.polityId).primaryCulture = event.cultureId;
    break;
  case EventType::TransferProvince:
  case EventType::ConsolidateRegion:
    if (auto province = state.provinces.find(event.provinceId);
        province != state.provinces.end()) {
      province->second.owner = event.polityId;
      if (event.polityId != NoPolity && state.polities.contains(event.polityId) &&
          state.polities.at(event.polityId).primaryCulture < 0)
        state.polities.at(event.polityId).primaryCulture =
            province->second.culture;
    }
    break;
  case EventType::SetPrimaryCulture:
    if (state.polities.contains(event.polityId))
      state.polities.at(event.polityId).primaryCulture = event.cultureId;
    break;
  case EventType::DissolvePolity:
    if (auto polity = state.polities.find(event.polityId);
        polity != state.polities.end())
      polity->second.dissolvedYear = event.year;
    break;
  case EventType::CreateCulture:
    state.cultures.try_emplace(
        event.cultureId,
        CultureLineage{event.cultureId, event.provinceId, event.year,
                       event.parentId >= 0
                           ? std::optional<CultureId>(event.parentId)
                           : std::nullopt,
                       event.colour});
    break;
  case EventType::SetCulture:
    if (auto province = state.provinces.find(event.provinceId);
        province != state.provinces.end())
      province->second.culture = event.cultureId;
    break;
  case EventType::CreateReligion:
    state.religions.try_emplace(event.religionId,
                                ReligionLineage{event.religionId,
                                                event.provinceId, event.year,
                                                std::nullopt, event.colour});
    if (event.parentId >= 0)
      state.religions.at(event.religionId).parentId = event.parentId;
    break;
  case EventType::SetReligion:
    if (auto province = state.provinces.find(event.provinceId);
        province != state.provinces.end())
      province->second.religion = event.religionId;
    break;
  case EventType::UpdatePopulation:
    if (auto province = state.provinces.find(event.provinceId);
        province != state.provinces.end()) {
      const auto previousPopulation = province->second.population;
      province->second.population = event.value;
      if (previousPopulation > 0.0) {
        const auto factor = event.value / previousPopulation;
        for (auto &[cultureId, population] :
             province->second.culturePopulations)
          population *= factor;
      }
    }
    break;
  case EventType::UpdateDevelopment:
    if (auto province = state.provinces.find(event.provinceId);
        province != state.provinces.end())
      province->second.development = event.value;
    break;
  case EventType::UpdateCarryingCapacity:
    if (auto province = state.provinces.find(event.provinceId);
        province != state.provinces.end())
      province->second.carryingCapacity = event.value;
    break;
  case EventType::MigratePopulation:
    // if (auto source = state.provinces.find(event.provinceId);
    //     source != state.provinces.end())
    //   source->second.population =
    //       std::max(0.0, source->second.population - event.value);
    // if (auto target = state.provinces.find(event.regionId);
    //     target != state.provinces.end())
    //   target->second.population += event.value;
    break;
  case EventType::SetRegionalPhase:
    state.regionalPhases[event.regionId] =
        static_cast<RegionalPhase>(static_cast<int>(event.value));
    break;
  case EventType::SetSuperRegion:
    state.regionSuperRegions[event.regionId] =
        static_cast<SuperRegionId>(event.value);
    break;
  case EventType::CreateSuperRegion:
    state.superRegions.try_emplace(
        event.regionId, DevelopmentSuperRegion{event.regionId, event.polityId});
    break;
  case EventType::SetSuperRegionPhase:
    if (auto superRegion =
            state.superRegions.find(static_cast<SuperRegionId>(event.regionId));
        superRegion != state.superRegions.end())
      superRegion->second.phase =
          static_cast<SuperRegionPhase>(static_cast<int>(event.value));
    break;
  case EventType::MigrateCulturePopulation:
    // if (auto source = state.provinces.find(event.provinceId);
    //     source != state.provinces.end())
    //   source->second.culturePopulations[event.cultureId] =
    //       std::max(0.0, source->second.culturePopulations[event.cultureId] -
    //                         event.value);
    // if (auto target = state.provinces.find(event.regionId);
    //     target != state.provinces.end())
    //   target->second.culturePopulations[event.cultureId] += event.value;
    // if (auto source = state.provinces.find(event.provinceId);
    //     source != state.provinces.end())
    //   source->second.culture = state.dominantCultureOf(event.provinceId);
    // if (auto target = state.provinces.find(event.regionId);
    //     target != state.provinces.end())
    //   target->second.culture = state.dominantCultureOf(event.regionId);
    break;
  case EventType::ColonizeProvince:
    if (auto province = state.provinces.find(event.provinceId);
        province != state.provinces.end()) {
      province->second.owner = event.polityId;
      if (event.polityId != NoPolity && state.polities.contains(event.polityId) &&
          state.polities.at(event.polityId).primaryCulture < 0)
        state.polities.at(event.polityId).primaryCulture =
            province->second.culture;
    }
    break;
  }
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
  }
  return "Unknown";
}

using AppendEvent = std::function<void(Event &&)>;

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

void updateSuperRegionPhases(Year year, Year nextYear,
                             const Configuration &configuration, State &state,
                             const AppendEvent &append) {
  std::vector<SuperRegionId> ordered;
  for (const auto &[id, superRegion] : state.superRegions)
    ordered.push_back(id);
  if (nextYear >= configuration.persistentDevelopmentStartYear) {
    std::sort(ordered.begin(), ordered.end(),
              [&](const auto left, const auto right) {
                return state.superRegions.at(left).development >
                       state.superRegions.at(right).development;
              });
  } else if (!ordered.empty()) {
    std::rotate(ordered.begin(),
                ordered.begin() +
                    RandNum::getRandom<int>(static_cast<int>(ordered.size())),
                ordered.end());
  }
  for (size_t index = 0; index < ordered.size(); ++index) {
    const auto phase =
        index < 2 ? SuperRegionPhase::Booming : SuperRegionPhase::Lagging;
    append({nextYear, EventType::SetSuperRegionPhase, -1, ordered[index],
            NoPolity, NoPolity, -1, NoReligion,
            static_cast<double>(static_cast<int>(phase)), 0.0,
            "superregional development cycle"});
  }
}

void updateRegionalPhases(Year year, Year nextYear,
                          const Configuration &configuration,
                          const NormalizedInput &normalized,
                          const AppendEvent &append) {
  for (const auto &[regionId, provinces] : normalized.regions) {
    const auto phase =
        static_cast<RegionalPhase>(RandNum::getRandom<int>(3) - 1);
    append({nextYear, EventType::SetRegionalPhase, -1, regionId, NoPolity,
            NoPolity, -1, NoReligion,
            static_cast<double>(static_cast<int>(phase)), 0.0,
            "regional boom or bust transition"});
  }
}

void updateProvinceGrowth(
    const Year nextYear, double centuries, const Configuration &configuration,
    const NormalizedInput &normalized, State &state,
    const std::map<ProvinceId, double> &growthPotential,
    const std::map<ProvinceId, double> &baseCapacity, const AppendEvent &append,
    std::map<PolityId, std::vector<ProvinceId>> &territories) {
  for (const auto &[provinceId, province] : state.provinces) {
    const auto potential = growthPotential.at(provinceId);
    const auto regionId = normalized.provinceRegions.at(provinceId);
    const auto phase = state.regionalPhases.contains(regionId)
                           ? state.regionalPhases.at(regionId)
                           : RegionalPhase::Neutral;
    const auto superRegionId = state.regionSuperRegions.at(regionId);
    const auto superPhase = state.superRegions.at(superRegionId).phase;
    const bool bordersBoomingSuperRegion =
        std::any_of(state.superRegions.at(superRegionId).neighbours.begin(),
                    state.superRegions.at(superRegionId).neighbours.end(),
                    [&](const auto neighbourId) {
                      return state.superRegions.at(neighbourId).phase ==
                             SuperRegionPhase::Booming;
                    });
    const double superRegionMultiplier =
        (superPhase == SuperRegionPhase::Booming   ? 1.8
         : superPhase == SuperRegionPhase::Lagging ? 0.55
                                                   : 1.0) +
        (bordersBoomingSuperRegion && superPhase != SuperRegionPhase::Booming
             ? configuration.superRegionNeighbourInfluence
             : 0.0);
    const double capacity = baseCapacity.at(provinceId) *
                            capacityEraMultiplier(configuration, nextYear) *
                            (1.0 + province.development * 0.05) *
                            phaseMultiplier(phase) * superRegionMultiplier;
    const double densityFactor = std::clamp(
        1.0 - province.population / std::max(capacity, 1.0), 0.0, 1.0);
    const auto immigrationHeadroom =
        std::any_of(normalized.neighbours.at(provinceId).begin(),
                    normalized.neighbours.at(provinceId).end(),
                    [&](const auto neighbourId) {
                      const auto &neighbour = state.provinces.at(neighbourId);
                      return neighbour.carryingCapacity > neighbour.population;
                    });
    const double phaseGrowth = phaseMultiplier(phase) * superRegionMultiplier;
    const double immigrationBonus = immigrationHeadroom ? 1.0 : 0.0;
    const double emigrationPenalty =
        normalized.neighbours.at(provinceId).empty() ? 0.0 : 0.25;
    const double gainChance = std::clamp(
        configuration.populationGrowthPerCentury * centuries * potential *
            densityFactor *
            (0.5 + phaseGrowth + immigrationBonus - emigrationPenalty),
        0.0, 1.0);
    auto population = province.population;
    if (chance(gainChance) && population < capacity)
      population = std::floor(population) + 1.0;
    population = std::max(configuration.minimumPopulation,
                          std::min(population, std::floor(capacity)));
    const double developmentChange =
        configuration.developmentGrowthPerCentury * potential *
        (1.0 + std::log1p(province.population) / 10.0) * centuries *
        (phase == RegionalPhase::Bust ? -0.35 : phaseMultiplier(phase)) *
        superRegionMultiplier;
    const double development =
        std::max(0.0, province.development + developmentChange);
    state.superRegions[superRegionId].development += developmentChange;
    append({nextYear, EventType::UpdateCarryingCapacity, provinceId, regionId,
            NoPolity, NoPolity, -1, NoReligion, capacity,
            capacityEraMultiplier(configuration, nextYear),
            "era-adjusted carrying capacity"});
    if (population != province.population) {
      append({nextYear, EventType::UpdatePopulation, provinceId, -1, NoPolity,
              NoPolity, -1, NoReligion, population, 0.0, "population growth"});
    }
    if (development != province.development) {
      append({nextYear, EventType::UpdateDevelopment, provinceId, -1, NoPolity,
              NoPolity, -1, NoReligion, development, 0.0,
              "development growth"});
    }
    territories[province.owner].push_back(provinceId);
  }
}

void updatePolityStrengths(const Year nextYear, State &state,
                           const AppendEvent &append) {
  std::map<PolityId, PolityStrength> strengths;
  for (const auto &[provinceId, province] : state.provinces) {
    auto &strength = strengths[province.owner];
    strength.population += province.population;
    strength.development += province.development;
  }
  for (auto &[polityId, strength] : strengths) {
    strength.score = strength.population * 0.01 + strength.development * 10.0;
    append({nextYear,
            EventType::UpdatePolityStrength,
            -1,
            -1,
            polityId,
            NoPolity,
            -1,
            NoReligion,
            strength.population,
            strength.development,
            "polity strength snapshot",
            -1,
            {},
            strength.score});
  }
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
    const auto centuriesSinceMaritimeStart = std::max(
        0.0, static_cast<double>(nextYear -
                                 configuration.maritimeExpansionStartYear) /
                   100.0);
    const auto preRenaissanceRange =
        configuration.initialMaritimeRange *
        std::pow(1.0 + configuration.maritimeRangeGrowthPerCentury,
                 centuriesSinceMaritimeStart);
    const auto renaissanceYears = std::max(
        0.0, static_cast<double>(nextYear - configuration.renaissanceStartYear) /
                 100.0);
    const auto maritimeRange = std::max(
        1.0, nextYear < configuration.renaissanceStartYear
                 ? preRenaissanceRange
                 : preRenaissanceRange *
                       configuration.renaissanceMaritimeRangeMultiplier *
                       std::pow(
                           1.0 +
                               configuration.renaissanceMaritimeRangeGrowthPerCentury,
                           renaissanceYears));
    const auto routesToProvince = normalized.seaRoutesTo.find(provinceId);
    const bool reachableBySea =
        routesToProvince != normalized.seaRoutesTo.end() &&
        std::any_of(routesToProvince->second.begin(), routesToProvince->second.end(),
                    [&](const auto &route) {
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

  const auto centuriesSinceStart = std::max(
      0.0,
      static_cast<double>(nextYear - configuration.maritimeExpansionStartYear) /
          100.0);
  const auto preRenaissanceRange =
      configuration.initialMaritimeRange *
      std::pow(1.0 + configuration.maritimeRangeGrowthPerCentury,
               centuriesSinceStart);
  const auto renaissanceYears = std::max(
      0.0, static_cast<double>(nextYear - configuration.renaissanceStartYear) /
               100.0);
  const auto maritimeRange = std::max(
      1.0, nextYear < configuration.renaissanceStartYear
               ? preRenaissanceRange
               : preRenaissanceRange *
                     configuration.renaissanceMaritimeRangeMultiplier *
                     std::pow(1.0 +
                                  configuration.renaissanceMaritimeRangeGrowthPerCentury,
                              renaissanceYears));
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
            !remainsContiguousAfterConquest(
                candidate.owner, candidateId, normalized.neighbours,
                state.provinces))
          continue;
        const auto weakness = expansionTargetWeakness(
            polityId, candidate.owner, state.polityStrengths);
        const auto candidateScore = weakness -
                                    route.distance /
                                        std::max(1.0, maritimeRange);
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

void balancePolities(const Year nextYear, double centuries,
                     const Configuration &configuration,
                     const NormalizedInput &normalized, State &state,
                     const AppendEvent &append, PolityId &nextPolity,
                     std::map<PolityId, std::vector<ProvinceId>> &territories) {
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
  const double expansionPressure =
      std::max(consolidationPressure, aggressive ? 0.35 : 0.0);
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
  for (const auto &[provinceId, province] : state.provinces) {
    const auto territorySize = territories[province.owner].size();
    const auto capacityMultiplier =
        capacityExpansionMultiplier(configuration, nextYear, territorySize);
    if (!chance(configuration.expansionChance * expansionMultiplier *
                centuries * expansionPressure * capacityMultiplier))
      continue;
    const auto &neighbours = normalized.neighbours.at(provinceId);
    if (neighbours.empty())
      continue;
    std::vector<std::pair<ProvinceId, double>> candidates;
    candidates.reserve(neighbours.size());
    for (const auto neighbourId : neighbours) {
      const auto &neighbour = state.provinces.at(neighbourId);
      if (neighbour.owner == province.owner)
        continue;
      if (!remainsContiguousAfterConquest(
              neighbour.owner, neighbourId, normalized.neighbours,
              state.provinces))
        continue;
      const auto attacker = state.polityStrengths.at(province.owner).score;
      const auto defender = state.polityStrengths.at(neighbour.owner).score;
      const auto targetTerritory = territories.find(neighbour.owner);
      const auto targetSize = targetTerritory == territories.end()
                                  ? size_t{0}
                                  : targetTerritory->second.size();
      const auto smallPolityConsolidationMultiplier =
          nextYear >= configuration.regionOwnershipYear &&
                  targetSize <= configuration.lateSmallPolitySize
              ? configuration.lateSmallPolityConquestMultiplier
              : 1.0;
      if (!chance(std::clamp(
              attacker / std::max(1.0, attacker + defender) *
                  smallPolityConsolidationMultiplier,
              0.0, 1.0)))
        continue;
      candidates.emplace_back(neighbourId,
                              expansionBorderScore(neighbourId, province.owner,
                                                   normalized.neighbours,
                                                   state.provinces) +
                                  8.0 * expansionTargetWeakness(
                                            province.owner, neighbour.owner,
                                            state.polityStrengths));
    }
    if (candidates.empty())
      continue;
    const auto bestScore =
        std::max_element(candidates.begin(), candidates.end(),
                         [](const auto &left, const auto &right) {
                           return left.second < right.second;
                         })
            ->second;
    std::vector<ProvinceId> bestCandidates;
    for (const auto &[candidateId, score] : candidates)
      if (score >= bestScore - 0.001)
        bestCandidates.push_back(candidateId);
    const auto selected = bestCandidates[RandNum::getRandom<int>(
        static_cast<int>(bestCandidates.size()))];
    const auto &target = state.provinces.at(selected);
    append({nextYear, EventType::TransferProvince, selected,
            normalized.provinceRegions.at(selected), province.owner,
            target.owner, -1, NoReligion, 0.0, 0.0, "provincial expansion"});
  }

  territories.clear();
  for (const auto &[provinceId, province] : state.provinces)
    territories[province.owner].push_back(provinceId);
  maritimeExpansion(nextYear, centuries, configuration, normalized, state,
                    append, territories);

  territories.clear();
  for (const auto &[provinceId, province] : state.provinces)
    territories[province.owner].push_back(provinceId);
  const auto strongestPolity = strongestPolityOf(state);
  const auto strongestScore =
      strongestPolity == NoPolity
          ? 0.0
          : state.polityStrengths.at(strongestPolity).score;
  for (const auto &[polityId, provinceIds] : territories) {
    if (provinceIds.size() < 2)
      continue;
    const auto splitProvince = provinceIds.back();
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
      if (province.religion != NoReligion &&
          neighbour.religion != NoReligion &&
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
    if (!primaryCultureNeighbour &&
        state.provinces.at(provinceId).island &&
        nextYear >= configuration.maritimeExpansionStartYear) {
      double closestSeaDistance = std::numeric_limits<double>::max();
      const auto routesToProvince = normalized.seaRoutesTo.find(provinceId);
      if (routesToProvince != normalized.seaRoutesTo.end())
        for (const auto &route : routesToProvince->second) {
          if (route.distance >= closestSeaDistance)
            continue;
          const auto source = state.provinces.find(route.provinceId);
          if (source != state.provinces.end() &&
              source->second.owner == province.owner &&
              source->second.coastal)
            closestSeaDistance = route.distance;
        }
      const auto centuriesSinceMaritimeStart = std::max(
          0.0, static_cast<double>(nextYear -
                                   configuration.maritimeExpansionStartYear) /
                     100.0);
      const auto preRenaissanceRange =
          configuration.initialMaritimeRange *
          std::pow(1.0 + configuration.maritimeRangeGrowthPerCentury,
                   centuriesSinceMaritimeStart);
      const auto renaissanceYears = std::max(
          0.0, static_cast<double>(nextYear - configuration.renaissanceStartYear) /
                   100.0);
      const auto maritimeRange = std::max(
          1.0, nextYear < configuration.renaissanceStartYear
                   ? preRenaissanceRange
                   : preRenaissanceRange *
                         configuration.renaissanceMaritimeRangeMultiplier *
                         std::pow(
                             1.0 +
                                 configuration.renaissanceMaritimeRangeGrowthPerCentury,
                             renaissanceYears));
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
    } else if (province.culture != primaryCulture &&
               !primaryCultureNeighbour &&
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
  std::map<PolityId, std::vector<ProvinceId>> territories;
  for (const auto &[provinceId, province] : state.provinces)
    territories[province.owner].push_back(provinceId);
  for (const auto &[polityId, polity] : state.polities) {
    if (!polity.dissolvedYear && !territories.contains(polityId))
      append({nextYear, EventType::DissolvePolity, -1, -1, polityId, NoPolity,
              -1, NoReligion, 0.0, 0.0, "annexed or dissolved"});
  }
}

} // namespace

const ProvinceState *State::findProvince(ProvinceId provinceId) const {
  const auto province = provinces.find(provinceId);
  return province == provinces.end() ? nullptr : &province->second;
}

std::vector<ProvinceId> State::territoryOf(PolityId polityId) const {
  std::vector<ProvinceId> territory;
  for (const auto &[provinceId, province] : provinces) {
    if (province.owner == polityId)
      territory.push_back(provinceId);
  }
  return territory;
}

CultureId State::dominantCultureOf(ProvinceId provinceId) const {
  const auto province = provinces.find(provinceId);
  if (province == provinces.end() ||
      province->second.culturePopulations.empty())
    return -1;
  return province->second.culture;
}

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
  std::map<ProvinceId, double> growthPotential;
  std::map<ProvinceId, double> baseCapacity;
  auto append = [&](Event &&event) {
    result.events.emplace_back(std::move(event));
    if (result.events.back().type == EventType::InitializeProvince) {
      const auto provinceId = result.events.back().provinceId;
      result.events.back().coastal =
          normalized.environments.at(provinceId).coastal;
      result.events.back().island = normalized.environments.at(provinceId).island;
    }
    ++result.eventCounts[result.events.back().type];
    const auto &appliedEvent = result.events.back();
    applyEvent(state, appliedEvent);
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
    append({configuration.startYear, EventType::InitializeProvince, provinceId,
            normalized.provinceRegions.at(provinceId), polityId, NoPolity,
            cultureId, NoReligion, population, development,
            "initial tribe and culture", -1, randomPolityColour()});
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
      updateSuperRegionPhases(year, nextYear, configuration, state, append);
    }
    if (!(configuration.regionPhaseDurationYears <= 0 ||
          (nextYear - configuration.startYear) /
                  configuration.regionPhaseDurationYears <=
              (year - configuration.startYear) /
                  configuration.regionPhaseDurationYears)) {
      updateRegionalPhases(year, nextYear, configuration, normalized, append);
    }
    std::map<PolityId, std::vector<ProvinceId>> territories;
    updateProvinceGrowth(nextYear, centuries, configuration, normalized, state,
                         growthPotential, baseCapacity, append, territories);
    updatePolityStrengths(nextYear, state, append);
    integrateCultures(nextYear, centuries, configuration, state, append);
    /*colonize(nextYear, centuries, configuration, normalized, state, append,
             strongestPolityOf(state));*/
    balancePolities(nextYear, centuries, configuration, normalized, state,
                    append, nextPolity, territories);
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
    applyEvent(eventState, event);
  }
  return result;
}

State HistorySimulation::reconstruct(const std::vector<Event> &events,
                                     Year year) const {
  State state;
  for (const auto &event : events) {
    if (event.year > year)
      break;
    applyEvent(state, event);
    state.year = event.year;
  }
  return state;
}

std::vector<ValidationError>
HistorySimulation::validate(const State &state, Year year,
                            bool requireWholeRegions) const {
  std::vector<ValidationError> errors;
  for (const auto &[provinceId, province] : state.provinces) {
    if (province.owner == NoPolity || !state.polities.contains(province.owner))
      errors.push_back({year, "Province has no valid owner", provinceId, {}});
    if (province.culture < 0 || !state.cultures.contains(province.culture))
      errors.push_back({year, "Province has no valid culture", provinceId, {}});
    if (province.religion != NoReligion &&
        !state.religions.contains(province.religion))
      errors.push_back(
          {year, "Province has no valid religion", provinceId, {}});
    if (province.population < configuration.minimumPopulation)
      errors.push_back({year,
                        "Province population is below the configured minimum",
                        provinceId,
                        {}});
    if (province.development < 0.0)
      errors.push_back(
          {year, "Province development is negative", provinceId, {}});
    if (province.carryingCapacity < configuration.minimumPopulation)
      errors.push_back(
          {year,
           "Province carrying capacity is below the configured minimum",
           provinceId,
           {}});
    const auto culturalPopulation = std::accumulate(
        province.culturePopulations.begin(), province.culturePopulations.end(),
        0.0,
        [](double total, const auto &entry) { return total + entry.second; });
    if (std::abs(culturalPopulation - province.population) >
        std::max(0.001, province.population * 0.001))
      errors.push_back(
          {year,
           "Cultural populations do not sum to province population",
           provinceId,
           {}});
  }
  const auto boomingSuperRegions =
      std::count_if(state.superRegions.begin(), state.superRegions.end(),
                    [](const auto &entry) {
                      return entry.second.phase == SuperRegionPhase::Booming;
                    });
  if (boomingSuperRegions > 2)
    errors.push_back({year, "More than two superregions are booming", {}, {}});
  if (requireWholeRegions) {
    std::map<RegionId, PolityId> owners;
    for (const auto &[provinceId, province] : state.provinces) {
      const auto region = provinceRegions.find(provinceId);
      if (region == provinceRegions.end())
        continue;
      const auto [owner, inserted] =
          owners.emplace(region->second, province.owner);
      if (!inserted && owner->second != province.owner)
        errors.push_back(
            {year, "Region has split ownership", {}, region->second});
    }
  }
  return errors;
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