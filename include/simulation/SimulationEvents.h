#pragma once

#include "simulation/Simulation.h"

namespace Arda::Simulation::events {

void apply(State &state, const EventStore &events, size_t index);

} // namespace Arda::Simulation::events
