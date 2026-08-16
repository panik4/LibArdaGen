#pragma once

#include "simulation/SimulationInternal.h"

namespace Arda::Simulation::culture {

void integrate(Year nextYear, double centuries, const Configuration &configuration,
			   State &state, const detail::AppendEvent &append);

void evolveAndReligions(Year nextYear, double centuries,
						const Configuration &configuration,
						const detail::NormalizedInput &normalized, State &state,
						const detail::AppendEvent &append, CultureId &nextCulture,
						ReligionId &nextReligion);

} // namespace Arda::Simulation::culture
