#include "areas/AreaGen.h"
namespace Arda::Areas {
void generateStrategicRegions(
    std::function<std::shared_ptr<SuperRegion>()> factory,
    std::vector<std::shared_ptr<SuperRegion>> &superRegions,
    std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions, const float& superRegionFactor) {
  Fwg::Utils::Logging::logLine(
      "Scenario: Dividing world into strategic regions");
  superRegions.clear();
  const auto &config = Fwg::Cfg::Values();

  std::vector<int> waterAreaPixels;
  std::vector<int> landAreaPixels;
  for (auto &region : ardaRegions) {
    // as per types, group the regions. Land and lake together, while ocean
    // and islands together. MixedLand is landArea sum up all the province
    // pixels of the region in one vector
    region->pixels = region->gatherPixels();
    if (region->type == Arda::ArdaRegion::RegionType::Ocean ||
        region->type == Arda::ArdaRegion::RegionType::OceanCoastal ||
        region->type == Arda::ArdaRegion::RegionType::OceanIslandCoastal ||
        region->type == Arda::ArdaRegion::RegionType::OceanMixedCoastal ||
        region->type == Arda::ArdaRegion::RegionType::CoastalIsland ||
        region->type == Arda::ArdaRegion::RegionType::Island ||
        region->type == Arda::ArdaRegion::RegionType::IslandLake) {
      for (auto &province : region->provinces) {
        waterAreaPixels.insert(waterAreaPixels.end(), region->pixels.begin(),
                               region->pixels.end());
      }
    } else {
      for (auto &province : region->provinces) {
        landAreaPixels.insert(landAreaPixels.end(), region->pixels.begin(),
                              region->pixels.end());
      }
    }
  }
  auto landShare = static_cast<double>(landAreaPixels.size()) /
                   (landAreaPixels.size() + waterAreaPixels.size());
  Fwg::Utils::Logging::logLine("Land share: ", landShare);
  auto waterShare = static_cast<double>(waterAreaPixels.size()) /
                    (landAreaPixels.size() + waterAreaPixels.size());
  Fwg::Utils::Logging::logLine("Water share: ", waterShare);
  if (landShare + waterShare != 1.0) {
    Fwg::Utils::Logging::logLine(
        "Error: Land and water share do not add up to 1.0");
  }
  // calculate the amount of strategic regions we want to have
  int landSuperRegions =
      static_cast<int>(landShare * 110.0 * 2.0 * superRegionFactor);
  int waterSuperRegions =
      static_cast<int>(waterShare * 110.0 * 1.0 * superRegionFactor);

  int landMinDist = Fwg::Utils::computePoissonMinDistFromArea(
      landAreaPixels.size(), landSuperRegions, config.width, 8.0);
  int waterMinDist = Fwg::Utils::computePoissonMinDistFromArea(
      waterAreaPixels.size(), waterSuperRegions, config.width, 8.0);

  // Launch parallel Poisson disk generation
  auto waterFuture = std::async(std::launch::async, [&]() {
    return Fwg::Utils::generatePoissonDiskPoints(
        waterAreaPixels, config.width, waterSuperRegions, waterMinDist);
  });

  auto landFuture = std::async(std::launch::async, [&]() {
    return Fwg::Utils::generatePoissonDiskPoints(landAreaPixels, config.width,
                                                 landSuperRegions, landMinDist);
  });

  // Wait and retrieve the results
  auto waterPoints = waterFuture.get();
  auto landPoints = landFuture.get();

  std::vector<int> validLandSeeds;
  auto landVoronois = Fwg::Utils::growRegionsMultiSourceClusters(
      landAreaPixels, landPoints, config.width, config.height,
      /*wrapX=*/false,
      /*fillIslands=*/true, &validLandSeeds);

  std::vector<int> validWaterSeeds;
  auto waterVoronois = Fwg::Utils::growRegionsMultiSourceClusters(
      waterAreaPixels, waterPoints, config.width, config.height,
      /*wrapX=*/false,
      /*fillIslands=*/true, &validWaterSeeds);

  if (config.debugLevel > 5) {
    // debug visualise landVoronoi
    Fwg::Gfx::Bitmap landVoronoiBmp(config.width, config.height, 24);
    for (auto &landvor : landVoronois) {
      Fwg::Gfx::Colour c;
      c.randomize();
      for (const auto &pix : landvor) {
        landVoronoiBmp.setColourAtIndex(pix, c);
      }
    }
    Fwg::Gfx::Png::save(landVoronoiBmp,
                        config.mapsPath + "debug//landVoronoi.png", false);
    //  debug visualise waterVoronoi
    Fwg::Gfx::Bitmap waterVoronoiBmp(config.width, config.height, 24);
    for (auto &watervor : waterVoronois) {
      Fwg::Gfx::Colour c;
      c.randomize();
      for (const auto &pix : watervor) {
        waterVoronoiBmp.setColourAtIndex(pix, c);
      }
    }
    Fwg::Gfx::Png::save(waterVoronoiBmp,
                        config.mapsPath + "debug//waterVoronoi.png", false);
  }

  std::vector<int> indexToVoronoiID(config.bitmapSize);
  for (int i = 0; i < landVoronois.size(); ++i) {
    for (const auto &pix : landVoronois[i]) {
      indexToVoronoiID[pix] = i;
    }
  }
  for (int i = 0; i < waterVoronois.size(); ++i) {
    for (const auto &pix : waterVoronois[i]) {
      indexToVoronoiID[pix] = i + landVoronois.size();
    }
  }
  for (auto &landVor : landVoronois) {
    auto superRegion = factory();
    superRegion->ID = superRegions.size();
    superRegion->areaType = Fwg::Areas::AreaType::Land;
    superRegions.push_back(superRegion);
  }
  for (auto &waterVor : waterVoronois) {
    auto superRegion = factory();
    superRegion->ID = superRegions.size();
    superRegion->areaType = Fwg::Areas::AreaType::Sea;
    superRegions.push_back(superRegion);
  }
  // just add a map to track which region belongs to which type of voronoiArea
  std::map<int, Fwg::Areas::AreaType> regionAreaTypeMap;
  // now we match the regions to the voronoi areas, and create the strategic
  // regions by a best fit
  for (auto &region : ardaRegions) {
    std::unordered_map<int, int> voronoiOverlap;
    auto &regionPixels = region->pixels;
    for (const auto &pix : regionPixels) {
      auto voronoiID = indexToVoronoiID[pix];
      if (voronoiOverlap.find(voronoiID) == voronoiOverlap.end()) {
        voronoiOverlap[voronoiID] = 1;
      } else {
        voronoiOverlap[voronoiID]++;
      }
    }
    // now find the voronoi area with the most overlap
    int maxOverlap = 0;
    int bestVoronoiID = -1;
    for (const auto &voronoi : voronoiOverlap) {
      if (voronoi.second > maxOverlap) {
        maxOverlap = voronoi.second;
        bestVoronoiID = voronoi.first;
      }
    }
    superRegions[bestVoronoiID]->addRegion(region);
    regionAreaTypeMap[region->ID] = superRegions[bestVoronoiID]->areaType;
  }
  // to track which regions should be reassigned later after evaluation of
  // some metrics
  std::queue<std::shared_ptr<Arda::ArdaRegion>> regionsToBeReassigned;

  // postprocess stratregions
  for (auto &superRegion : superRegions) {
    superRegion->colour = Fwg::Gfx::generateUniqueColour(
        superRegion->ID, superRegion->areaType == Fwg::Areas::AreaType::Sea);
    // lets sum up all the pixels of the strategic region
    for (auto &ardaRegion : superRegion->ardaRegions) {
      superRegion->pixels.insert(superRegion->pixels.end(),
                                 ardaRegion->pixels.begin(),
                                 ardaRegion->pixels.end());
    }
    // let's find the weighted centre of the strat region
    superRegion->position.calcWeightedCenter(superRegion->pixels);

    // now get all clusters
    superRegion->regionClusters = superRegion->getClusters(ardaRegions);
    if (superRegion->regionClusters.size() > 1) {
      Fwg::Utils::Logging::logLineLevel(
          9, "Strategic region with ID: ", superRegion->ID,
          " has multiple clusters: ", superRegion->regionClusters.size());
    }

    // now if the strategic region is of AreaType sea, free the smaller
    // clusters, add their regions to the regionsToBeReassigned vector
    if (superRegion->regionClusters.size() > 1 &&
        superRegion->areaType == Fwg::Areas::AreaType::Sea) {
      Fwg::Utils::Logging::logLineLevel(
          9, "Strategic region with ID: ", superRegion->ID,
          " has multiple clusters, trying to free smaller clusters");
      // the biggest cluster by pixels size remains
      auto biggestCluster =
          std::max_element(superRegion->regionClusters.begin(),
                           superRegion->regionClusters.end(),
                           [](const Arda::Cluster &a, const Arda::Cluster &b) {
                             return a.size() < b.size();
                           });
      // free the others
      for (auto &cluster : superRegion->regionClusters) {
        if (&cluster != &(*biggestCluster)) {
          Fwg::Utils::Logging::logLine("Freeing cluster with size: ",
                                       cluster.size());
          // add the regions of the cluster to the regionsToBeReassigned
          // vector
          for (auto &region : cluster.regions) {
            regionsToBeReassigned.push(region);
            // remove the region from the superRegion ardaRegions vector
            auto it = std::find(superRegion->ardaRegions.begin(),
                                superRegion->ardaRegions.end(), region);
            if (it != superRegion->ardaRegions.end()) {
              Fwg::Utils::Logging::logLine(
                  "Removing region with ID: ", region->ID,
                  " from strategic region with ID: ", superRegion->ID);
              superRegion->ardaRegions.erase(it);

            } else {
              Fwg::Utils::Logging::logLine("Warning: Region not found in "
                                           "strategic region ardaRegions");
            }
          }
          // clear the cluster regions
          cluster.regions.clear();
        }
      }

      // remove empty clusters
      superRegion->regionClusters.erase(
          std::remove_if(superRegion->regionClusters.begin(),
                         superRegion->regionClusters.end(),
                         [](const Arda::Cluster &cluster) {
                           return cluster.regions.empty();
                         }),
          superRegion->regionClusters.end());
    }
  }

  // create a buffer to map which region is assigned to which strategic region
  std::map<int, int> assignedToIDs;
  for (auto &superRegion : superRegions) {
    // assign the regions to the strategic region
    for (auto &region : superRegion->ardaRegions) {
      // check for duplicate regions
      if (assignedToIDs.count(region->ID)) {
        Fwg::Utils::Logging::logLine(
            "Warning: Region with ID: ", region->ID,
            " is already assigned to strategic region with ID: ",
            assignedToIDs[region->ID]);
        continue; // skip this region, it is already assigned
      }

      assignedToIDs[region->ID] = superRegion->ID;
    }
  }

  // now we reassign the regions to the strategic regions
  // take from queue regionsToBeReassigned, and assign all of them until none
  // are left. We assign by taking all neighbours of the same AreaType, and
  // determining which one of those is closest to us using the position
  while (!regionsToBeReassigned.empty()) {
    auto region = regionsToBeReassigned.front();
    regionsToBeReassigned.pop();
    std::map<int, int> regionDistances;
    for (auto &neighbourId : region->neighbours) {
      auto &neighbourRegion = ardaRegions[neighbourId];
      // if the neighbour region is of the same area type, we can consider
      // that ones distance, and it must already be assigned
      if (regionAreaTypeMap[neighbourRegion->ID] ==
              regionAreaTypeMap[region->ID] &&
          assignedToIDs.count(neighbourRegion->ID)) {
        // calculate the distance between the two regions
        auto distance = Fwg::Utils::getDistance(
            region->position.weightedCenter,
            neighbourRegion->position.weightedCenter, config.width);
        regionDistances[neighbourId] = distance;
      }
    }
    // now find the closest neighbour region
    if (regionDistances.empty()) {
      Fwg::Utils::Logging::logLine("Warning: No neighbours of the same area "
                                   "type found for region with "
                                   "ID: ",
                                   region->ID);
      continue; // no neighbours of the same area type, skip
    }
    auto closestNeighbourIt = std::min_element(
        regionDistances.begin(), regionDistances.end(),
        [](const std::pair<int, int> &a, const std::pair<int, int> &b) {
          return a.second < b.second;
        });
    auto closestNeighbourID = closestNeighbourIt->first;
    auto closestNeighbourRegion = ardaRegions[closestNeighbourID];
    // check if the closest neighbour region is already assigned to a
    // strategic region
    if (assignedToIDs.find(closestNeighbourID) != assignedToIDs.end()) {
      // if it is, assign the region to the same strategic region
      auto superRegionID = assignedToIDs[closestNeighbourID];
      Fwg::Utils::Logging::logLine(
          "Reassigning region with ID: ", region->ID,
          " to strategic region with ID: ", superRegionID);
      superRegions[superRegionID]->addRegion(region);
      assignedToIDs[region->ID] = superRegionID;
    } else {
      // if it is not, put it back on the queue to be reassigned
      Fwg::Utils::Logging::logLine(
          "No strategic region found for region with ID: ", region->ID,
          " closest neighbour is: ", closestNeighbourID);
      regionsToBeReassigned.push(region);
    }
  }

  // safety check to see if all regions are assigned to some region
  for (const auto &region : ardaRegions) {
    if (assignedToIDs.find(region->ID) == assignedToIDs.end()) {
      Fwg::Utils::Logging::logLine("Warning: Region with ID: ", region->ID,
                                   " is not assigned to any strategic region");
      // assign it to the first strategic region
      if (!superRegions.empty()) {
        superRegions[0]->addRegion(region);
        assignedToIDs[region->ID] = superRegions[0]->ID;
      } else {
        Fwg::Utils::Logging::logLine("Error: No strategic regions available, "
                                     "cannot assign region with "
                                     "ID: ",
                                     region->ID);
      }
    }
  }
  // delete all empty strategic regions
  superRegions.erase(
      std::remove_if(superRegions.begin(), superRegions.end(),
                     [](const std::shared_ptr<Arda::SuperRegion> &superRegion) {
                       return superRegion->ardaRegions.empty();
                     }),
      superRegions.end());
  // fix IDs of strategic regions
  for (size_t i = 0; i < superRegions.size(); ++i) {
    superRegions[i]->ID = i;
    for (auto &region : superRegions[i]->ardaRegions) {
      // set the super region ID for the region
      region->superRegionID = i;
    }
    // also set the name of the strategic region
    superRegions[i]->name = std::to_string(i + 1);
  }

  Fwg::Utils::Logging::logLine(
      "Scenario: Done Dividing world into strategic regions");
  for (auto &superRegion : superRegions) {
    superRegion->checkPosition(superRegions);
    superRegion->name = std::to_string(superRegion->ID + 1);
  }
  return;
}


void saveRegions(std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions,
                   const std::string &mappingPath,
                   const Fwg::Gfx::Bitmap &regionImage) {
  std::string fileContent = "#r;g;b;name;population\n";
  for (const auto &region : ardaRegions) {
    fileContent += region->exportLine();
    fileContent += "\n";
  }
  Fwg::Parsing::writeFile(mappingPath + "//stateInputs.txt", fileContent);
  Fwg::Gfx::Png::save(regionImage, mappingPath + "//states.png");
}

} // namespace Arda::Areas