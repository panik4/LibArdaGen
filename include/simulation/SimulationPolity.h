#pragma once

#include "simulation/SimulationInternal.h"

namespace Arda::Simulation::polity {

void implode(Year nextYear, double centuries,
             const Configuration &configuration,
             const detail::NormalizedInput &normalized, State &state,
             const detail::AppendEvent &append, PolityId &nextPolity);

void consolidateRegions(Year nextYear, const Configuration &configuration,
                        const detail::NormalizedInput &normalized,
                        const State &state, const detail::AppendEvent &append);

void dissolveEmpty(Year nextYear, State &state,
                   const detail::AppendEvent &append);

void updatePoliticalStates(Year nextYear, double centuries,
                           const Configuration &configuration, State &state,
                           const detail::AppendEvent &append,
                           const std::vector<WarEvent> &wars,
                           GovernmentTransitionDiagnostics &diagnostics);

void captureTimelineSnapshot(Year year, const detail::NormalizedInput &normalized,
                             const State &state,
                             std::map<PolityId, std::vector<PolityTimelineSnapshot>> &timeline);

} // namespace Arda::Simulation::polity
