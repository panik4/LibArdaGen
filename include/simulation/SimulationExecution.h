#pragma once

#include "simulation/SimulationRun.h"

namespace Arda::Simulation::execution {

void simulateYears(const Configuration &configuration,
                   detail::SimulationRun &run);

void finalize(const Configuration &configuration,
              const std::map<ProvinceId, RegionId> &provinceRegions,
              detail::SimulationRun &run);

} // namespace Arda::Simulation::execution
