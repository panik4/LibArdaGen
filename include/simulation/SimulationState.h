#pragma once

#include "simulation/Simulation.h"

namespace Arda::Simulation::state {

std::map<PolityId, std::vector<ProvinceId>>
territoriesByPolity(const State &state);

void refreshDominantCulture(ProvinceState &province);
void relocateCapital(State &state, PolityId polityId);

} // namespace Arda::Simulation::state
