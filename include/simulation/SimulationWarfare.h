#pragma once

#include "simulation/SimulationInternal.h"

namespace Arda::Simulation::warfare {

double provinceCenterDistance(const ArdaProvince &left,
							  const ArdaProvince &right);

double polityDistance(
	PolityId left, PolityId right, const State &state,
	const std::map<ProvinceId, std::shared_ptr<ArdaProvince>> &provinces);

double maritimeRangeForYear(Year year, const Configuration &configuration);

bool hasMaritimeWarConnection(
	PolityId left, PolityId right, const State &state,
	const detail::NormalizedInput &normalized, double maritimeRange);

bool capitalsShareLandMass(
	PolityId left, PolityId right, const State &state,
	const std::map<ProvinceId, std::shared_ptr<ArdaProvince>> &provinces);

void resolveWars(
	Year nextYear, double centuries, const Configuration &configuration,
	const detail::NormalizedInput &normalized, State &state,
	const detail::AppendEvent &append, std::vector<WarEvent> &wars,
	int &nextWarId, const std::vector<Event> &events,
	const std::map<ProvinceId, std::shared_ptr<ArdaProvince>> &inputProvinces);

} // namespace Arda::Simulation::warfare
