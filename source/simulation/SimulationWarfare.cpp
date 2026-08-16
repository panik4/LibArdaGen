#include "simulation/SimulationWarfare.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Arda::Simulation::warfare {
namespace {

bool hasMaritimeWarConnectionOneWay(
	PolityId left, PolityId right, const State &state,
	const detail::NormalizedInput &normalized, double maritimeRange) {
  for (const auto &[sourceId, routes] : normalized.seaRoutesFrom) {
	const auto source = state.provinces.find(sourceId);
	if (source == state.provinces.end() || source->second.owner != left ||
		!source->second.coastal)
	  continue;
	for (const auto &route : routes) {
	  if (route.distance > maritimeRange)
		break;
	  const auto target = state.provinces.find(route.provinceId);
	  if (target != state.provinces.end() && target->second.owner == right)
		return true;
	}
  }
  return false;
}

} // namespace

double provinceCenterDistance(const ArdaProvince &left,
							  const ArdaProvince &right) {
  const auto dx = static_cast<double>(left.position.widthCenter -
									  right.position.widthCenter);
  const auto dy = static_cast<double>(left.position.heightCenter -
									  right.position.heightCenter);
  return std::sqrt(dx * dx + dy * dy);
}

double polityDistance(
	PolityId left, PolityId right, const State &state,
	const std::map<ProvinceId, std::shared_ptr<ArdaProvince>> &provinces) {
  const auto leftPolity = state.polities.find(left);
  const auto rightPolity = state.polities.find(right);
  if (leftPolity == state.polities.end() ||
	  rightPolity == state.polities.end() ||
	  leftPolity->second.capitalProvince < 0 ||
	  rightPolity->second.capitalProvince < 0)
	return std::numeric_limits<double>::max();

  const auto leftProvince = provinces.find(leftPolity->second.capitalProvince);
  const auto rightProvince =
	  provinces.find(rightPolity->second.capitalProvince);
  if (leftProvince == provinces.end() || rightProvince == provinces.end() ||
	  !leftProvince->second || !rightProvince->second)
	return std::numeric_limits<double>::max();

  return provinceCenterDistance(*leftProvince->second, *rightProvince->second);
}

double maritimeRangeForYear(const Year year,
							const Configuration &configuration) {
  if (year < configuration.maritimeExpansionStartYear)
	return 0.0;
  const auto centuriesSinceStart = std::max(
	  0.0,
	  static_cast<double>(year - configuration.maritimeExpansionStartYear) /
		  100.0);
  const auto preRenaissanceRange =
	  configuration.initialMaritimeRange *
	  std::pow(1.0 + configuration.maritimeRangeGrowthPerCentury,
			   centuriesSinceStart);
  if (year < configuration.renaissanceStartYear)
	return std::max(1.0, preRenaissanceRange);
  const auto renaissanceYears = std::max(
	  0.0, static_cast<double>(year - configuration.renaissanceStartYear) /
			   100.0);
  return std::max(
	  1.0,
	  preRenaissanceRange * configuration.renaissanceMaritimeRangeMultiplier *
		  std::pow(1.0 + configuration.renaissanceMaritimeRangeGrowthPerCentury,
				   renaissanceYears));
}

bool hasMaritimeWarConnection(PolityId left, PolityId right,
							  const State &state,
							  const detail::NormalizedInput &normalized,
							  double maritimeRange) {
  return hasMaritimeWarConnectionOneWay(left, right, state, normalized,
										 maritimeRange) ||
		 hasMaritimeWarConnectionOneWay(right, left, state, normalized,
										 maritimeRange);
}

bool capitalsShareLandMass(
	PolityId left, PolityId right, const State &state,
	const std::map<ProvinceId, std::shared_ptr<ArdaProvince>> &provinces) {
  const auto leftPolity = state.polities.find(left);
  const auto rightPolity = state.polities.find(right);
  if (leftPolity == state.polities.end() ||
	  rightPolity == state.polities.end())
	return false;

  const auto leftProvince = provinces.find(leftPolity->second.capitalProvince);
  const auto rightProvince =
	  provinces.find(rightPolity->second.capitalProvince);
  if (leftProvince == provinces.end() || rightProvince == provinces.end() ||
	  !leftProvince->second || !rightProvince->second ||
	  leftProvince->second->landMassID < 0 ||
	  rightProvince->second->landMassID < 0)
	return false;

  return leftProvince->second->landMassID == rightProvince->second->landMassID;
}

} // namespace Arda::Simulation::warfare
