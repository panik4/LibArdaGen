#include "simulation/SimulationState.h"

#include <algorithm>

namespace Arda::Simulation {

const ProvinceState *State::findProvince(ProvinceId provinceId) const {
  const auto province = provinces.find(provinceId);
  return province == provinces.end() ? nullptr : &province->second;
}

std::vector<ProvinceId> State::territoryOf(PolityId polityId) const {
  std::vector<ProvinceId> territory;
  for (const auto &[provinceId, province] : provinces)
    if (province.owner == polityId)
      territory.push_back(provinceId);
  return territory;
}

CultureId State::dominantCultureOf(ProvinceId provinceId) const {
  const auto province = provinces.find(provinceId);
  if (province == provinces.end() ||
      province->second.culturePopulations.empty())
    return -1;
  return province->second.culture;
}

namespace state {

std::map<PolityId, std::vector<ProvinceId>>
territoriesByPolity(const State &state) {
  std::map<PolityId, std::vector<ProvinceId>> territories;
  for (const auto &[provinceId, province] : state.provinces)
    if (province.owner != NoPolity)
      territories[province.owner].push_back(provinceId);
  return territories;
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

void relocateCapital(State &state, PolityId polityId) {
  const auto polity = state.polities.find(polityId);
  if (polity == state.polities.end())
    return;
  const auto capital = state.provinces.find(polity->second.capitalProvince);
  if (capital != state.provinces.end() && capital->second.owner == polityId)
    return;
  ProvinceId replacement = NoPolity;
  for (const auto &[provinceId, province] : state.provinces) {
    if (province.owner != polityId)
      continue;
    if (replacement == NoPolity ||
        province.population > state.provinces.at(replacement).population)
      replacement = provinceId;
  }
  polity->second.capitalProvince = replacement;
}

} // namespace state
} // namespace Arda::Simulation
