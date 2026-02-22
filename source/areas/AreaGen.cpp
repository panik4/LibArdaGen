#include "areas/AreaGen.h"
namespace Arda::Areas {

// Helper function to free smaller clusters from a super region
void freeSmallerClustersFromSuperRegion(
    std::shared_ptr<SuperRegion> &superRegion,
    std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions,
    std::queue<std::shared_ptr<Arda::ArdaRegion>> &regionsToBeReassigned) {

  if (superRegion->regionClusters.size() <= 1) {
    return; // Nothing to do if there's only one cluster
  }

  Fwg::Utils::Logging::logLineLevel(
      9, "Strategic region with ID: ", superRegion->ID,
      " has multiple clusters, trying to free smaller clusters");

  // Find the biggest cluster by pixel size
  auto biggestCluster = std::max_element(
      superRegion->regionClusters.begin(), superRegion->regionClusters.end(),
      [](const Arda::Cluster &a, const Arda::Cluster &b) {
        return a.size() < b.size();
      });

  // Free all clusters except the biggest one
  for (auto &cluster : superRegion->regionClusters) {
    if (&cluster != &(*biggestCluster)) {
      Fwg::Utils::Logging::logLine("Freeing cluster with size: ",
                                   cluster.size());

      // Add the regions of the cluster to the regionsToBeReassigned vector
      for (auto &region : cluster.regions) {
        regionsToBeReassigned.push(region);

        // Remove the region from the superRegion ardaRegions vector
        auto it = std::find(superRegion->ardaRegions.begin(),
                            superRegion->ardaRegions.end(), region);
        if (it != superRegion->ardaRegions.end()) {
          Fwg::Utils::Logging::logLine(
              "Removing region with ID: ", region->ID,
              " from strategic region with ID: ", superRegion->ID);
          superRegion->ardaRegions.erase(it);
        } else {
          Fwg::Utils::Logging::logLine(
              "Warning: Region not found in strategic region ardaRegions");
        }
      }

      // Clear the cluster regions
      cluster.regions.clear();
    }
  }

  // Remove empty clusters
  superRegion->regionClusters.erase(
      std::remove_if(
          superRegion->regionClusters.begin(),
          superRegion->regionClusters.end(),
          [](const Arda::Cluster &cluster) { return cluster.regions.empty(); }),
      superRegion->regionClusters.end());
}

void generateSuperRegionVoronoi(
    std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions,
    const float &superRegionFactor, std::vector<std::vector<int>> &landVoronois,
    std::vector<std::vector<int>> &waterVoronois) {
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
      waterAreaPixels.insert(waterAreaPixels.end(), region->pixels.begin(),
                             region->pixels.end());
    } else {
      landAreaPixels.insert(landAreaPixels.end(), region->pixels.begin(),
                            region->pixels.end());
    }
  }
  auto assignedPixels = landAreaPixels.size() + waterAreaPixels.size();
  Fwg::Utils::Logging::logLine("We have a total of ", assignedPixels,
                               " pixels for investigation");
  if (assignedPixels > config.processingArea) {
    Fwg::Utils::Logging::logLine(
        "We have gathered too many pixels, maximum size should be ",
        config.processingArea);
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
      static_cast<int>(landShare * 110.0 * 4.0 * superRegionFactor);
  int waterSuperRegions =
      static_cast<int>(waterShare * 110.0 * 3.0 * superRegionFactor);

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
  // ========== LLOYD'S RELAXATION ==========
  const int lloydIterations = 3; // Number of relaxation iterations
  Fwg::Utils::Logging::logLine("Applying Lloyd's relaxation with ", 
                               lloydIterations, " iterations...");

  // Helper lambda to perform one iteration of Lloyd's relaxation
  auto performLloydRelaxation = [&](std::vector<int> &points, 
                                    const std::vector<int> &validPixels) {
    // Generate Voronoi diagram with current points
    std::vector<int> validSeeds;
    auto voronoiCells = Fwg::Utils::growRegionsMultiSourceClusters(
        validPixels, points, config.width, config.height,
        /*wrapX=*/false, /*fillIslands=*/true, &validSeeds);

    // Calculate centroids for each cell
    std::vector<int> newPoints;
    newPoints.reserve(voronoiCells.size());

    for (const auto &cell : voronoiCells) {
      if (cell.empty()) continue;

      // Calculate centroid (average position) of all pixels in this cell
      long long sumX = 0;
      long long sumY = 0;

      for (const auto &pixel : cell) {
        int x = pixel % config.width;
        int y = pixel / config.width;
        sumX += x;
        sumY += y;
      }

      int centroidX = static_cast<int>(sumX / cell.size());
      int centroidY = static_cast<int>(sumY / cell.size());
      int centroidPixel = centroidY * config.width + centroidX;

      // Verify centroid is within valid pixels
      bool centroidValid = std::find(validPixels.begin(), validPixels.end(), 
                                     centroidPixel) != validPixels.end();

      if (centroidValid) {
        newPoints.push_back(centroidPixel);
      } else {
        // If centroid is outside valid area, find nearest valid pixel
        int nearestPixel = cell[0];
        double minDist = std::numeric_limits<double>::max();

        for (const auto &candidatePixel : cell) {
          double dist = Fwg::Utils::getDistance(centroidPixel, candidatePixel, 
                                               config.width);
          if (dist < minDist) {
            minDist = dist;
            nearestPixel = candidatePixel;
          }
        }
        newPoints.push_back(nearestPixel);
      }
    }

    points = std::move(newPoints);
  };

  // Apply Lloyd's relaxation to land points
  Fwg::Utils::Logging::logLine("Relaxing land Voronoi cells...");
  for (int iter = 0; iter < lloydIterations; ++iter) {
    Fwg::Utils::Logging::logLineLevel(7, "  Land iteration ", iter + 1, "/", 
                                      lloydIterations);
    performLloydRelaxation(landPoints, landAreaPixels);
  }

  // Apply Lloyd's relaxation to water points
  Fwg::Utils::Logging::logLine("Relaxing water Voronoi cells...");
  for (int iter = 0; iter < lloydIterations; ++iter) {
    Fwg::Utils::Logging::logLineLevel(7, "  Water iteration ", iter + 1, "/", 
                                      lloydIterations);
    performLloydRelaxation(waterPoints, waterAreaPixels);
  }

  Fwg::Utils::Logging::logLine("Lloyd's relaxation complete");
  // ========== END LLOYD'S RELAXATION ==========

  // Generate final Voronoi diagrams with relaxed points
  std::vector<int> validLandSeeds;
  landVoronois = Fwg::Utils::growRegionsMultiSourceClusters(
      landAreaPixels, landPoints, config.width, config.height,
      /*wrapX=*/false,
      /*fillIslands=*/true, &validLandSeeds);

  std::vector<int> validWaterSeeds;
  waterVoronois = Fwg::Utils::growRegionsMultiSourceClusters(
      waterAreaPixels, waterPoints, config.width, config.height,
      /*wrapX=*/false,
      /*fillIslands=*/true, &validWaterSeeds);

  if (config.debugLevel > 5) {
    // debug visualise landVoronoi
    Fwg::Gfx::Image landVoronoiBmp(config.width, config.height, 24);
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
    Fwg::Gfx::Image waterVoronoiBmp(config.width, config.height, 24);
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
}

void assignStrategicRegionsFromClusters(
    std::function<std::shared_ptr<SuperRegion>()> factory,
    std::vector<std::shared_ptr<SuperRegion>> &superRegions,
    std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions,
    std::map<int, Fwg::Areas::AreaType> &regionAreaTypeMap,
    const std::vector<std::vector<int>> &landVoronois,
    const std::vector<std::vector<int>> &waterVoronois) {
  auto &config = Fwg::Cfg::Values();
  std::vector<int> indexToVoronoiID(config.processingArea);
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

  // now we match the regions to the voronoi areas, and create the strategic
  // regions by a best fit
  for (auto &region : ardaRegions) {
    std::unordered_map<int, int> voronoiOverlap;
    const auto regionPixels = region->getNonOwningPixelView();
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
}

void postProcessStrategicRegions(
    std::function<std::shared_ptr<SuperRegion>()> factory,
    std::vector<std::shared_ptr<SuperRegion>> &superRegions,
    std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions,
    const std::map<int, Fwg::Areas::AreaType> &regionAreaTypeMap) {

  const auto &config = Fwg::Cfg::Values();
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
    if (superRegion->areaType == Fwg::Areas::AreaType::Sea) {
      // Use the modular function to free smaller clusters
      freeSmallerClustersFromSuperRegion(superRegion, ardaRegions,
                                         regionsToBeReassigned);

      // Additionally check if a sea region is NOT contiguous at the pixel level
      // This catches cases where regionClusters might not detect disjunction
      if (!Fwg::Areas::isAreaContiguous(superRegion->pixels, config.width)) {
        Fwg::Utils::Logging::logLineLevel(
            9, "Warning: Strategic sea region with ID: ", superRegion->ID,
            " is still NOT contiguous (disjunct) after cluster freeing!");

        // Detect pixel-level clusters
        auto pixelClusters = Fwg::Areas::groupContiguousAreas(
            superRegion->pixels, config.width, config.height);

        if (pixelClusters.size() > 1) {
          Fwg::Utils::Logging::logLineLevel(
              9, "Found ", pixelClusters.size(),
              " disconnected pixel clusters in sea region ", superRegion->ID);

          // Find the largest pixel cluster
          auto largestCluster = std::max_element(
              pixelClusters.begin(), pixelClusters.end(),
              [](const std::vector<int> &a, const std::vector<int> &b) {
                return a.size() < b.size();
              });

          // Create a set of pixels in small clusters for fast lookup
          std::unordered_set<int> smallClusterPixels;
          for (auto &cluster : pixelClusters) {
            if (&cluster != &(*largestCluster)) {
              smallClusterPixels.insert(cluster.begin(), cluster.end());
            }
          }

          // Free regions that have majority of pixels in smaller clusters
          for (auto it = superRegion->ardaRegions.begin();
               it != superRegion->ardaRegions.end();) {
            auto &region = *it;

            // Count how many pixels of this region are in small clusters
            int overlapCount = 0;
            for (const auto &pix : region->pixels) {
              if (smallClusterPixels.count(pix)) {
                overlapCount++;
              }
            }

            // If majority of region is in small clusters, free it
            if (overlapCount > region->pixels.size() / 2) {
              Fwg::Utils::Logging::logLine(
                  "Freeing region with ID: ", region->ID,
                  " from strategic region with ID: ", superRegion->ID, " (",
                  overlapCount, " pixels in disjunct cluster)");
              regionsToBeReassigned.push(region);
              it = superRegion->ardaRegions.erase(it);
            } else {
              ++it;
            }
          }

          // Update the superRegion's pixels to only contain remaining regions
          superRegion->pixels.clear();
          for (auto &ardaRegion : superRegion->ardaRegions) {
            superRegion->pixels.insert(superRegion->pixels.end(),
                                       ardaRegion->pixels.begin(),
                                       ardaRegion->pixels.end());
          }

          // Recalculate region clusters after freeing regions
          superRegion->regionClusters = superRegion->getClusters(ardaRegions);
        }
      }
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
    for (auto &neighbour : region->neighbours) {
      auto &neighbourRegion = ardaRegions[neighbour->ID];
      // if the neighbour region is of the same area type, we can consider
      // that ones distance, and it must already be assigned
      if (regionAreaTypeMap.at(neighbourRegion->ID) ==
              regionAreaTypeMap.at(region->ID) &&
          assignedToIDs.count(neighbourRegion->ID)) {
        // calculate the distance between the two regions
        auto distance = Fwg::Utils::getDistance(
            region->position.weightedCenter,
            neighbourRegion->position.weightedCenter, config.width);
        regionDistances[neighbour->ID] = distance;
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
    superRegion->centerOutsidePixels = superRegion->checkPosition(superRegions);
    superRegion->name = std::to_string(superRegion->ID + 1);
  }

  // now let's try to split the strategic regions which have
  // superRegion->centerOutsidePixels = true. For this, we need to somehow
  // evaluate which region(s) need(s) to be removed to land inside the center.
  // We also need to check, in case we have multiple candidates, which the best
  // candidate for removal is. A removed region needs to be either assigned to
  // another strategic region of the same time, that is adjacent to the removed
  // region, and does not move the center of the other strategic region outside
  // of its own pixels. If there is no assignment to another strategic region
  // possible, we need to split of the strategic region and simply make it
  // separate

  Fwg::Utils::Logging::logLine(
      "=== Processing Strategic Regions with Centers Outside Pixels ===");

  // Collect all strategic regions that need fixing
  std::vector<std::shared_ptr<SuperRegion>> regionsNeedingFix;
  for (auto &superRegion : superRegions) {
    if (superRegion->centerOutsidePixels) {
      regionsNeedingFix.push_back(superRegion);
      Fwg::Utils::Logging::logLine("Strategic region ", superRegion->ID + 1,
                                   " needs fixing (center outside pixels)");
    }
  }

  if (regionsNeedingFix.empty()) {
    Fwg::Utils::Logging::logLine("No strategic regions need center fixing");
  } else {
    Fwg::Utils::Logging::logLine("Processing ", regionsNeedingFix.size(),
                                 " strategic regions with misplaced centers");
  }

  // Process each problematic strategic region
  for (auto &problematicSuperRegion : regionsNeedingFix) {
    Fwg::Utils::Logging::logLine("--- Attempting to fix Strategic Region ",
                                 problematicSuperRegion->ID + 1, " ---");

    // Store original state for potential rollback
    auto originalRegions = problematicSuperRegion->ardaRegions;
    auto originalPixels = problematicSuperRegion->pixels;
    auto originalCenter = problematicSuperRegion->position.weightedCenter;

    // Identify which region the center currently falls into
    int centerPixel = problematicSuperRegion->position.weightedCenter;
    std::shared_ptr<ArdaRegion> regionContainingCenter = nullptr;
    std::shared_ptr<SuperRegion> targetSuperRegionForCenter = nullptr;

    for (auto &superReg : superRegions) {
      if (superReg->ID == problematicSuperRegion->ID)
        continue;

      for (auto &reg : superReg->ardaRegions) {
        for (const auto pix : reg->getNonOwningPixelView()) {
          if (pix == centerPixel) {
            regionContainingCenter = reg;
            targetSuperRegionForCenter = superReg;
            break;
          }
        }
        if (regionContainingCenter)
          break;
      }
      if (regionContainingCenter)
        break;
    }

    if (regionContainingCenter) {
      Fwg::Utils::Logging::logLine("Center is in Region ",
                                   regionContainingCenter->ID + 1,
                                   " (part of Strategic Region ",
                                   targetSuperRegionForCenter->ID + 1, ")");
    }

    // Strategy 1: Try to remove regions iteratively until center is inside
    bool fixedByRemoval = false;
    std::vector<std::shared_ptr<ArdaRegion>> regionsToReassign;

    // Sort regions by distance from current center (furthest first)
    std::vector<std::pair<std::shared_ptr<ArdaRegion>, double>> regionDistances;
    for (auto &region : problematicSuperRegion->ardaRegions) {
      double distance = Fwg::Utils::getDistance(
          region->position.weightedCenter,
          problematicSuperRegion->position.weightedCenter, config.width);
      regionDistances.push_back({region, distance});
    }

    std::sort(regionDistances.begin(), regionDistances.end(),
              [](const auto &a, const auto &b) {
                return a.second > b.second; // Sort descending by distance
              });

    Fwg::Utils::Logging::logLine(
        "Attempting iterative removal (furthest regions first)...");

    // Try removing regions one by one, starting with the furthest
    for (auto &[candidateRegion, distance] : regionDistances) {
      // Don't remove the last region
      if (problematicSuperRegion->ardaRegions.size() <= 1) {
        Fwg::Utils::Logging::logLine(
            "Cannot remove more regions (only 1 remaining)");
        break;
      }

      // Temporarily remove this region
      auto it =
          std::find(problematicSuperRegion->ardaRegions.begin(),
                    problematicSuperRegion->ardaRegions.end(), candidateRegion);
      if (it == problematicSuperRegion->ardaRegions.end())
        continue;

      problematicSuperRegion->ardaRegions.erase(it);

      // Recalculate pixels and center
      problematicSuperRegion->pixels.clear();
      for (auto &ardaRegion : problematicSuperRegion->ardaRegions) {
        problematicSuperRegion->pixels.insert(
            problematicSuperRegion->pixels.end(), ardaRegion->pixels.begin(),
            ardaRegion->pixels.end());
      }

      problematicSuperRegion->position.calcWeightedCenter(
          problematicSuperRegion->pixels);

      // Check if center is now inside
      if (problematicSuperRegion->position.centerPresent(
              problematicSuperRegion->pixels)) {
        Fwg::Utils::Logging::logLine(
            "Success! Removed Region ", candidateRegion->ID + 1,
            " (distance: ", distance, ") - center now inside");
        regionsToReassign.push_back(candidateRegion);
        fixedByRemoval = true;
        break;
      } else {
        // Still outside, continue removing
        Fwg::Utils::Logging::logLine(
            "Removed Region ", candidateRegion->ID + 1,
            " but center still outside, continuing...");
        regionsToReassign.push_back(candidateRegion);
      }
    }

    // If we couldn't fix it by removal, restore original and try different
    // strategy
    if (!fixedByRemoval && !regionsToReassign.empty()) {
      Fwg::Utils::Logging::logLine(
          "Could not fix by removal alone. Reverting changes.");
      problematicSuperRegion->ardaRegions = originalRegions;
      problematicSuperRegion->pixels = originalPixels;
      problematicSuperRegion->position.calcWeightedCenter(
          problematicSuperRegion->pixels);
      regionsToReassign.clear();
    }

    // Strategy 2: If we successfully removed regions, try to reassign them
    if (fixedByRemoval && !regionsToReassign.empty()) {
      Fwg::Utils::Logging::logLine("Attempting to reassign ",
                                   regionsToReassign.size(),
                                   " removed regions...");

      for (auto &regionToReassign : regionsToReassign) {
        // Find adjacent strategic regions of the same type
        std::vector<std::pair<std::shared_ptr<SuperRegion>, double>> candidates;

        for (auto &neighbour : regionToReassign->neighbours) {
          auto neighbourRegion = ardaRegions[neighbour->ID];
          if (neighbourRegion->superRegionID == problematicSuperRegion->ID) {
            continue; // Skip regions still in the problematic strategic region
          }

          auto candidateSuperRegion =
              superRegions[neighbourRegion->superRegionID];

          // Must be same area type
          if (candidateSuperRegion->areaType !=
              problematicSuperRegion->areaType) {
            continue;
          }

          // Check if already in candidates
          bool alreadyCandidate = false;
          for (auto &[candSR, dist] : candidates) {
            if (candSR->ID == candidateSuperRegion->ID) {
              alreadyCandidate = true;
              break;
            }
          }

          if (!alreadyCandidate) {
            double distance = Fwg::Utils::getDistance(
                regionToReassign->position.weightedCenter,
                candidateSuperRegion->position.weightedCenter, config.width);
            candidates.push_back({candidateSuperRegion, distance});
          }
        }

        // Sort by distance (closest first)
        std::sort(
            candidates.begin(), candidates.end(),
            [](const auto &a, const auto &b) { return a.second < b.second; });

        // Try each candidate
        bool reassigned = false;
        for (auto &[candidateSuperRegion, distance] : candidates) {
          // Test if adding this region would break the candidate's center
          auto testPixels = candidateSuperRegion->pixels;
          testPixels.insert(testPixels.end(), regionToReassign->pixels.begin(),
                            regionToReassign->pixels.end());

          Fwg::Position testPosition;
          testPosition.calcWeightedCenter(testPixels);

          if (testPosition.centerPresent(testPixels)) {
            // Safe to add
            Fwg::Utils::Logging::logLine(
                "Reassigning Region ", regionToReassign->ID + 1,
                " to Strategic Region ", candidateSuperRegion->ID + 1);

            candidateSuperRegion->addRegion(regionToReassign);
            candidateSuperRegion->pixels = testPixels;
            candidateSuperRegion->position = testPosition;
            reassigned = true;
            break;
          } else {
            Fwg::Utils::Logging::logLine("Cannot reassign to Strategic Region ",
                                         candidateSuperRegion->ID + 1,
                                         " (would move its center outside)");
          }
        }

        // If we couldn't reassign, create a new strategic region
        if (!reassigned) {
          Fwg::Utils::Logging::logLine(
              "Creating new Strategic Region for orphaned Region ",
              regionToReassign->ID + 1);

          auto newSuperRegion = factory();
          newSuperRegion->ID = superRegions.size();
          newSuperRegion->areaType = problematicSuperRegion->areaType;
          newSuperRegion->addRegion(regionToReassign);

          // Calculate pixels and center for new super region
          newSuperRegion->pixels = regionToReassign->pixels;
          newSuperRegion->position.calcWeightedCenter(newSuperRegion->pixels);

          newSuperRegion->colour = Fwg::Gfx::generateUniqueColour(
              newSuperRegion->ID,
              newSuperRegion->areaType == Fwg::Areas::AreaType::Sea);

          newSuperRegion->name = std::to_string(newSuperRegion->ID + 1);

          superRegions.push_back(newSuperRegion);

          Fwg::Utils::Logging::logLine("Created new Strategic Region ",
                                       newSuperRegion->ID + 1);
        }
      }
    }

    // Final verification
    if (problematicSuperRegion->position.centerPresent(
            problematicSuperRegion->pixels)) {
      Fwg::Utils::Logging::logLine("Strategic Region ",
                                   problematicSuperRegion->ID + 1,
                                   " successfully fixed!");
      problematicSuperRegion->centerOutsidePixels = false;
    } else {
      Fwg::Utils::Logging::logLine(
          "Warning: Could not fully fix Strategic Region ",
          problematicSuperRegion->ID + 1, " - center still outside pixels");
    }
  }

  // Recalculate all strategic region IDs and relationships after changes
  Fwg::Utils::Logging::logLine(
      "Recalculating strategic region IDs and relationships...");
  for (size_t i = 0; i < superRegions.size(); ++i) {
    superRegions[i]->ID = i;
    superRegions[i]->name = std::to_string(i + 1);
    for (auto &region : superRegions[i]->ardaRegions) {
      region->superRegionID = i;
    }
  }

  // Rebuild neighbor relationships
  for (auto &superRegion : superRegions) {
    superRegion->neighbourSuperRegions.clear();
    for (auto &region : superRegion->ardaRegions) {
      for (auto &neighbour : region->neighbours) {
        auto neighbourRegion = ardaRegions[neighbour->ID];
        if (neighbourRegion->superRegionID != superRegion->ID) {
          bool alreadyNeighbour = false;
          for (auto &neighbourSuperRegion :
               superRegion->neighbourSuperRegions) {
            if (neighbourSuperRegion->ID == neighbourRegion->superRegionID) {
              alreadyNeighbour = true;
              break;
            }
          }
          if (!alreadyNeighbour) {
            superRegion->neighbourSuperRegions.push_back(
                superRegions.at(neighbourRegion->superRegionID));
          }
        }
      }
    }
  }

  Fwg::Utils::Logging::logLine(
      "=== Finished Processing Centers Outside Pixels ===");

  std::set<int> centers;
  for (auto &superRegion : superRegions) {
    if (centers.count(superRegion->position.weightedCenter) > 0) {
      Fwg::Utils::Logging::logLine(
          "Warning: Strategic region with ID: ", superRegion->ID,
          " has the same center as another strategic region, ID: ",
          centers.count(superRegion->position.weightedCenter));
    } else {
      centers.insert(superRegion->position.weightedCenter);
    }
  }
  // now let's check if any of the strategic regions has a center that is
  // exactly on a region center, where that region does NOT belong to the same
  // strategic region
  for (auto &superRegion : superRegions) {
    for (auto &region : ardaRegions) {
      if (region->position.weightedCenter ==
              superRegion->position.weightedCenter &&
          region->superRegionID != superRegion->ID) {
        Fwg::Utils::Logging::logLine(
            "Warning: Strategic region with ID: ", superRegion->ID,
            " has the same center as region with ID: ", region->ID,
            " which does not belong to the same strategic region");
      }
    }
  }
  // now let's do the same for provinces
  for (auto &superRegion : superRegions) {
    for (auto &region : ardaRegions) {
      for (auto &province : region->provinces) {
        if (province->position.weightedCenter ==
                superRegion->position.weightedCenter &&
            region->superRegionID != superRegion->ID) {
          Fwg::Utils::Logging::logLine(
              "Warning: Strategic region with ID: ", superRegion->ID,
              " has the same center as province with ID: ", province->ID,
              " which does not belong to the same strategic region");
        }
      }
    }
  }

  // let's now find all superregion neighbours
  for (auto &superRegion : superRegions) {
    superRegion->neighbours.clear();
    for (auto &region : superRegion->ardaRegions) {
      for (auto &neighbour : region->neighbours) {
        auto neighbourRegion = ardaRegions[neighbour->ID];
        if (neighbourRegion->superRegionID != superRegion->ID) {
          // check if the neighbour super region is already in the neighbours
          // vector
          bool alreadyNeighbour = false;
          for (auto &neighbourSuperRegion :
               superRegion->neighbourSuperRegions) {
            if (neighbourSuperRegion->ID == neighbourRegion->superRegionID) {
              alreadyNeighbour = true;
              break;
            }
          }
          if (!alreadyNeighbour) {
            superRegion->neighbourSuperRegions.push_back(
                superRegions.at(neighbourRegion->superRegionID));
          }
        }
      }
    }
  }
  // now let's debug dump the strategic regions into a file, with center
  // positions, and neighbour strategic regions, their positions, and the
  // distance to them
  std::string debugOutput = "ID;Name;ColourAreaType;Center;CenterX;\n";
  std::string header = "ID;Center;Distance;WidthCenter;HeightCenter\n";
  for (auto &superRegion : superRegions) {
    debugOutput +=
        std::to_string(superRegion->ID + 1) + ";" + superRegion->name + ";" +
        superRegion->colour.toString() + ";" +
        (superRegion->areaType == Fwg::Areas::AreaType::Land ? "Land" : "Sea") +
        ";" + std::to_string(superRegion->position.weightedCenter) + ";" +
        std::to_string(superRegion->position.widthCenter) + ";";
    debugOutput += "\n";
    debugOutput += header;

    for (auto &neighbour : superRegion->neighbourSuperRegions) {
      debugOutput += std::to_string(neighbour->ID + 1) + ",";
      debugOutput += std::to_string(neighbour->position.weightedCenter) + ",";
      debugOutput += std::to_string(Fwg::Utils::getDistance(
          superRegion->position.weightedCenter,
          neighbour->position.weightedCenter, config.width));
      debugOutput += std::to_string(neighbour->position.widthCenter) + ",";
      debugOutput += std::to_string(neighbour->position.heightCenter) + ",";
      debugOutput += "\n";
    }
    debugOutput += "\n";
  }
  Fwg::Parsing::writeFile(config.mapsPath + "debug//strategicRegionsDebug.txt",
                          debugOutput);
}

void loadStrategicRegions(
    const Fwg::Gfx::Image &inputImage,
    std::function<std::shared_ptr<SuperRegion>()> factory,
    std::vector<std::shared_ptr<SuperRegion>> &superRegions,
    std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions,
    const Fwg::Terrain::TerrainData &terrainData) {
  Fwg::Utils::Logging::logLine("Arda::Areas: Loading superregions regions");
  superRegions.clear();
  // detect areas from inputImage
  auto inputAreas = Fwg::Areas::detectAreasByColour(inputImage);
  std::vector<std::vector<int>> landVoronois;
  std::vector<std::vector<int>> waterVoronois;
  for (auto &inputArea : inputAreas) {
    inputArea.calculateTerrainType(terrainData);
    if (inputArea.areaType == Fwg::Areas::AreaType::Land) {
      landVoronois.push_back(inputArea.pixels);
    } else {
      waterVoronois.push_back(inputArea.pixels);
    }
  }

  // just add a map to track which region belongs to which type of
  // voronoiArea
  std::map<int, Fwg::Areas::AreaType> regionAreaTypeMap;
  assignStrategicRegionsFromClusters(factory, superRegions, ardaRegions,
                                     regionAreaTypeMap, landVoronois,
                                     waterVoronois);
  postProcessStrategicRegions(factory, superRegions, ardaRegions,
                              regionAreaTypeMap);
}

void generateStrategicRegions(
    std::function<std::shared_ptr<SuperRegion>()> factory,
    std::vector<std::shared_ptr<SuperRegion>> &superRegions,
    std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions,
    const float &superRegionFactor) {
  Fwg::Utils::Logging::logLine(
      "Arda::Areas: Dividing world into super regions");
  superRegions.clear();
  const auto &config = Fwg::Cfg::Values();

  std::vector<std::vector<int>> landVoronois;
  std::vector<std::vector<int>> waterVoronois;
  generateSuperRegionVoronoi(ardaRegions, superRegionFactor, landVoronois,
                             waterVoronois);
  // just add a map to track which region belongs to which type of
  // voronoiArea
  std::map<int, Fwg::Areas::AreaType> regionAreaTypeMap;
  assignStrategicRegionsFromClusters(factory, superRegions, ardaRegions,
                                     regionAreaTypeMap, landVoronois,
                                     waterVoronois);
  postProcessStrategicRegions(factory, superRegions, ardaRegions,
                              regionAreaTypeMap);

  return;
}

void saveRegions(std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions,
                 const std::string &mappingPath,
                 const Fwg::Gfx::Image &regionImage) {
  std::string fileContent = "#r;g;b;name;population\n";
  for (const auto &region : ardaRegions) {
    fileContent += region->exportLine();
    fileContent += "\n";
  }
  Fwg::Parsing::writeFile(mappingPath + "//stateInputs.txt", fileContent);
  Fwg::Gfx::Png::save(regionImage, mappingPath + "//states.png");
}

} // namespace Arda::Areas