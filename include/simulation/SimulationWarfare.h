#pragma once

#include "simulation/SimulationInternal.h"

namespace Arda::Simulation::warfare {

double provinceCenterDistance(const ArdaProvince &left,
                              const ArdaProvince &right);

double polityDistance(PolityId left, PolityId right, const State &state);

double maritimeRangeForYear(Year year, const Configuration &configuration);

double expansionBorderScore(
    ProvinceId targetProvince, PolityId attacker,
    const std::map<ProvinceId, std::vector<ProvinceId>> &neighbours,
    const std::vector<ProvinceState> &provinces);

double
expansionTargetWeakness(PolityId attacker, PolityId defender,
                        const std::map<PolityId, PolityStrength> &strengths);

bool remainsContiguousAfterConquest(
    PolityId owner, ProvinceId removedProvince,
    const std::map<ProvinceId, std::vector<ProvinceId>> &neighbours,
    const std::vector<ProvinceState> &provinces);

bool hasMaritimeWarConnection(PolityId left, PolityId right, const State &state,
                               const std::vector<std::vector<bool>> &reachability);

bool capitalsShareLandMass(PolityId left, PolityId right, const State &state);

void resolveWars(
    Year nextYear, double centuries, const Configuration &configuration,
    const detail::NormalizedInput &normalized, State &state,
    const detail::AppendEvent &append, std::vector<WarEvent> &wars,
    int &nextWarId, const std::vector<Event> &events);

} // namespace Arda::Simulation::warfare
