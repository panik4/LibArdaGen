#include "simulation/SimulationState.h"

#include <algorithm>

namespace Arda::Simulation::state {

void refreshDominantCulture(ProvinceState &province) {
  if (province.culturePopulations.empty()) {
	province.culture = -1;
	return;
  }
  province.culture = std::max_element(
						 province.culturePopulations.begin(),
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

} // namespace Arda::Simulation::state
