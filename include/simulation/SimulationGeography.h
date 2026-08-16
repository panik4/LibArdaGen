#pragma once

#include "simulation/SimulationInternal.h"

namespace Arda::Simulation::geography {

SeaRouteMap buildWeightedSeaRoutes(
    const std::vector<std::shared_ptr<ArdaContinent>> &continents,
    const Fwg::Terrain::TerrainData *terrainData);

detail::NormalizedInput normalize(const Input &input);

} // namespace Arda::Simulation::geography
