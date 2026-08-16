#pragma once

#include "simulation/Simulation.h"

namespace Arda::Simulation::state {

void refreshDominantCulture(ProvinceState &province);
void relocateCapital(State &state, PolityId polityId);

} // namespace Arda::Simulation::state
