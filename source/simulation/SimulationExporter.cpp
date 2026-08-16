#include "simulation/SimulationExporter.h"

#include <algorithm>
#include <map>
#include <set>
#include <utility>

namespace Arda::Simulation {
namespace {

struct ProvinceExportMetrics {
  double maximumPopulationDensity = 0.0;
  double maximumDevelopment = 0.0;
};

ProvinceExportMetrics calculateProvinceExportMetrics(
	const State &state, const std::map<ProvinceId, size_t> &provinceAreas) {
  ProvinceExportMetrics metrics;
  for (const auto &[provinceId, province] : state.provinces) {
	const auto area = provinceAreas.find(provinceId);
	const auto populationDensity =
		area != provinceAreas.end() && area->second > 0
			? province.population / static_cast<double>(area->second)
			: 0.0;
	metrics.maximumPopulationDensity =
		std::max(metrics.maximumPopulationDensity, populationDensity);
	metrics.maximumDevelopment =
		std::max(metrics.maximumDevelopment, province.development);
  }
  return metrics;
}

void exportProvinceData(
	SimulationExport &exported, const State &state,
	const std::map<ProvinceId, RegionId> &provinceRegions,
	const std::map<ProvinceId, size_t> &provinceAreas,
	const ProvinceExportMetrics &metrics) {
  for (const auto &[provinceId, province] : state.provinces) {
	const auto region = provinceRegions.find(provinceId);
	const auto regionId = region == provinceRegions.end() ? -1 : region->second;
	const auto area = provinceAreas.find(provinceId);
	const auto populationDensity =
		area != provinceAreas.end() && area->second > 0
			? province.population / static_cast<double>(area->second)
			: 0.0;
	exported.provinces.emplace(
		provinceId,
		SimulationProvinceExport{
			provinceId,
			regionId,
			province.owner,
			province.culture,
			province.religion,
			province.population,
			metrics.maximumPopulationDensity > 0.0
				? populationDensity / metrics.maximumPopulationDensity
				: 0.0,
			province.development,
			metrics.maximumDevelopment > 0.0
				? province.development / metrics.maximumDevelopment
				: 0.0});
	if (regionId >= 0)
	  exported.regions[regionId].push_back(provinceId);
  }
}

void exportActiveCivilizationLineages(SimulationExport &exported,
									  const State &state) {
  std::set<CultureId> activeCultures;
  std::set<ReligionId> activeReligions;
  for (const auto &[provinceId, province] : state.provinces) {
	if (province.culture >= 0)
	  activeCultures.insert(province.culture);
	if (province.religion != NoReligion)
	  activeReligions.insert(province.religion);
  }

  for (auto culture = exported.cultures.begin();
	   culture != exported.cultures.end();) {
	if (!activeCultures.contains(culture->first))
	  culture = exported.cultures.erase(culture);
	else
	  ++culture;
  }
  for (auto religion = exported.religions.begin();
	   religion != exported.religions.end();) {
	if (!activeReligions.contains(religion->first))
	  religion = exported.religions.erase(religion);
	else
	  ++religion;
  }
}

void exportHistoricalOwnership(
	SimulationExport &exported, const State &state,
	const std::map<ProvinceId, RegionId> &provinceRegions,
	const SimulationPolityHistory &polityHistory) {
	for (const auto &[regionId, owners] : polityHistory.historicalRegionOwners) {
	  auto &exportedOwners = exported.historicalRegionOwners[regionId];
	  exportedOwners.assign(owners.begin(), owners.end());
  }

  const auto lineageRoot = [&state](PolityId polityId) {
	std::set<PolityId> visited;
	while (polityId != NoPolity && visited.insert(polityId).second) {
	  const auto polity = state.polities.find(polityId);
	  if (polity == state.polities.end() || !polity->second.predecessorId)
		break;
	  polityId = *polity->second.predecessorId;
	}
	return polityId;
  };
  for (auto &[regionId, owners] : exported.historicalRegionOwners) {
	std::vector<PolityId> consolidatedOwners;
	for (const auto owner : owners) {
	  const auto root = lineageRoot(owner);
	  if (std::find(consolidatedOwners.begin(), consolidatedOwners.end(), root) ==
		  consolidatedOwners.end())
		consolidatedOwners.push_back(root);
	}
	owners = std::move(consolidatedOwners);
  }

	for (const auto &[polityId, provinceIds] : polityHistory.peakProvinceIds) {
	auto &peakExport = exported.historicalPolityPeaks[polityId];
	peakExport.provinceIds = provinceIds;
	for (const auto provinceId : provinceIds) {
	  const auto region = provinceRegions.find(provinceId);
	  if (region != provinceRegions.end() &&
		  std::find(peakExport.regionIds.begin(), peakExport.regionIds.end(),
					region->second) == peakExport.regionIds.end())
		peakExport.regionIds.push_back(region->second);
	}
  }
}

void exportSignificantPolities(SimulationExport &exported, const State &state) {
  double strongestPolityScore = 0.0;
  for (const auto &[polityId, strength] : state.polityStrengths)
	strongestPolityScore = std::max(strongestPolityScore, strength.score);

  constexpr double significantSizeRatio = 0.05;
  constexpr double recentCollapseSizeRatio = 0.01;
  constexpr Year recentCollapseYears = 100;
  std::set<PolityId> currentPolities;
  for (const auto &[provinceId, province] : state.provinces)
	if (province.owner != NoPolity)
	  currentPolities.insert(province.owner);

  for (const auto &[polityId, polity] : state.polities) {
	const auto strength = state.polityStrengths.find(polityId);
	const auto score =
		strength == state.polityStrengths.end() ? 0.0 : strength->second.score;
	const bool recentlyDissolved =
		polity.dissolvedYear && *polity.dissolvedYear <= state.year &&
		state.year - *polity.dissolvedYear <= recentCollapseYears;
	const double requiredRatio =
		recentlyDissolved ? recentCollapseSizeRatio : significantSizeRatio;
	const bool ownsFinalProvince = currentPolities.contains(polityId);
	if (!ownsFinalProvince && (strongestPolityScore <= 0.0 ||
							   score < strongestPolityScore * requiredRatio))
	  continue;

	SimulationPolityExport polityExport;
	polityExport.polity = polity;
	if (strength != state.polityStrengths.end())
	  polityExport.strength = strength->second;
	for (const auto &[provinceId, province] : exported.provinces) {
	  if (province.polityId != polityId)
		continue;
	  polityExport.provinceIds.push_back(provinceId);
	  if (province.regionId >= 0 &&
		  std::find(polityExport.regionIds.begin(), polityExport.regionIds.end(),
					province.regionId) == polityExport.regionIds.end())
		polityExport.regionIds.push_back(province.regionId);
	}
	exported.polities.emplace(polityId, std::move(polityExport));
  }
}

} // namespace

SimulationExport SimulationExporter::exportFinalState(
	const State &state, const std::map<ProvinceId, RegionId> &provinceRegions,
	const SimulationPolityHistory &polityHistory,
	const std::map<ProvinceId, size_t> &provinceAreas) {
  SimulationExport exported;
  exported.year = state.year;
  exported.cultures = state.cultures;
  exported.religions = state.religions;
  exportActiveCivilizationLineages(exported, state);

  const auto metrics = calculateProvinceExportMetrics(state, provinceAreas);
  exportProvinceData(exported, state, provinceRegions, provinceAreas, metrics);
	exportHistoricalOwnership(exported, state, provinceRegions, polityHistory);
  exportSignificantPolities(exported, state);
  return exported;
}

} // namespace Arda::Simulation
