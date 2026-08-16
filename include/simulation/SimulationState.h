#pragma once

#include "simulation/Simulation.h"

namespace Arda::Simulation::state {

const ProvinceState *findProvince(const State &state, ProvinceId provinceId);
ProvinceState *findProvince(State &state, ProvinceId provinceId);

const Polity *findPolity(const State &state, PolityId polityId);
Polity *findPolity(State &state, PolityId polityId);

ProvinceState &ensureProvince(State &state, ProvinceId provinceId);
Polity &ensurePolity(State &state, PolityId polityId);

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
