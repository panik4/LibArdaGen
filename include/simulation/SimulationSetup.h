#pragma once

#include "simulation/SimulationRun.h"

namespace Arda::Simulation::setup {

bool prepareInput(
    const Configuration &configuration, const Input &input,
    std::map<ProvinceId, RegionId> &provinceRegions,
    std::map<ProvinceId, std::vector<ProvinceId>> &provinceNeighbours,
    detail::SimulationRun &run);

void initializeWorld(const Configuration &configuration,
                     detail::SimulationRun &run);

} // namespace Arda::Simulation::setup
