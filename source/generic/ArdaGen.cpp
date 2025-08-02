#include "generic/ArdaGen.h"
namespace Logging = Fwg::Utils::Logging;
namespace Arda {
using namespace Fwg::Gfx;
ArdaGen::ArdaGen() {}

ArdaGen::ArdaGen(const std::string &configSubFolder)
    : FastWorldGenerator(configSubFolder) {
  Gfx::Flag::readColourGroups();
  Gfx::Flag::readFlagTypes();
  Gfx::Flag::readFlagTemplates();
  Gfx::Flag::readSymbolTemplates();
  superRegionMap = Bitmap(0, 0, 24);
}

ArdaGen::ArdaGen(Fwg::FastWorldGenerator &fwg) : FastWorldGenerator(fwg) {}

ArdaGen::~ArdaGen() {}

void ArdaGen::generateCountries(
    std::function<std::shared_ptr<Country>()> factory) {
  // generate country data
  Arda::Countries::generateCountries(factory, numCountries, ardaRegions,
                                     countries, ardaProvinces, civData, nData);
  //  first gather generic neighbours, they will be mapped to hoi4 countries
  //  in mapCountries
  Arda::Countries::evaluateCountryNeighbours(areaData.regions, ardaRegions,
                                             countries);
  evaluateCountries();
  Fwg::Utils::Logging::logLine("Visualising Countries");
  visualiseCountries(countryMap);
  Fwg::Gfx::Png::save(countryMap,
                      Fwg::Cfg::Values().mapsPath + "countries.png");
  mapCountries();
  evaluateCountries();
}

void ArdaGen::mapContinents() {
  Logging::logLine("Mapping Continents");
  scenContinents.clear();
  for (const auto &continent : this->areaData.continents) {
    // we copy the fwg continents by choice, to leave them untouched
    scenContinents.push_back(ArdaContinent(continent));
  }
}

void ArdaGen::mapRegions() {
  Logging::logLine("Mapping Regions");
  ardaRegions.clear();

  for (auto &region : this->areaData.regions) {
    std::sort(region.provinces.begin(), region.provinces.end(),
              [](const std::shared_ptr<Fwg::Areas::Province> a,
                 const std::shared_ptr<Fwg::Areas::Province> b) {
                return (*a < *b);
              });
    auto ardaRegion = std::make_shared<ArdaRegion>(region);

    for (auto &province : ardaRegion->provinces) {
      ardaRegion->ardaProvinces.push_back(ardaProvinces[province->ID]);
    }
    // save game region
    ardaRegions.push_back(ardaRegion);
  }
  // sort by Arda::ArdaProvince ID
  std::sort(ardaRegions.begin(), ardaRegions.end(),
            [](auto l, auto r) { return *l < *r; });
  // check if we have the same amount of ardaProvinces as FastWorldGen provinces
  if (ardaProvinces.size() != this->areaData.provinces.size())
    throw(std::exception("Fatal: Lost provinces, terminating"));
  if (ardaRegions.size() != this->areaData.regions.size())
    throw(std::exception("Fatal: Lost regions, terminating"));
  for (const auto &ardaRegion : ardaRegions) {
    if (ardaRegion->ID > ardaRegions.size()) {
      throw(std::exception("Fatal: Invalid region IDs, terminating"));
    }
  }
  applyRegionInput();
}

void ArdaGen::mapCountries() {}

void ArdaGen::applyRegionInput() {
  Fwg::Utils::ColourTMap<std::vector<std::string>> regionInputMap;
  if (regionMappingPath.size() && std::filesystem::exists(regionMappingPath)) {
    auto mappingFileLines = Fwg::Parsing::getLines(regionMappingPath);
    for (auto &line : mappingFileLines) {
      auto tokens = Fwg::Parsing::getTokens(line, ';');
      auto colour = Fwg::Gfx::Colour(std::stoi(tokens[0]), std::stoi(tokens[1]),
                                     std::stoi(tokens[2]));
      regionInputMap.setValue(colour, tokens);
    }
  }
  for (auto &ardaRegion : this->ardaRegions) {
    if (regionInputMap.find(ardaRegion->colour)) {
      if (regionInputMap[ardaRegion->colour].size() > 3 &&
          regionInputMap[ardaRegion->colour][3].size()) {
        // get the predefined name
        ardaRegion->name = regionInputMap[ardaRegion->colour][3];
      }
      if (regionInputMap[ardaRegion->colour].size() > 4 &&
          regionInputMap[ardaRegion->colour][4].size()) {
        try {

          // get the predefined population
          ardaRegion->totalPopulation =
              stoi(regionInputMap[ardaRegion->colour][4]);
        } catch (std::exception e) {
          Fwg::Utils::Logging::logLine(
              "ERROR: Some of the tokens can't be turned into a population "
              "number. The faulty token is ",
              regionInputMap[ardaRegion->colour][4]);
        }
      }
    }
  }
  // debug visualisation of all regions, if coastal they are yellow, if sea they
  // are blue, if non-coastal they are green
  Bitmap regionMap(Fwg::Cfg::Values().width, Fwg::Cfg::Values().height, 24);
  for (auto &ardaRegion : this->ardaRegions) {
    for (auto &gameProv : ardaRegion->ardaProvinces) {
      for (auto &pix : gameProv->baseProvince->pixels) {
        if (ardaRegion->isSea()) {
          regionMap.setColourAtIndex(pix, Fwg::Cfg::Values().colours.at("sea"));

        } else if (ardaRegion->coastal && !ardaRegion->isSea()) {
          regionMap.setColourAtIndex(pix,
                                     Fwg::Cfg::Values().colours.at("ores"));

        } else if (ardaRegion->isLake()) {
          regionMap.setColourAtIndex(pix,
                                     Fwg::Cfg::Values().colours.at("lake"));
        }

        else {
          regionMap.setColourAtIndex(pix,
                                     Fwg::Cfg::Values().colours.at("land"));
          if (gameProv->baseProvince->coastal) {
            regionMap.setColourAtIndex(
                pix, Fwg::Cfg::Values().colours.at("autumnForest"));
          }
        }
      }
    }
  }
  Png::save(regionMap, Fwg::Cfg::Values().mapsPath + "debug//regionTypes.png",
            false);
}

void ArdaGen::applyCountryInput() {}

void ArdaGen::mapProvinces() {
  ardaProvinces.clear();
  for (auto &prov : this->areaData.provinces) {
    // edit coastal status: lakes are not coasts!
    if (prov->coastal && prov->isLake())
      prov->coastal = false;
    // if it is a land province, check that a neighbour is an ocean, otherwise
    // this isn't coastal in this scenario definition
    else if (prov->coastal) {
      bool foundTrueCoast = false;
      for (auto &neighbour : prov->neighbours) {
        if (neighbour->isSea()) {
          foundTrueCoast = true;
        }
      }
      prov->coastal = foundTrueCoast;
    }

    // now create ardaProvinces from FastWorldGen provinces
    auto gP = std::make_shared<Arda::ArdaProvince>(prov);
    // also copy neighbours
    for (auto &baseProvinceNeighbour : gP->baseProvince->neighbours)
      gP->neighbours.push_back(baseProvinceNeighbour);
    ardaProvinces.push_back(gP);
  }

  // sort by Arda::ArdaProvince ID
  std::sort(ardaProvinces.begin(), ardaProvinces.end(),
            [](auto l, auto r) { return *l < *r; });
}

void ArdaGen::evaluateCountries() {}

Bitmap ArdaGen::visualiseCountries(Fwg::Gfx::Bitmap &countryBmp, const int ID) {
  Logging::logLine("Drawing borders");
  auto &config = Fwg::Cfg::Values();
  if (!countryBmp.initialised()) {
    countryBmp = Bitmap(config.width, config.height, 24);
  }
  if (ID > -1) {
    for (const auto &prov : ardaRegions[ID]->provinces) {
      auto countryColour = Fwg::Gfx::Colour(0, 0, 0);
      const auto &region = ardaRegions[ID];

      if (region->owner) {
        countryColour = region->owner->colour;
      }
      for (const auto &pix : prov->pixels) {
        countryBmp.setColourAtIndex(pix,
                                    countryColour * 0.9 + prov->colour * 0.1);
      }
      for (auto &pix : region->borderPixels) {
        countryBmp.setColourAtIndex(pix, countryColour * 0.0);
      }
    }
  } else {
    Fwg::Gfx::Bitmap noBorderCountries(config.width, config.height, 24);
    for (const auto &region : ardaRegions) {
      auto countryColour = Fwg::Gfx::Colour(0, 0, 0);
      // if this tag is assigned, use the colour
      if (region->owner) {
        countryColour = region->owner->colour;
      }
      for (const auto &prov : region->provinces) {
        for (const auto &pix : prov->pixels) {
          countryBmp.setColourAtIndex(pix,
                                      countryColour * 0.9 + prov->colour * 0.1);
          // clean export, for editing outside of the tool, for later loading
          noBorderCountries.setColourAtIndex(pix, countryColour * 1.0);
        }
      }
      for (auto &pix : region->borderPixels) {
        countryBmp.setColourAtIndex(pix, countryColour * 0.0);
      }
    }
    Png::save(noBorderCountries,
              Fwg::Cfg::Values().mapsPath + "countries_no_borders.png");
  }
  return countryBmp;
}

void ArdaGen::totalResourceVal(const std::vector<float> &resPrev,
                               float resourceModifier,
                               const Arda::Utils::ResConfig &resourceConfig) {
  const auto baseResourceAmount = resourceModifier;
  auto totalRes = 0.0;
  for (auto &val : resPrev) {
    totalRes += val;
  }
  for (auto &reg : ardaRegions) {
    auto resShare = 0.0;
    for (const auto &prov : reg->provinces) {
      for (const auto &pix : prov->pixels) {
        resShare += resPrev[pix];
      }
    }
    // basically fictive value from given input of how often this resource
    // appears
    auto stateRes = baseResourceAmount * (resShare / totalRes);
    // round to whole number
    stateRes = std::round(stateRes);
    reg->resources.insert(
        {resourceConfig.name,
         {resourceConfig.name, resourceConfig.capped, stateRes}});
  }
}

void ArdaGen::generateStrategicRegions(
    std::function<std::shared_ptr<SuperRegion>()> factory) {
  Fwg::Utils::Logging::logLine(
      "Scenario: Dividing world into strategic regions");
  superRegions.clear();
  superRegionMap.clear();
  const auto &config = Fwg::Cfg::Values();

  std::vector<int> waterAreaPixels;
  std::vector<int> landAreaPixels;
  for (auto &region : this->ardaRegions) {
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
      static_cast<int>(landShare * 110.0 * 2.0 * this->superRegionFactor);
  int waterSuperRegions =
      static_cast<int>(waterShare * 110.0 * 1.0 * this->superRegionFactor);

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
  for (auto &region : this->ardaRegions) {
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
  ////  build a vector of superregions from all the strategic regions
  // for (auto &superRegion : superRegions) {
  //   StrategicRegion stratRegion;
  //   stratRegion->ID = superRegion->ID;
  //   stratRegion->ardaRegions = superRegion->ardaRegions;
  //   stratRegion->setType();
  //   strategicRegions.push_back(stratRegion);
  // }
  // visualiseStrategicRegions();
  // Fwg::Utils::Logging::logLine(
  //     "Scenario: Done visualising and checking positions of strategic
  //     regions");

  return;
}

void ArdaGen::printStatistics() {
  Logging::logLine("Printing Statistics");
  std::map<std::string, int> countryPop;
  for (auto &c : countries) {
    countryPop[c.first] = 0;
    for (auto &gR : c.second->ownedRegions) {
      countryPop[c.first] += gR->totalPopulation;
    }
  }
  for (auto &c : countries) {
    Logging::logLine("Country: ", c.first,
                     " Population: ", countryPop[c.first]);
  }
}
void ArdaGen::writeTextFiles() {}
void ArdaGen::writeLocalisation() {}
void ArdaGen::writeImages() {}
} // namespace Arda