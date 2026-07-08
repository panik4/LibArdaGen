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

std::vector<std::shared_ptr<SuperRegion>> collectRegionsNeedingCenterFix(
    const std::vector<std::shared_ptr<SuperRegion>> &superRegions) {
  std::vector<std::shared_ptr<SuperRegion>> regionsNeedingFix;
  for (auto &superRegion : superRegions) {
    if (superRegion->centerOutsidePixels) {
      regionsNeedingFix.push_back(superRegion);
      Fwg::Utils::Logging::logLine("Strategic region ", superRegion->ID + 1,
                                   " needs fixing (center outside pixels)");
    }
  }
  return regionsNeedingFix;
}

std::shared_ptr<ArdaRegion> findRegionContainingCenter(
    const std::vector<std::shared_ptr<SuperRegion>> &superRegions,
    const std::shared_ptr<SuperRegion> &problematicSuperRegion, int centerPixel,
    std::shared_ptr<SuperRegion> &targetSuperRegionForCenter) {
  for (auto &superReg : superRegions) {
    if (superReg->ID == problematicSuperRegion->ID) {
      continue;
    }

    for (auto &reg : superReg->ardaRegions) {
      for (const auto pix : reg->getNonOwningPixelView()) {
        if (pix == centerPixel) {
          targetSuperRegionForCenter = superReg;
          return reg;
        }
      }
    }
  }

  return nullptr;
}

void recalculateSuperRegionPixels(
    const std::shared_ptr<SuperRegion> &superRegion,
    const std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions,
    const Fwg::Cfg &config) {
  superRegion->pixels.clear();
  for (auto &ardaRegion : superRegion->ardaRegions) {
    superRegion->pixels.insert(superRegion->pixels.end(),
                               ardaRegion->pixels.begin(),
                               ardaRegion->pixels.end());
  }

  superRegion->position.calcWeightedCenter(superRegion->pixels, config.width,
                                           config.height);
}

enum class CenterFixRemovalResult { Fixed, RolledBack, NotNeeded };

CenterFixRemovalResult attemptIterativeCenterFixRemoval(
    const std::function<std::shared_ptr<SuperRegion>()> &factory,
    std::vector<std::shared_ptr<SuperRegion>> &superRegions,
    std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions,
    const std::shared_ptr<SuperRegion> &problematicSuperRegion,
    const Fwg::Cfg &config,
    std::vector<std::shared_ptr<ArdaRegion>> &regionsToReassign) {
  auto originalRegions = problematicSuperRegion->ardaRegions;
  auto originalPixels = problematicSuperRegion->pixels;

  int centerPixel = problematicSuperRegion->position.weightedCenter;
  std::shared_ptr<SuperRegion> targetSuperRegionForCenter = nullptr;
  auto regionContainingCenter =
      findRegionContainingCenter(superRegions, problematicSuperRegion,
                                 centerPixel, targetSuperRegionForCenter);

  if (regionContainingCenter) {
    Fwg::Utils::Logging::logLine(
        "Center is in Region ", regionContainingCenter->ID + 1,
        " (part of Strategic Region ", targetSuperRegionForCenter->ID + 1, ")");
  }

  std::vector<std::pair<std::shared_ptr<ArdaRegion>, double>> regionDistances;
  for (auto &region : problematicSuperRegion->ardaRegions) {
    double distance = Fwg::Utils::Math::getDistance(
        region->position.weightedCenter,
        problematicSuperRegion->position.weightedCenter, config.width);
    regionDistances.push_back({region, distance});
  }

  std::sort(regionDistances.begin(), regionDistances.end(),
            [](const auto &a, const auto &b) { return a.second > b.second; });

  Fwg::Utils::Logging::logLine(
      "Attempting iterative removal (furthest regions first)...");

  bool fixedByRemoval = false;
  for (auto &[candidateRegion, distance] : regionDistances) {
    if (problematicSuperRegion->ardaRegions.size() <= 1) {
      Fwg::Utils::Logging::logLine(
          "Cannot remove more regions (only 1 remaining)");
      break;
    }

    auto it =
        std::find(problematicSuperRegion->ardaRegions.begin(),
                  problematicSuperRegion->ardaRegions.end(), candidateRegion);
    if (it == problematicSuperRegion->ardaRegions.end()) {
      continue;
    }

    problematicSuperRegion->ardaRegions.erase(it);
    recalculateSuperRegionPixels(problematicSuperRegion, ardaRegions, config);

    if (problematicSuperRegion->position.centerPresent(
            problematicSuperRegion->pixels)) {
      Fwg::Utils::Logging::logLine(
          "Success! Removed Region ", candidateRegion->ID + 1,
          " (distance: ", distance, ") - center now inside");
      regionsToReassign.push_back(candidateRegion);
      fixedByRemoval = true;
      break;
    }

    Fwg::Utils::Logging::logLine("Removed Region ", candidateRegion->ID + 1,
                                 " but center still outside, continuing...");
    regionsToReassign.push_back(candidateRegion);
  }

  if (!fixedByRemoval && !regionsToReassign.empty()) {
    Fwg::Utils::Logging::logLine(
        "Could not fix by removal alone. Reverting changes.");
    problematicSuperRegion->ardaRegions = originalRegions;
    problematicSuperRegion->pixels = originalPixels;
    problematicSuperRegion->position.calcWeightedCenter(
        problematicSuperRegion->pixels, config.width, config.height);
    regionsToReassign.clear();
    return CenterFixRemovalResult::RolledBack;
  }

  return fixedByRemoval ? CenterFixRemovalResult::Fixed
                        : CenterFixRemovalResult::NotNeeded;
}

void reassignRemovedCenterFixRegions(
    const std::function<std::shared_ptr<SuperRegion>()> &factory,
    std::vector<std::shared_ptr<SuperRegion>> &superRegions,
    const std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions,
    const std::shared_ptr<SuperRegion> &problematicSuperRegion,
    const Fwg::Cfg &config,
    std::vector<std::shared_ptr<ArdaRegion>> &regionsToReassign) {
  if (regionsToReassign.empty()) {
    return;
  }

  Fwg::Utils::Logging::logLine("Attempting to reassign ",
                               regionsToReassign.size(), " removed regions...");

  for (auto &regionToReassign : regionsToReassign) {
    std::vector<std::pair<std::shared_ptr<SuperRegion>, double>> candidates;

    for (auto &neighbour : regionToReassign->neighbours) {
      auto neighbourRegion = ardaRegions[neighbour->ID];
      if (neighbourRegion->superRegionID == problematicSuperRegion->ID) {
        continue;
      }

      auto candidateSuperRegion = superRegions[neighbourRegion->superRegionID];
      if (candidateSuperRegion->areaType != problematicSuperRegion->areaType) {
        continue;
      }

      bool alreadyCandidate = false;
      for (auto &[candSR, dist] : candidates) {
        if (candSR->ID == candidateSuperRegion->ID) {
          alreadyCandidate = true;
          break;
        }
      }

      if (!alreadyCandidate) {
        double distance = Fwg::Utils::Math::getDistance(
            regionToReassign->position.weightedCenter,
            candidateSuperRegion->position.weightedCenter, config.width);
        candidates.push_back({candidateSuperRegion, distance});
      }
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const auto &a, const auto &b) { return a.second < b.second; });

    bool reassigned = false;
    for (auto &[candidateSuperRegion, distance] : candidates) {
      auto testPixels = candidateSuperRegion->pixels;
      testPixels.insert(testPixels.end(), regionToReassign->pixels.begin(),
                        regionToReassign->pixels.end());

      Fwg::Position testPosition;
      testPosition.calcWeightedCenter(testPixels, config.width, config.height);

      if (testPosition.centerPresent(testPixels)) {
        Fwg::Utils::Logging::logLine(
            "Reassigning Region ", regionToReassign->ID + 1,
            " to Strategic Region ", candidateSuperRegion->ID + 1);

        candidateSuperRegion->addRegion(regionToReassign);
        candidateSuperRegion->pixels = testPixels;
        candidateSuperRegion->position = testPosition;
        reassigned = true;
        break;
      }

      Fwg::Utils::Logging::logLine("Cannot reassign to Strategic Region ",
                                   candidateSuperRegion->ID + 1,
                                   " (would move its center outside)");
    }

    if (!reassigned) {
      Fwg::Utils::Logging::logLine(
          "Creating new Strategic Region for orphaned Region ",
          regionToReassign->ID + 1);

      auto newSuperRegion = factory();
      newSuperRegion->ID = superRegions.size();
      newSuperRegion->areaType = problematicSuperRegion->areaType;
      newSuperRegion->addRegion(regionToReassign);
      newSuperRegion->pixels = regionToReassign->pixels;
      newSuperRegion->position.calcWeightedCenter(newSuperRegion->pixels,
                                                  config.width, config.height);
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
    if (region->areaSubType == Fwg::Areas::AreaSubType::Ocean ||
        region->areaSubType == Fwg::Areas::AreaSubType::OceanCoastal ||
        region->areaSubType == Fwg::Areas::AreaSubType::OceanIslandCoastal ||
        region->areaSubType == Fwg::Areas::AreaSubType::OceanMixedCoastal ||
        region->areaSubType == Fwg::Areas::AreaSubType::CoastalIsland ||
        region->areaSubType == Fwg::Areas::AreaSubType::Island ||
        region->areaSubType ==
            Fwg::Areas::AreaSubType::IslandEncompassingLake) {
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
      static_cast<int>(landShare * 110.0 * 6.0 * superRegionFactor);
  int waterSuperRegions =
      static_cast<int>(waterShare * 110.0 * 3.0 * superRegionFactor);

  int landMinDist = Fwg::Utils::Random::computePoissonMinDistFromArea(
      landAreaPixels.size(), landSuperRegions, config.width, 8.0);
  int waterMinDist = Fwg::Utils::Random::computePoissonMinDistFromArea(
      waterAreaPixels.size(), waterSuperRegions, config.width, 8.0);

  // Launch parallel Poisson disk generation
  auto waterFuture = std::async(std::launch::async, [&]() {
    return Fwg::Utils::Random::generatePoissonDiskPoints(
        waterAreaPixels, config.width, waterSuperRegions, waterMinDist,
        config.mapSeed);
  });

  auto landFuture = std::async(std::launch::async, [&]() {
    return Fwg::Utils::Random::generatePoissonDiskPoints(
        landAreaPixels, config.width, landSuperRegions, landMinDist,
        config.mapSeed);
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
      if (cell.empty())
        continue;

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
          double dist = Fwg::Utils::Math::getDistance(
              centroidPixel, candidatePixel, config.width);
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
                        config.mapsPath + "debug/landVoronoi.png", false);
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
                        config.mapsPath + "debug/waterVoronoi.png", false);
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

// Assign colors, aggregate pixels, compute centers, and build initial region
// clusters.
void initialiseStrategicRegions(
    std::vector<std::shared_ptr<SuperRegion>> &superRegions,
    const std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions) {
  for (auto &superRegion : superRegions) {
    superRegion->colour = Fwg::Gfx::generateUniqueColour(
        superRegion->ID, superRegion->areaType == Fwg::Areas::AreaType::Sea);

    for (auto &ardaRegion : superRegion->ardaRegions) {
      superRegion->pixels.insert(superRegion->pixels.end(),
                                 ardaRegion->pixels.begin(),
                                 ardaRegion->pixels.end());
    }

    const auto &config = Fwg::Cfg::Values();
    superRegion->position.calcWeightedCenter(superRegion->pixels, config.width,
                                             config.height);

    superRegion->regionClusters = superRegion->getClusters(ardaRegions);
    if (superRegion->regionClusters.size() > 1) {
      Fwg::Utils::Logging::logLineLevel(
          9, "Strategic region with ID: ", superRegion->ID,
          " has multiple clusters: ", superRegion->regionClusters.size());
    }
  }
}

// Split disjunct sea regions by freeing smaller clusters and queueing their
// regions for reassignment.
void freeDisjunctSeaClusters(
    std::vector<std::shared_ptr<SuperRegion>> &superRegions,
    std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions,
    std::queue<std::shared_ptr<Arda::ArdaRegion>> &regionsToBeReassigned) {
  const auto &config = Fwg::Cfg::Values();

  for (auto &superRegion : superRegions) {
    if (superRegion->areaType != Fwg::Areas::AreaType::Sea) {
      continue;
    }

    freeSmallerClustersFromSuperRegion(superRegion, ardaRegions,
                                       regionsToBeReassigned);

    if (!Fwg::Areas::isAreaContiguous(superRegion->pixels, config.width)) {
      Fwg::Utils::Logging::logLineLevel(
          9, "Warning: Strategic sea region with ID: ", superRegion->ID,
          " is still NOT contiguous (disjunct) after cluster freeing!");

      auto pixelClusters = Fwg::Areas::groupContiguousAreas(
          superRegion->pixels, config.width, config.height);

      if (pixelClusters.size() > 1) {
        Fwg::Utils::Logging::logLineLevel(
            9, "Found ", pixelClusters.size(),
            " disconnected pixel clusters in sea region ", superRegion->ID);

        auto largestCluster = std::max_element(
            pixelClusters.begin(), pixelClusters.end(),
            [](const std::vector<int> &a, const std::vector<int> &b) {
              return a.size() < b.size();
            });

        std::unordered_set<int> smallClusterPixels;
        for (auto &cluster : pixelClusters) {
          if (&cluster != &(*largestCluster)) {
            smallClusterPixels.insert(cluster.begin(), cluster.end());
          }
        }

        for (auto it = superRegion->ardaRegions.begin();
             it != superRegion->ardaRegions.end();) {
          auto &region = *it;

          int overlapCount = 0;
          for (const auto &pix : region->pixels) {
            if (smallClusterPixels.count(pix)) {
              overlapCount++;
            }
          }

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

        superRegion->pixels.clear();
        for (auto &ardaRegion : superRegion->ardaRegions) {
          superRegion->pixels.insert(superRegion->pixels.end(),
                                     ardaRegion->pixels.begin(),
                                     ardaRegion->pixels.end());
        }

        superRegion->regionClusters = superRegion->getClusters(ardaRegions);
      }
    }
  }
}

// Build a lookup of region ID to the strategic region it is currently assigned
// to.
std::map<int, int> buildRegionAssignmentMap(
    const std::vector<std::shared_ptr<SuperRegion>> &superRegions) {
  std::map<int, int> assignedToIDs;
  for (auto &superRegion : superRegions) {
    for (auto &region : superRegion->ardaRegions) {
      if (assignedToIDs.count(region->ID)) {
        Fwg::Utils::Logging::logLine(
            "Warning: Region with ID: ", region->ID,
            " is already assigned to strategic region with ID: ",
            assignedToIDs[region->ID]);
        continue;
      }

      assignedToIDs[region->ID] = superRegion->ID;
    }
  }
  return assignedToIDs;
}

// Reassign queued regions to nearby same-type strategic regions or queue them
// again if no match is found.
void reassignOrphanedRegions(
    std::vector<std::shared_ptr<SuperRegion>> &superRegions,
    const std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions,
    const std::map<int, Fwg::Areas::AreaType> &regionAreaTypeMap,
    std::queue<std::shared_ptr<Arda::ArdaRegion>> &regionsToBeReassigned,
    std::map<int, int> &assignedToIDs) {
  const auto &config = Fwg::Cfg::Values();

  while (!regionsToBeReassigned.empty()) {
    auto region = regionsToBeReassigned.front();
    regionsToBeReassigned.pop();

    std::map<int, int> regionDistances;
    for (auto &neighbour : region->neighbours) {
      auto &neighbourRegion = ardaRegions[neighbour->ID];
      if (regionAreaTypeMap.at(neighbourRegion->ID) ==
              regionAreaTypeMap.at(region->ID) &&
          assignedToIDs.count(neighbourRegion->ID)) {
        auto distance = Fwg::Utils::Math::getDistance(
            region->position.weightedCenter,
            neighbourRegion->position.weightedCenter, config.width);
        regionDistances[neighbour->ID] = distance;
      }
    }

    if (regionDistances.empty()) {
      Fwg::Utils::Logging::logLine("Warning: No neighbours of the same area "
                                   "type found for region with "
                                   "ID: ",
                                   region->ID);
      continue;
    }

    auto closestNeighbourIt = std::min_element(
        regionDistances.begin(), regionDistances.end(),
        [](const std::pair<int, int> &a, const std::pair<int, int> &b) {
          return a.second < b.second;
        });
    auto closestNeighbourID = closestNeighbourIt->first;

    if (assignedToIDs.find(closestNeighbourID) != assignedToIDs.end()) {
      auto superRegionID = assignedToIDs[closestNeighbourID];
      Fwg::Utils::Logging::logLine(
          "Reassigning region with ID: ", region->ID,
          " to strategic region with ID: ", superRegionID);
      superRegions[superRegionID]->addRegion(region);
      assignedToIDs[region->ID] = superRegionID;
    } else {
      Fwg::Utils::Logging::logLine(
          "No strategic region found for region with ID: ", region->ID,
          " closest neighbour is: ", closestNeighbourID);
      regionsToBeReassigned.push(region);
    }
  }
}

// Ensure every region has a strategic region assignment, falling back to the
// first region if needed.
void ensureAllRegionsAssigned(
    std::vector<std::shared_ptr<SuperRegion>> &superRegions,
    const std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions,
    std::map<int, int> &assignedToIDs) {
  for (const auto &region : ardaRegions) {
    if (assignedToIDs.find(region->ID) == assignedToIDs.end()) {
      Fwg::Utils::Logging::logLine("Warning: Region with ID: ", region->ID,
                                   " is not assigned to any strategic region");
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
}

// Remove empty strategic regions and renumber the remaining ones to keep IDs
// consistent.
void cleanupAndRenumberSuperRegions(
    std::vector<std::shared_ptr<SuperRegion>> &superRegions) {
  superRegions.erase(
      std::remove_if(superRegions.begin(), superRegions.end(),
                     [](const std::shared_ptr<Arda::SuperRegion> &superRegion) {
                       return superRegion->ardaRegions.empty();
                     }),
      superRegions.end());

  for (size_t i = 0; i < superRegions.size(); ++i) {
    superRegions[i]->ID = i;
    for (auto &region : superRegions[i]->ardaRegions) {
      region->superRegionID = i;
    }
    superRegions[i]->name = std::to_string(i + 1);
  }
}

// Recompute whether each strategic region's center lies inside its pixels.
void updateCenterOutsideFlags(
    std::vector<std::shared_ptr<SuperRegion>> &superRegions) {
  for (auto &superRegion : superRegions) {
    superRegion->centerOutsidePixels = superRegion->checkPosition(superRegions);
    superRegion->name = std::to_string(superRegion->ID + 1);
  }
}

// Rebuild strategic-region neighbour links from the underlying region
// adjacency graph.
void rebuildSuperRegionNeighbours(
    std::vector<std::shared_ptr<SuperRegion>> &superRegions,
    const std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions) {
  for (auto &superRegion : superRegions) {
    superRegion->neighbourSuperRegions.clear();
    superRegion->neighbours.clear();

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
            superRegion->neighbours.push_back(
                superRegions.at(neighbourRegion->superRegionID));
            superRegion->neighbourSuperRegions.push_back(
                superRegions.at(neighbourRegion->superRegionID));
          }
        }
      }
    }
  }
}

// Try to fix strategic regions whose weighted center falls outside their owned
// pixels.
void fixSuperRegionCenters(
    std::function<std::shared_ptr<SuperRegion>()> factory,
    std::vector<std::shared_ptr<SuperRegion>> &superRegions,
    std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions) {
  const auto &config = Fwg::Cfg::Values();

  Fwg::Utils::Logging::logLine(
      "=== Processing Strategic Regions with Centers Outside Pixels ===");

  auto regionsNeedingFix = collectRegionsNeedingCenterFix(superRegions);

  if (regionsNeedingFix.empty()) {
    Fwg::Utils::Logging::logLine("No strategic regions need center fixing");
  } else {
    Fwg::Utils::Logging::logLine("Processing ", regionsNeedingFix.size(),
                                 " strategic regions with misplaced centers");
  }

  for (auto &problematicSuperRegion : regionsNeedingFix) {
    Fwg::Utils::Logging::logLine("--- Attempting to fix Strategic Region ",
                                 problematicSuperRegion->ID + 1, " ---");

    std::vector<std::shared_ptr<ArdaRegion>> regionsToReassign;
    auto removalResult = attemptIterativeCenterFixRemoval(
        factory, superRegions, ardaRegions, problematicSuperRegion, config,
        regionsToReassign);

    if (removalResult == CenterFixRemovalResult::RolledBack) {
      continue;
    }

    if (removalResult == CenterFixRemovalResult::Fixed) {
      reassignRemovedCenterFixRegions(factory, superRegions, ardaRegions,
                                      problematicSuperRegion, config,
                                      regionsToReassign);
    }

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

  Fwg::Utils::Logging::logLine(
      "Recalculating strategic region IDs and relationships...");
  cleanupAndRenumberSuperRegions(superRegions);
  rebuildSuperRegionNeighbours(superRegions, ardaRegions);
}

// Log warnings for duplicate strategic centers and mismatches with regions or
// provinces.
void validateStrategicRegionCenters(
    const std::vector<std::shared_ptr<SuperRegion>> &superRegions,
    const std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions) {
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
}

// Final validation pass for super regions after all post-processing is done.
bool validateFinalSuperRegions(
    const std::vector<std::shared_ptr<SuperRegion>> &superRegions,
    const std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions) {
  const auto &config = Fwg::Cfg::Values();
  bool success = true;

  for (const auto &superRegion : superRegions) {
    superRegion->regionClusters = superRegion->getClusters(ardaRegions);
    if (superRegion->areaType == Fwg::Areas::AreaType::Sea) {

      if (superRegion->regionClusters.size() > 1) {
        Fwg::Utils::Logging::logLine(
            "Warning: Sea super region with ID: ", superRegion->ID,
            " still has disjoint region clusters: ",
            superRegion->regionClusters.size());
        success = false;
      }

      if (!Fwg::Areas::isAreaContiguous(superRegion->pixels, config.width)) {
        Fwg::Utils::Logging::logLine(
            "Warning: Sea super region with ID: ", superRegion->ID,
            " is not contiguous at pixel level");
        success = false;
      }

      for (const auto &region : superRegion->ardaRegions) {
        if (region->areaSubType == Fwg::Areas::AreaSubType::Mainland) {
          Fwg::Utils::Logging::logLine(
              "Warning: Sea super region with ID: ", superRegion->ID,
              " contains mainland region with ID: ", region->ID);
          success = false;
        }
      }
    } else {
      // for land super regions, we check if we contain any ocean regions
      for (const auto &region : superRegion->ardaRegions) {
        if (region->areaSubType == Fwg::Areas::AreaSubType::Ocean ||
            region->areaSubType == Fwg::Areas::AreaSubType::OceanCoastal ||
            region->areaSubType ==
                Fwg::Areas::AreaSubType::OceanIslandCoastal ||
            region->areaSubType == Fwg::Areas::AreaSubType::OceanMixedCoastal) {
          Fwg::Utils::Logging::logLine(
              "Warning: Land super region with ID: ", superRegion->ID,
              " contains ocean region with ID: ", region->ID);
        }
      }
    }
  }
  return success;
}

// Write a debug text dump of strategic-region centers and neighbor distances.
void dumpStrategicRegionDebugInfo(
    const std::vector<std::shared_ptr<SuperRegion>> &superRegions) {
  const auto &config = Fwg::Cfg::Values();

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
      debugOutput += std::to_string(Fwg::Utils::Math::getDistance(
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

void postProcessStrategicRegions(
    std::function<std::shared_ptr<SuperRegion>()> factory,
    std::vector<std::shared_ptr<SuperRegion>> &superRegions,
    std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions,
    const std::map<int, Fwg::Areas::AreaType> &regionAreaTypeMap) {
  std::queue<std::shared_ptr<Arda::ArdaRegion>> regionsToBeReassigned;

  initialiseStrategicRegions(superRegions, ardaRegions);
  freeDisjunctSeaClusters(superRegions, ardaRegions, regionsToBeReassigned);

  auto assignedToIDs = buildRegionAssignmentMap(superRegions);
  reassignOrphanedRegions(superRegions, ardaRegions, regionAreaTypeMap,
                          regionsToBeReassigned, assignedToIDs);
  ensureAllRegionsAssigned(superRegions, ardaRegions, assignedToIDs);

  cleanupAndRenumberSuperRegions(superRegions);

  Fwg::Utils::Logging::logLine(
      "Scenario: Done Dividing world into strategic regions");
  updateCenterOutsideFlags(superRegions);

  fixSuperRegionCenters(factory, superRegions, ardaRegions);

  Fwg::Utils::Logging::logLine(
      "=== Finished Processing Centers Outside Pixels ===");

  validateStrategicRegionCenters(superRegions, ardaRegions);
  dumpStrategicRegionDebugInfo(superRegions);
}

bool loadStrategicRegions(
    const Fwg::Gfx::Image &inputImage,
    std::function<std::shared_ptr<SuperRegion>()> factory,
    std::vector<std::shared_ptr<SuperRegion>> &superRegions,
    std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions,
    const Fwg::Terrain::TerrainData &terrainData) {
  if (inputImage.size() == 0) {
    return false;
  }
  Fwg::Utils::Logging::logLine("Arda::Areas: Loading superregions regions");
  superRegions.clear();
  // detect areas from inputImage
  auto inputAreas = Fwg::Areas::detectAreasByColour(inputImage);
  std::vector<std::vector<int>> landVoronois;
  std::vector<std::vector<int>> waterVoronois;
  for (auto &inputArea : inputAreas) {
    inputArea.calculateAreaType(terrainData.landFormIds);
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
  if (!validateFinalSuperRegions(superRegions, ardaRegions)) {
    Fwg::Utils::Logging::logLine("Error: Final super region validation failed");
    return false;
  }
  return true;
}

void validateStrategicRegions(
    std::vector<std::shared_ptr<SuperRegion>> &superRegions,
    std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions) {
  Fwg::Utils::Logging::logLine("Arda::Areas: Validating super regions");
  for (auto &superRegion : superRegions) {
    if (superRegion->ardaRegions.empty()) {
      Fwg::Utils::Logging::logLine(
          "Warning: Super region with ID: ", superRegion->ID,
          " has no arda regions assigned to it");
    }
    for (auto &region : superRegion->ardaRegions) {
      if (region->superRegionID != superRegion->ID) {
        Fwg::Utils::Logging::logLine(
            "Warning: Region with ID: ", region->ID,
            " is assigned to super region with ID: ", region->superRegionID,
            " but is actually in super region with ID: ", superRegion->ID);
      }
    }
  }
}

bool generateStrategicRegions(
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
  if (!validateFinalSuperRegions(superRegions, ardaRegions)) {
    Fwg::Utils::Logging::logLine("Error: Final super region validation failed");
    return false;
  }
  return true;
}

void saveRegions(std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions,
                 const std::string &mappingPath,
                 const Fwg::Gfx::Image &regionImage) {
  std::string fileContent = "#r;g;b;name;population\n";
  for (const auto &region : ardaRegions) {
    fileContent += region->exportLine();
    fileContent += "\n";
  }
  Fwg::Parsing::writeFile(mappingPath + "/stateInputs.txt", fileContent);
  Fwg::Gfx::Png::save(regionImage, mappingPath + "/states.png");
}

} // namespace Arda::Areas