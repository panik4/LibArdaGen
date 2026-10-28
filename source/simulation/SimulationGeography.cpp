#include "simulation/SimulationGeography.h"

#include "civilisation/CivilisationLayer.h"
#include "utils/Cfg.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>
#include <sstream>
#include <type_traits>
#include <unordered_map>

namespace Arda::Simulation::geography {
namespace {

bool isEligible(const std::shared_ptr<ArdaProvince> &province) {
  using TopographyType =
      std::remove_cvref_t<decltype(*province->topographyTypes.begin())>;
  return province && !province->isSea() && !province->isLake() &&
         !province->topographyTypes.contains(TopographyType::WASTELAND);
}

} // namespace

SeaRouteMap buildWeightedSeaRoutes(
    const std::vector<std::shared_ptr<ArdaContinent>> &continents,
    const Fwg::Terrain::TerrainData *terrainData) {
  std::map<ProvinceId, std::shared_ptr<ArdaProvince>> provinces;
  std::queue<std::shared_ptr<ArdaProvince>> pending;
  for (const auto &continent : continents) {
    if (!continent)
      continue;
    for (const auto &province : continent->ardaProvinces) {
      if (province && provinces.emplace(province->ID, province).second)
        pending.push(province);
    }
  }
  while (!pending.empty()) {
    const auto province = pending.front();
    pending.pop();
    for (const auto &baseNeighbour : province->provinceNeighbours) {
      const auto neighbour =
          std::dynamic_pointer_cast<ArdaProvince>(baseNeighbour);
      if (neighbour && provinces.emplace(neighbour->ID, neighbour).second)
        pending.push(neighbour);
    }
  }

  std::map<ProvinceId, double> depths;
  const auto seaLevel =
      static_cast<double>(Fwg::Cfg::Values().terrainConfig.seaLevel);
  for (const auto &[provinceId, province] : provinces) {
    double depth = 0.0;
    size_t samples = 0;
    if (terrainData) {
      for (const auto pixel : province->pixels) {
        if (pixel < 0 ||
            pixel >= static_cast<int>(terrainData->detailedHeightMap.size()))
          continue;
        depth += std::max(0.0, seaLevel -
                                   static_cast<double>(
                                       terrainData->detailedHeightMap[pixel]));
        ++samples;
      }
    }
    depths[provinceId] = samples == 0 ? 0.0 : depth / samples;
  }

  std::set<ProvinceId> coastalEndpoints;
  for (const auto &[provinceId, province] : provinces)
    if (isEligible(province) && province->isCoastalToOcean())
      coastalEndpoints.insert(provinceId);
  if (coastalEndpoints.empty())
    return {};

  // Compact indices follow province-ID order, preserving priority-queue ties.
  // Edge eligibility and weights are independent of the coastal source.
  struct Edge {
    size_t target;
    double cost;
  };
  std::map<ProvinceId, size_t> nodeIndices;
  std::vector<uint8_t> seaNodes;
  seaNodes.reserve(provinces.size());
  for (const auto &[provinceId, province] : provinces) {
    nodeIndices.emplace(provinceId, nodeIndices.size());
    seaNodes.push_back(province->isSea());
  }
  std::vector<std::vector<Edge>> edges(provinces.size());
  for (const auto &[provinceId, current] : provinces) {
    if (!current->isSea() && !coastalEndpoints.contains(provinceId))
      continue;
    auto &outgoing = edges[nodeIndices.at(provinceId)];
    outgoing.reserve(current->provinceNeighbours.size());
    for (const auto &neighbour : current->provinceNeighbours) {
      if (!neighbour || neighbour->isLake() ||
          (!neighbour->isSea() && !coastalEndpoints.contains(neighbour->ID)))
        continue;
      const auto neighbourId = neighbour->ID;
      if (!provinces.contains(neighbourId))
        continue;
      const auto dx = static_cast<double>(current->position.widthCenter -
                                          neighbour->position.widthCenter);
      const auto dy = static_cast<double>(current->position.heightCenter -
                                          neighbour->position.heightCenter);
      const auto transitionDistance = std::sqrt(dx * dx + dy * dy);
      const auto depthIt = depths.find(neighbourId);
      const auto normalizedDepth = seaLevel > 0.0 && depthIt != depths.end()
                                       ? depthIt->second / seaLevel
                                       : 0.0;
      const auto currentDepthIt = depths.find(provinceId);
      const auto currentNormalizedDepth =
          seaLevel > 0.0 && currentDepthIt != depths.end()
              ? currentDepthIt->second / seaLevel
              : 0.0;
      const auto currentMultiplier =
          current->isSea() ? 1.0 + currentNormalizedDepth : 1.0;
      const auto neighbourMultiplier =
          neighbour->isSea() ? 1.0 + normalizedDepth : 1.0;
      const auto cost =
          transitionDistance * std::max(currentMultiplier, neighbourMultiplier);
      outgoing.push_back({nodeIndices.at(neighbourId), cost});
    }
  }

  std::vector<std::pair<ProvinceId, size_t>> endpoints;
  endpoints.reserve(coastalEndpoints.size());
  for (const auto id : coastalEndpoints)
    endpoints.emplace_back(id, nodeIndices.at(id));
  std::vector<double> distances(provinces.size());
  std::vector<uint8_t> reached(provinces.size());
  SeaRouteMap routes;
  for (const auto &[sourceId, sourceIndex] : endpoints) {
    std::fill(reached.begin(), reached.end(), 0);
    std::priority_queue<std::pair<double, size_t>,
                        std::vector<std::pair<double, size_t>>, std::greater<>>
        queue;
    distances[sourceIndex] = 0.0;
    reached[sourceIndex] = 1;
    queue.push({0.0, sourceIndex});
    while (!queue.empty()) {
      const auto [distance, index] = queue.top();
      queue.pop();
      if (distance > distances[index])
        continue;
      // Other coastal land provinces remain endpoints, never transit nodes.
      if (index != sourceIndex && !seaNodes[index])
        continue;
      for (const auto &edge : edges[index]) {
        const auto candidateDistance = distance + edge.cost;
        if (reached[edge.target] && distances[edge.target] <= candidateDistance)
          continue;
        distances[edge.target] = candidateDistance;
        reached[edge.target] = 1;
        queue.push({candidateDistance, edge.target});
      }
    }
    auto &sourceRoutes = routes[sourceId];
    for (const auto &[targetId, targetIndex] : endpoints) {
      if (targetId != sourceId && reached[targetIndex])
        sourceRoutes.push_back({targetId, distances[targetIndex]});
    }
    std::sort(sourceRoutes.begin(), sourceRoutes.end(),
              [](const auto &left, const auto &right) {
                if (left.distance != right.distance)
                  return left.distance < right.distance;
                return left.provinceId < right.provinceId;
              });
  }
  return routes;
}

detail::NormalizedInput normalize(const Input &input, bool buildSeaRoutes) {
  const auto geographyStart = std::chrono::steady_clock::now();
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
         "Climate input must provide habitability and arable-land data",
         {},
         {}});
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
        if (input.climateData &&
            index < input.climateData->habitabilities.size())
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

  if (!buildSeaRoutes)
    return normalized;

  Fwg::Utils::Logging::logLine(
      "History setup timing: geography normalization = ",
      std::chrono::duration<double, std::milli>(
          std::chrono::steady_clock::now() - geographyStart)
          .count(),
      " ms");
  Fwg::Utils::Logging::logLine("History setup: building maritime routes");
  const auto routesStart = std::chrono::steady_clock::now();
  auto weightedRoutes =
      buildWeightedSeaRoutes(input.continents, input.terrainData);
  size_t weightedRouteCount = 0;
  double weightedMinimumDistance = std::numeric_limits<double>::max();
  double weightedMaximumDistance = 0.0;
  for (const auto &[sourceId, routes] : weightedRoutes) {
    weightedRouteCount += routes.size();
    for (const auto &route : routes) {
      weightedMinimumDistance =
          std::min(weightedMinimumDistance, route.distance);
      weightedMaximumDistance =
          std::max(weightedMaximumDistance, route.distance);
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
  Fwg::Utils::Logging::logLine(
      "History setup timing: maritime route construction = ",
      std::chrono::duration<double, std::milli>(
          std::chrono::steady_clock::now() - routesStart)
          .count(),
      " ms");
  const auto routeNormalizationStart = std::chrono::steady_clock::now();
  size_t normalizedRouteCount = 0;
  size_t normalizedCrossLandmassRoutes = 0;
  struct RouteTarget {
    int landMassID;
    std::vector<SeaRoute> *incoming = nullptr;
  };
  std::unordered_map<ProvinceId, RouteTarget> routeTargets;
  routeTargets.reserve(normalized.provinces.size());
  for (const auto &[provinceId, province] : normalized.provinces)
    if (province && normalized.environments.contains(provinceId))
      routeTargets.emplace(provinceId, RouteTarget{province->landMassID});
  for (auto &[sourceId, routes] : weightedRoutes) {
    const auto sourceProvince = normalized.provinces.find(sourceId);
    if (sourceProvince == normalized.provinces.end() || !sourceProvince->second)
      continue;
    size_t retained = 0;
    for (const auto &route : routes) {
      const auto target = routeTargets.find(route.provinceId);
      if (target == routeTargets.end())
        continue;
      if (!target->second.incoming)
        target->second.incoming = &normalized.seaRoutesTo[route.provinceId];
      target->second.incoming->push_back({sourceId, route.distance});
      routes[retained++] = route;
      ++normalizedRouteCount;
      if (sourceProvince->second->landMassID >= 0 &&
          target->second.landMassID >= 0 &&
          sourceProvince->second->landMassID != target->second.landMassID)
        ++normalizedCrossLandmassRoutes;
    }
    routes.resize(retained);
    if (!routes.empty())
      normalized.seaRoutesFrom.emplace(sourceId, std::move(routes));
  }
  for (auto &[targetId, routes] : normalized.seaRoutesTo)
    std::sort(routes.begin(), routes.end(),
              [](const auto &left, const auto &right) {
                if (left.distance != right.distance)
                  return left.distance < right.distance;
                return left.provinceId < right.provinceId;
              });
  {
    std::ostringstream message;
    message << "Maritime routes normalized: sources="
            << normalized.seaRoutesFrom.size()
            << " targets=" << normalized.seaRoutesTo.size()
            << " routes=" << normalizedRouteCount
            << " crossLandmassRoutes=" << normalizedCrossLandmassRoutes;
    Fwg::Utils::Logging::logLine(message.str());
  }
  Fwg::Utils::Logging::logLine(
      "History setup timing: maritime route normalization = ",
      std::chrono::duration<double, std::milli>(
          std::chrono::steady_clock::now() - routeNormalizationStart)
          .count(),
      " ms");

  return normalized;
}

} // namespace Arda::Simulation::geography
