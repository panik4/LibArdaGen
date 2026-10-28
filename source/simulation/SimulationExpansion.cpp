#include "simulation/SimulationExpansion.h"
#include "simulation/SimulationState.h"

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
using warfare::maritimeRangeForYear;

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
  for (ProvinceId provinceId = 0;
       provinceId < static_cast<ProvinceId>(state.provinces.size());
       ++provinceId) {
    const auto *province = state::findProvince(state, provinceId);
    if (!province)
      continue;
    if (province->owner == strongestPolity && province->coastal &&
        (sourceProvince < 0 ||
         province->development >
             state::findProvince(state, sourceProvince)->development))
      sourceProvince = provinceId;
  }
  if (sourceProvince < 0)
    return;
  ProvinceId targetProvince = -1;
  for (ProvinceId provinceId = 0;
       provinceId < static_cast<ProvinceId>(state.provinces.size());
       ++provinceId) {
    const auto *province = state::findProvince(state, provinceId);
    if (!province)
      continue;
    const auto targetRegion = normalized.provinceRegions.at(provinceId);
    const auto targetSuperRegion = state.regionSuperRegions.at(targetRegion);
    const bool lagging = state.superRegions.at(targetSuperRegion).phase ==
                         SuperRegionPhase::Lagging;
    const auto *source = state::findProvince(state, sourceProvince);
    const bool reachableByLand = source && province->continentId >= 0 &&
                                 province->continentId == source->continentId;
    const auto maritimeRange = maritimeRangeForYear(nextYear, configuration);
    const auto routesToProvince = normalized.seaRoutesTo.find(provinceId);
    const bool reachableBySea =
        routesToProvince != normalized.seaRoutesTo.end() &&
        std::any_of(
            routesToProvince->second.begin(), routesToProvince->second.end(),
            [&](const auto &route) {
              return route.distance <= maritimeRange &&
                     state::findProvince(state, route.provinceId) &&
                     state::findProvince(state, route.provinceId)->coastal &&
                     state::findProvince(state, route.provinceId)->owner ==
                         strongestPolity;
            });
    if (province->owner != strongestPolity && lagging &&
        (reachableByLand || reachableBySea) &&
        (targetProvince < 0 ||
         province->development <
             state::findProvince(state, targetProvince)->development))
      targetProvince = provinceId;
  }
  if (targetProvince < 0 ||
      !chance(configuration.colonialSettlementRatePerCentury * centuries *
              capacityExpansionMultiplier(
                  configuration, nextYear,
                  static_cast<size_t>(std::count_if(
                      state.provinces.begin(), state.provinces.end(),
                      [strongestPolity](const auto &entry) {
                        return entry.initialized &&
                               entry.owner == strongestPolity;
                      })))))
    return;
  append({nextYear, EventType::ColonizeProvince, targetProvince,
          normalized.provinceRegions.at(targetProvince), strongestPolity,
          state::findProvince(state, targetProvince)->owner, -1, NoReligion,
          1.0, 0.0, "colonial settlement"});
}

} // namespace Arda::Simulation::expansion
