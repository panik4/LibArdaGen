#pragma once

#include "simulation/Simulation.h"

namespace Arda::Simulation::validation {

std::vector<ValidationError>
validateState(const State &state, const Configuration &configuration,
              const std::map<ProvinceId, RegionId> &provinceRegions, Year year,
              bool requireWholeRegions);

} // namespace Arda::Simulation::validation
