#pragma once

#include "simulation/SimulationInternal.h"

namespace Arda::Simulation::expansion {

void colonize(Year nextYear, double centuries,
              const Configuration &configuration,
              const detail::NormalizedInput &normalized, State &state,
              const detail::AppendEvent &append, PolityId strongestPolity);

} // namespace Arda::Simulation::expansion
