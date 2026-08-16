#include "simulation/SimulationExpansion.h"

#include "RandNum.h"
#include "simulation/SimulationDevelopment.h"
#include "simulation/SimulationWarfare.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Arda::Simulation::expansion {
namespace {

using detail::AppendEvent;
using detail::NormalizedInput;
using development::capacityExpansionMultiplier;
using warfare::expansionTargetWeakness;
using warfare::maritimeRangeForYear;
using warfare::remainsContiguousAfterConquest;

bool chance(double probability) {
  return probability > 0.0 && RandNum::getRandom<double>(1.0) < probability;
}

} // namespace

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
  if (sourceProvince < 0)
    return;
  ProvinceId targetProvince = -1;
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

void maritime(const Year nextYear, double centuries,
              const Configuration &configuration,
              const NormalizedInput &normalized, State &state,
              const AppendEvent &append,
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

} // namespace Arda::Simulation::expansion
