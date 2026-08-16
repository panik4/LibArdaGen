#pragma once

#include "simulation/Simulation.h"

namespace Arda::Simulation::state {

std::map<PolityId, std::vector<ProvinceId>>
territoriesByPolity(const State &state);

const std::set<ProvinceId> &coastalProvincesOf(const State &state,
											   PolityId polityId);

void setCapital(State &state, PolityId polityId, ProvinceId provinceId);

const std::shared_ptr<ArdaProvince> &capitalOf(const State &state,
												PolityId polityId);

void refreshDominantCulture(ProvinceState &province);
void relocateCapital(State &state, PolityId polityId);

} // namespace Arda::Simulation::state
