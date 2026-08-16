#pragma once

#include "simulation/SimulationInternal.h"

namespace Arda::Simulation::development {

double capacityEraMultiplier(const Configuration &configuration, Year year);
double phaseMultiplier(RegionalPhase phase);
double polityCapacityAt(const Configuration &configuration, Year year);
double capacityExpansionMultiplier(const Configuration &configuration,
                                   Year year, size_t territorySize);

void updateSuperRegionPhases(Year year, Year nextYear,
                             const Configuration &configuration, State &state,
                             const detail::AppendEvent &append);
void updateRegionalPhases(Year year, Year nextYear,
                          const Configuration &configuration,
                          const detail::NormalizedInput &normalized,
                          const detail::AppendEvent &append);
void updateProvinceGrowth(
    Year nextYear, double centuries, const Configuration &configuration,
    const detail::NormalizedInput &normalized, State &state,
    const std::map<ProvinceId, double> &growthPotential,
    const std::map<ProvinceId, double> &baseCapacity,
    const detail::AppendEvent &append,
    std::map<PolityId, std::vector<ProvinceId>> &territories);
void updatePolityStrengths(Year nextYear, State &state,
                           const detail::AppendEvent &append);

} // namespace Arda::Simulation::development
