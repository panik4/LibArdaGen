#include "simulation/SimulationValidation.h"

#include "simulation/SimulationState.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace Arda::Simulation::validation {

std::vector<ValidationError>
validateState(const State &state, const Configuration &configuration,
              const std::map<ProvinceId, RegionId> &provinceRegions, Year year,
              bool requireWholeRegions) {
  std::vector<ValidationError> errors;
  std::map<PolityId, size_t> territorySizes;
  for (ProvinceId provinceId = 0;
       provinceId < static_cast<ProvinceId>(state.provinces.size());
       ++provinceId) {
    const auto *province = state::findProvince(state, provinceId);
    if (!province)
      continue;
    if (province->owner == NoPolity || !state::findPolity(state, province->owner))
      errors.push_back({year, "Province has no valid owner", provinceId, {}});
    if (province->culture < 0 || !state.cultures.contains(province->culture))
      errors.push_back({year, "Province has no valid culture", provinceId, {}});
    if (province->religion != NoReligion &&
        !state.religions.contains(province->religion))
      errors.push_back(
          {year, "Province has no valid religion", provinceId, {}});
    if (province->population < configuration.minimumPopulation)
      errors.push_back({year,
                        "Province population is below the configured minimum",
                        provinceId,
                        {}});
    ++territorySizes[province->owner];
    if (province->development < 0.0)
      errors.push_back(
          {year, "Province development is negative", provinceId, {}});
    if (province->carryingCapacity < configuration.minimumPopulation)
      errors.push_back(
          {year,
           "Province carrying capacity is below the configured minimum",
           provinceId,
           {}});
    const auto culturalPopulation = std::accumulate(
        province->culturePopulations.begin(), province->culturePopulations.end(),
        0.0,
        [](double total, const auto &entry) { return total + entry.second; });
    if (std::abs(culturalPopulation - province->population) >
        std::max(0.001, province->population * 0.001))
      errors.push_back(
          {year,
           "Cultural populations do not sum to province population",
           provinceId,
           {}});
  }
  for (PolityId polityId = 0;
       polityId < static_cast<PolityId>(state.polities.size()); ++polityId) {
    const auto *polity = state::findPolity(state, polityId);
    if (!polity || polity->dissolvedYear || territorySizes[polityId] == 0)
      continue;
    const auto *capital = state::findProvince(state, polity->capitalProvince);
    if (!capital || capital->owner != polityId)
      errors.push_back({year, "Living polity has no valid capital", {}, {}});
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
    for (ProvinceId provinceId = 0;
         provinceId < static_cast<ProvinceId>(state.provinces.size());
         ++provinceId) {
      const auto *province = state::findProvince(state, provinceId);
      if (!province)
        continue;
      const auto region = provinceRegions.find(provinceId);
      if (region == provinceRegions.end())
        continue;
      const auto [owner, inserted] =
          owners.emplace(region->second, province->owner);
      if (!inserted && owner->second != province->owner)
        errors.push_back(
            {year, "Region has split ownership", {}, region->second});
    }
  }
  return errors;
}

} // namespace Arda::Simulation::validation
