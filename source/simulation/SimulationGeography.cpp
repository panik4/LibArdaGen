#include "simulation/SimulationGeography.h"

#include "civilisation/CivilisationLayer.h"
#include "utils/Cfg.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>
#include <sstream>
#include <type_traits>

namespace Arda::Simulation::geography {
namespace {

bool isEligible(const std::shared_ptr<ArdaProvince> &province) {
	using TopographyType = std::remove_cvref_t<
	  decltype(*province->topographyTypes.begin())>;
  return province && !province->isSea() && !province->isLake() &&
		  !province->topographyTypes.contains(
			  TopographyType::WASTELAND);
}

}

SeaRouteMap buildWeightedSeaRoutes(
	const std::vector<std::shared_ptr<ArdaContinent>> &continents,
	const Fwg::Terrain::TerrainData *terrainData) {
  std::map<ProvinceId, std::shared_ptr<ArdaProvince>> provinces;
  for (const auto &continent : continents)
	if (continent)
	  for (const auto &province : continent->ardaProvinces)
		if (province)
		  provinces.emplace(province->ID, province);

  std::map<ProvinceId, double> depths;
  const auto seaLevel = static_cast<double>(Fwg::Cfg::Values().seaLevel);
  for (const auto &[provinceId, province] : provinces) {
	double depth = 0.0;
	size_t samples = 0;
	if (terrainData) {
	  for (const auto pixel : province->pixels) {
		if (pixel < 0 ||
			pixel >= static_cast<int>(terrainData->detailedHeightMap.size()))
		  continue;
		depth += std::max(
			0.0, seaLevel - static_cast<double>(
								  terrainData->detailedHeightMap[pixel]));
		++samples;
	  }
	}
	depths[provinceId] = samples == 0 ? 0.0 : depth / samples;
  }

  SeaRouteMap routes;
  for (const auto &[sourceId, source] : provinces) {
	if (!source->isCoastalToOcean())
	  continue;
	std::priority_queue<std::pair<double, ProvinceId>,
						std::vector<std::pair<double, ProvinceId>>,
						std::greater<>>
		queue;
	std::map<ProvinceId, double> distances;
	distances[sourceId] = 0.0;
	queue.push({0.0, sourceId});
	while (!queue.empty()) {
	  const auto [distance, provinceId] = queue.top();
	  queue.pop();
	  if (distance > distances.at(provinceId))
		continue;
	  const auto currentIt = provinces.find(provinceId);
	  if (currentIt == provinces.end())
		continue;
	  const auto &current = currentIt->second;
	  for (const auto &neighbour : current->provinceNeighbours) {
		if (!neighbour || neighbour->isLake() ||
			(!neighbour->isSea() && !neighbour->isCoastalToOcean()))
		  continue;
		const auto neighbourId = neighbour->ID;
		const auto neighbourIt = provinces.find(neighbourId);
		if (neighbourIt == provinces.end())
		  continue;
		const auto dx = static_cast<double>(
			current->position.widthCenter - neighbour->position.widthCenter);
		const auto dy = static_cast<double>(
			current->position.heightCenter - neighbour->position.heightCenter);
		const auto transitionDistance = std::sqrt(dx * dx + dy * dy);
		const auto depthIt = depths.find(neighbourId);
		const auto normalizedDepth =
			seaLevel > 0.0 && depthIt != depths.end()
				? depthIt->second / seaLevel
				: 0.0;
		const auto cost = transitionDistance *
						  (neighbour->isSea() ? 1.0 + normalizedDepth : 1.0);
		const auto candidateDistance = distance + cost;
		if (const auto existing = distances.find(neighbourId);
			existing != distances.end() &&
			existing->second <= candidateDistance)
		  continue;
		distances[neighbourId] = candidateDistance;
		queue.push({candidateDistance, neighbourId});
	  }
	}
	for (const auto &[targetId, distance] : distances) {
	  const auto targetIt = provinces.find(targetId);
	  if (targetIt == provinces.end())
		continue;
	  const auto &target = targetIt->second;
	  if (targetId != sourceId && target->isCoastalToOcean() &&
		  !target->isSea())
		routes[sourceId].push_back({targetId, distance});
	}
	std::sort(routes[sourceId].begin(), routes[sourceId].end(),
			  [](const auto &left, const auto &right) {
				if (left.distance != right.distance)
				  return left.distance < right.distance;
				return left.provinceId < right.provinceId;
			  });
  }
  return routes;
}

detail::NormalizedInput normalize(const Input &input) {
  detail::NormalizedInput normalized;
  std::map<ProvinceId, RegionId> allProvinceRegions;

  for (const auto &continent : input.continents) {
	if (!continent) {
	  normalized.errors.push_back(
		  {StartYear, "Input contains a null continent", {}, {}});
	  continue;
	}
	for (const auto &ardaRegion : continent->ardaRegions) {
	  if (!ardaRegion) {
		normalized.errors.push_back(
			{StartYear, "Input contains a null region", {}, {}});
		continue;
	  }
	  normalized.regionContinents[ardaRegion->ID] = continent->ID;
	  if (ardaRegion->areaSubType == Fwg::Areas::AreaSubType::Island ||
		  ardaRegion->areaSubType == Fwg::Areas::AreaSubType::CoastalIsland ||
		  ardaRegion->areaSubType == Fwg::Areas::AreaSubType::LakeIsland)
		normalized.islandRegions.insert(ardaRegion->ID);
	  for (const auto &neighbour : ardaRegion->neighbourRegions)
		if (neighbour)
		  normalized.regionNeighbours[ardaRegion->ID].push_back(neighbour->ID);
	  for (const auto &province : ardaRegion->ardaProvinces) {
		if (!province) {
		  normalized.errors.push_back({StartYear,
									   "Region contains a null province",
									   {},
									   ardaRegion->ID});
		  continue;
		}
		const auto [it, inserted] =
			allProvinceRegions.emplace(province->ID, ardaRegion->ID);
		if (!inserted && it->second != ardaRegion->ID) {
		  normalized.errors.push_back({StartYear,
									   "Province belongs to multiple regions",
									   province->ID, ardaRegion->ID});
		  continue;
		}
		if (!isEligible(province))
		  continue;
		const auto [provinceIt, provinceInserted] =
			normalized.provinces.emplace(province->ID, province);
		if (!provinceInserted && provinceIt->second != province) {
		  normalized.errors.push_back({StartYear,
									   "Input contains duplicate province IDs",
									   province->ID, ardaRegion->ID});
		  continue;
		}
		normalized.provinceRegions[province->ID] = ardaRegion->ID;
		normalized.provinceContinents[province->ID] = continent->ID;
		normalized.regions[ardaRegion->ID].push_back(province->ID);
	  }
	}
  }

  for (auto &[regionId, provinceIds] : normalized.regions)
	std::sort(provinceIds.begin(), provinceIds.end());
  if (input.climateData && (input.climateData->habitabilities.empty() ||
							input.climateData->arableLand.empty()))
	normalized.errors.push_back(
		{StartYear,
		 "Climate input must provide habitability and arable-land data", {}, {}});
  if (input.terrainData && input.terrainData->inclination.empty())
	normalized.errors.push_back(
		{StartYear, "Terrain input must provide inclination data", {}, {}});

  for (const auto &[provinceId, province] : normalized.provinces) {
	auto &neighbours = normalized.neighbours[provinceId];
	for (const auto &baseNeighbour : province->provinceNeighbours) {
	  auto neighbour = std::dynamic_pointer_cast<ArdaProvince>(baseNeighbour);
	  if (neighbour && normalized.provinces.contains(neighbour->ID))
		neighbours.push_back(neighbour->ID);
	}
	std::sort(neighbours.begin(), neighbours.end());
	neighbours.erase(std::unique(neighbours.begin(), neighbours.end()),
					 neighbours.end());
	detail::NormalizedInput::Environment environment;
	environment.coastal = province->isCoastalToOcean();
	const auto regionId = normalized.provinceRegions.at(provinceId);
	const auto regionIsIsland = normalized.islandRegions.contains(regionId);
	const auto provinceIsIslandSubtype =
		province->areaSubType == Fwg::Areas::AreaSubType::Island ||
		province->areaSubType == Fwg::Areas::AreaSubType::CoastalIsland ||
		province->areaSubType == Fwg::Areas::AreaSubType::LakeIsland;
	environment.island = regionIsIsland || provinceIsIslandSubtype;
	environment.area = province->pixels.size();
	if (input.climateData || input.terrainData) {
	  double habitability = 0.0;
	  double arableLand = 0.0;
	  double inclination = 0.0;
	  size_t samples = 0;
	  for (const auto pixel : province->pixels) {
		if (pixel < 0)
		  continue;
		const auto index = static_cast<size_t>(pixel);
		if (input.climateData && index < input.climateData->habitabilities.size())
		  habitability += input.climateData->habitabilities[index];
		if (input.climateData && index < input.climateData->arableLand.size())
		  arableLand += input.climateData->arableLand[index];
		if (input.terrainData && index < input.terrainData->inclination.size())
		  inclination += input.terrainData->inclination[index];
		++samples;
	  }
	  if (samples > 0) {
		environment.habitability = std::clamp(habitability / samples, 0.0, 1.0);
		environment.arableLand = std::clamp(arableLand / samples, 0.0, 1.0);
		environment.inclination = std::max(0.0, inclination / samples);
	  }
	}
	if (!input.climateData && province->habitability > 0.0f)
	  environment.habitability =
		  std::clamp(static_cast<double>(province->habitability), 0.0, 1.0);
	normalized.environments[provinceId] = environment;
  }

  const auto weightedRoutes =
	  buildWeightedSeaRoutes(input.continents, input.terrainData);
  size_t weightedRouteCount = 0;
  double weightedMinimumDistance = std::numeric_limits<double>::max();
  double weightedMaximumDistance = 0.0;
  for (const auto &[sourceId, routes] : weightedRoutes) {
	weightedRouteCount += routes.size();
	for (const auto &route : routes) {
	  weightedMinimumDistance =
		  std::min(weightedMinimumDistance, route.distance);
	  weightedMaximumDistance = std::max(weightedMaximumDistance, route.distance);
	}
  }
  {
	std::ostringstream message;
	message << "Maritime routes built: sources=" << weightedRoutes.size()
			<< " routes=" << weightedRouteCount << " minDistance="
			<< (weightedRouteCount == 0 ? 0.0 : weightedMinimumDistance)
			<< " maxDistance=" << weightedMaximumDistance;
	Fwg::Utils::Logging::logLine(message.str());
  }
  size_t normalizedRouteCount = 0;
  size_t normalizedCrossLandmassRoutes = 0;
  for (const auto &[sourceId, routes] : weightedRoutes) {
	const auto sourceProvince = normalized.provinces.find(sourceId);
	if (sourceProvince == normalized.provinces.end() || !sourceProvince->second)
	  continue;
	for (const auto &route : routes) {
	  const auto environment = normalized.environments.find(route.provinceId);
	  const auto targetProvince = normalized.provinces.find(route.provinceId);
	  if (environment == normalized.environments.end() ||
		  targetProvince == normalized.provinces.end() || !targetProvince->second)
		continue;
	  normalized.seaRoutesFrom[sourceId].push_back(route);
	  normalized.seaRoutesTo[route.provinceId].push_back(
		  {sourceId, route.distance});
	  ++normalizedRouteCount;
	  if (sourceProvince->second->landMassID >= 0 &&
		  targetProvince->second->landMassID >= 0 &&
		  sourceProvince->second->landMassID != targetProvince->second->landMassID)
		++normalizedCrossLandmassRoutes;
	}
  }
  {
	std::ostringstream message;
	message << "Maritime routes normalized: sources="
			<< normalized.seaRoutesFrom.size() << " targets="
			<< normalized.seaRoutesTo.size() << " routes=" << normalizedRouteCount
			<< " crossLandmassRoutes=" << normalizedCrossLandmassRoutes;
	Fwg::Utils::Logging::logLine(message.str());
  }

  return normalized;
}

} // namespace Arda::Simulation::geography
