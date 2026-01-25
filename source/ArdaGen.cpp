#include "ArdaGen.h"
namespace Logging = Fwg::Utils::Logging;
namespace Arda {
using namespace Fwg::Gfx;
ArdaGen::ArdaGen() {
  factories.provinceFactory = []() {
    return std::make_shared<Arda::ArdaProvince>();
  };
  factories.regionFactory = []() {
    return std::make_shared<Arda::ArdaRegion>();
  };
  factories.continentFactory = [](const std::vector<int> &pixels) {
    return std::make_shared<Arda::ArdaContinent>(pixels);
  };
  ardaFactories.superRegionFactory =
      []() -> std::shared_ptr<Arda::SuperRegion> {
    return std::make_shared<Arda::SuperRegion>();
  };
  ardaFactories.countryFactory = []() -> std::shared_ptr<Arda::Country> {
    return std::make_shared<Arda::Country>();
  };
}

ArdaGen::ArdaGen(const std::string &configSubFolder)
    : FastWorldGenerator(configSubFolder) {
  Gfx::Flag::readColourGroups();
  Gfx::Flag::readFlagTypes();
  Gfx::Flag::readFlagTemplates();
  Gfx::Flag::readSymbolTemplates();
  superRegionMap = Image(0, 0, 24);
  factories.provinceFactory = []() {
    return std::make_shared<Arda::ArdaProvince>();
  };
  factories.regionFactory = []() {
    return std::make_shared<Arda::ArdaRegion>();
  };
  factories.continentFactory = [](const std::vector<int> &pixels) {
    return std::make_shared<Arda::ArdaContinent>(pixels);
  };
  ardaFactories.superRegionFactory =
      []() -> std::shared_ptr<Arda::SuperRegion> {
    return std::make_shared<Arda::SuperRegion>();
  };
  ardaFactories.countryFactory = []() -> std::shared_ptr<Arda::Country> {
    return std::make_shared<Arda::Country>();
  };
}

ArdaGen::ArdaGen(Fwg::FastWorldGenerator &fwg) : FastWorldGenerator(fwg) {
  factories.provinceFactory = []() {
    return std::make_shared<Arda::ArdaProvince>();
  };
  factories.regionFactory = []() {
    return std::make_shared<Arda::ArdaRegion>();
  };
  factories.continentFactory = [](const std::vector<int> &pixels) {
    return std::make_shared<Arda::ArdaContinent>(pixels);
  };
  ardaFactories.superRegionFactory =
      []() -> std::shared_ptr<Arda::SuperRegion> {
    return std::make_shared<Arda::SuperRegion>();
  };
  ardaFactories.countryFactory = []() -> std::shared_ptr<Arda::Country> {
    return std::make_shared<Arda::Country>();
  };
}

ArdaGen::~ArdaGen() {}

void ArdaGen::generateCountries(
    std::function<std::shared_ptr<Country>()> factory) {

  std::filesystem::create_directory(Fwg::Cfg::Values().mapsPath + "/countries");
  // generate country data
  if (factory != nullptr) {
    Arda::Countries::generateCountries(
        ardaConfig.generationAge, factory, ardaConfig.numCountries, ardaRegions,
        countries, ardaProvinces, civData, nData);
  }
  //  first gather generic neighbours, they will be mapped to hoi4 countries
  //  in mapCountries
  Arda::Countries::evaluateCountryNeighbours(areaData.regions, ardaRegions,
                                             countries);
  Fwg::Utils::Logging::logLine("Visualising Countries");
  visualiseCountries(countryMap, worldMap);
  Fwg::Gfx::Png::save(countryMap,
                      Fwg::Cfg::Values().mapsPath + "countries/countries.png");
  mapCountries();
  evaluateCountries();
  Arda::Countries::saveCountries(countries,
                                 Fwg::Cfg::Values().mapsPath + "/countries/");
}

void ArdaGen::loadCountries(std::function<std::shared_ptr<Country>()> factory,
                            const std::string &path) {
  // generate country data
  if (factory != nullptr) {
    Arda::Countries::loadCountriesFromText(ardaConfig.generationAge, factory,
                                           ardaRegions, countries, civData,
                                           nData, Fwg::Parsing::readFile(path));
  }
  Arda::Countries::evaluateCountryNeighbours(areaData.regions, ardaRegions,
                                             countries);
  mapCountries();
  evaluateCountries();
}

void ArdaGen::loadCountries(std::function<std::shared_ptr<Country>()> factory,
                            const Fwg::Gfx::Image &inputImage) {
  // generate country data
  if (factory != nullptr) {
    Arda::Countries::loadCountries(ardaConfig.generationAge, factory,
                                   ardaRegions, countries, civData, nData,
                                   inputImage);
  }
  Arda::Countries::evaluateCountryNeighbours(areaData.regions, ardaRegions,
                                             countries);
  mapCountries();
  evaluateCountries();
}

void ArdaGen::mapContinents() {
  Logging::logLine("Mapping Continents");
  ardaContinents.clear();
  for (const auto &continent : this->areaData.continents) {
    // reinterpret FastWorldGen continent as ArdaContinent
    auto ardaContinent = std::dynamic_pointer_cast<ArdaContinent>(continent);
    for (const auto &prov : ardaContinent->provinces) {
      ardaContinent->ardaProvinces.push_back(
          std::dynamic_pointer_cast<Arda::ArdaProvince>(prov));
    }
    for (const auto &region : ardaContinent->regions) {
      ardaContinent->ardaRegions.push_back(
          std::dynamic_pointer_cast<Arda::ArdaRegion>(region));
    }
    ardaContinents.push_back(ardaContinent);
  }
}

void ArdaGen::mapRegions() {
  Logging::logLine("Mapping Regions");
  ardaRegions.clear();

  for (auto &region : this->areaData.regions) {
    std::sort(region->provinces.begin(), region->provinces.end(),
              [](const std::shared_ptr<Fwg::Areas::Province> a,
                 const std::shared_ptr<Fwg::Areas::Province> b) {
                return (*a < *b);
              });
    auto ardaRegion = std::dynamic_pointer_cast<Arda::ArdaRegion>(region);
    ardaRegion->ardaProvinces.clear();

    for (auto &province : ardaRegion->provinces) {
      ardaRegion->ardaProvinces.push_back(ardaProvinces[province->ID]);
    }
    // save game region
    ardaRegions.push_back(ardaRegion);
  }
  // sort by Arda::ArdaProvince ID
  // std::sort(ardaRegions.begin(), ardaRegions.end(),
  //          [](auto l, auto r) { return *l < *r; });
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
      if (line.size()) {
        auto tokens = Fwg::Parsing::getTokens(line, ';');
        if (tokens.size() > 3) {
          auto colour = Fwg::Gfx::Colour(
              std::stoi(tokens[0]), std::stoi(tokens[1]), std::stoi(tokens[2]));
          regionInputMap.setValue(colour, tokens);
        }
      }
    }
  }
  for (auto &ardaRegion : this->ardaRegions) {
    if (regionInputMap.contains(ardaRegion->colour)) {
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
  Image regionMap(Fwg::Cfg::Values().width, Fwg::Cfg::Values().height, 24);
  for (auto &ardaRegion : this->ardaRegions) {
    for (auto &gameProv : ardaRegion->ardaProvinces) {
      for (auto &pix : gameProv->pixels) {
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
          if (gameProv->coastal) {
            regionMap.setColourAtIndex(
                pix, Fwg::Cfg::Values().colours.at("autumnForest"));
          }
        }
      }
    }
  }
  Png::save(regionMap,
            Fwg::Cfg::Values().mapsPath + "debug/ardaRegionTypes.png", false);
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
    auto gP = std::dynamic_pointer_cast<Arda::ArdaProvince>(prov);
    // also copy neighbours
    // for (auto &baseProvinceNeighbour : gP->neighbours)
    //  gP->neighbours.push_back(baseProvinceNeighbour);
    ardaProvinces.push_back(gP);
  }

  //// sort by Arda::ArdaProvince ID
  // std::sort(ardaProvinces.begin(), ardaProvinces.end(),
  //           [](auto l, auto r) { return *l < *r; });
}

Fwg::Gfx::Image ArdaGen::mapTerrain() {
  using namespace Arda::Civilization;
  auto &civLayer = this->ardaData.civLayer;
  auto &overlayColours = Fwg::Cfg::Values().topographyOverlayColours;
  auto topoMap =
      Fwg::Gfx::Image(Fwg::Cfg::Values().width, Fwg::Cfg::Values().height, 24);

  // Define the topography types we care about and their thresholds
  struct TopoDef {
    TopographyType type;
    double provinceThreshold; // fraction of province pixels
    double regionThreshold;   // fraction of region pixels
  };

  const std::vector<TopoDef> topoDefs = {
      {TopographyType::CITY, 0.25, 0.25},
      {TopographyType::PORTCITY, 0.00, 0.00}, // any pixel triggers inclusion
      {TopographyType::MARSH, 0.50, 0.50},
      {TopographyType::WASTELAND, 0.50, 0.50},
      {TopographyType::FARMLAND, 0.50, 0.50},
  };

  // Process all regions
  for (auto &region : ardaRegions) {
    std::unordered_map<TopographyType, int> regionTotals;

    const auto regionSize = region->getNonOwningPixelView().size();

    // Process each province
    for (auto &province : region->ardaProvinces) {
      if (!province->isLand())
        continue;

      const int provSize = static_cast<int>(province->pixels.size());

      for (const auto &[type, provThresh, _] : topoDefs) {
        const int count = civLayer.countOfTypeInRange(province->pixels, type);
        regionTotals[type] += count;

        // Province classification
        const bool meetsThreshold =
            provThresh == 0.0 ? (count > 0) : (count > provSize * provThresh);

        if (meetsThreshold)
          province->topographyTypes.insert(type);
      }

      // City special case: combine with portcity pixels for density
      const int cityPixels =
          civLayer.countOfTypeInRange(province->pixels, TopographyType::CITY);
      const int portPixels = civLayer.countOfTypeInRange(
          province->pixels, TopographyType::PORTCITY);
      if (cityPixels + portPixels > provSize / 4) {
        province->topographyTypes.insert(TopographyType::CITY);
      }
    }

    // Region-level classification
    for (const auto &[type, _, regionThresh] : topoDefs) {
      const int total = regionTotals[type];
      const bool meetsThreshold = regionThresh == 0.0
                                      ? (total > 0)
                                      : (total > regionSize * regionThresh);

      if (meetsThreshold)
        region->topographyTypes.insert(type);
    }
  }

  return topoMap;
}

void ArdaGen::genNaturalFeatures() {
  // detect marshes
  // Arda::NaturalFeatures::detectMarshes(terrainData, climateData,
  //                                     ardaData.civLayer,
  //                                     Fwg::Cfg::Values());
  clearLocations();
  ardaData.civLayer.clear();
  // genWastelands(Fwg::Cfg::Values());
  mapTerrain();
}

bool ArdaGen::loadNaturalFeatures(Fwg::Cfg &config,
                                  const Fwg::Gfx::Image &inputFeatures) {
  // clearLocations();
  ardaData.civLayer.clear();
  Arda::NaturalFeatures::loadNaturalFeatures(config, inputFeatures,
                                             ardaData.civLayer);
  mapTerrain();
  return true;
}

bool ArdaGen::loadDevelopment(Fwg::Cfg &config, const std::string &path) {
  Civilization::loadDevelopment(Fwg::IO::Reader::readGenericImage(path, config),
                                ardaProvinces, ardaRegions, ardaContinents);
  gatherStatistics();
  return true;
}
bool ArdaGen::genDevelopment(Fwg::Cfg &config) {
  Civilization::generateDevelopment(ardaProvinces, ardaRegions, ardaContinents);
  gatherStatistics();
  return true;
}
bool ArdaGen::loadPopulation(Fwg::Cfg &config,
                             const Fwg::Gfx::Image &inputPop) {
  ardaConfig.calculateTargetWorldPopulation();
  Civilization::loadPopulation(inputPop, ardaProvinces, ardaRegions,
                               ardaContinents,
                               ardaConfig.targetWorldPopulation);
  gatherStatistics();

  return true;
}
bool ArdaGen::genPopulation(Fwg::Cfg &config) {
  ardaConfig.calculateTargetWorldPopulation();
  Civilization::generatePopulation(civData, ardaProvinces, ardaRegions,
                                   ardaContinents,
                                   ardaConfig.targetWorldPopulation);
  gatherStatistics();

  return true;
}
void ArdaGen::genEconomyData() {
  ardaConfig.calculateTargetWorldGdp();
  Arda::Civilization::generateEconomyData(civData, ardaProvinces, ardaRegions,
                                          ardaContinents,
                                          ardaConfig.targetWorldGdp);
  gatherStatistics();
}

void ArdaGen::genCultureData() {
  Arda::Civilization::generateCultureData(civData, ardaProvinces, ardaRegions,
                                          ardaContinents, superRegions);
  gatherStatistics();
}

void ArdaGen::genCivilisationData() {
  genNaturalFeatures();
  ardaConfig.calculateTargetWorldPopulation();
  ardaConfig.calculateTargetWorldGdp();
  Arda::Civilization::generateFullCivilisationData(
      ardaRegions, ardaProvinces, civData, ardaContinents, superRegions,
      ardaConfig.targetWorldPopulation, ardaConfig.targetWorldGdp);
  genLocations();
  gatherStatistics();
}

void ArdaGen::clearLocations() {
  Fwg::Civilization::Locations::clearLocations(areaData.regions);
  locationMap = Arda::Gfx::displayLocations(areaData.regions, worldMap);
}

void ArdaGen::genLocations() {
  locationMap.clear();
  Fwg::Civilization::Locations::generateLocations(
      areaData.regions, terrainData, climateData, provinceMap, areaData,
      ardaConfig.locationConfig);
  locationMap = Arda::Gfx::displayLocations(areaData.regions, worldMap);
  Fwg::Gfx::Png::save(locationMap,
                      Fwg::Cfg::Values().mapsPath + "//world//locations.png");
  Arda::Civilization::applyCivilisationTopography(ardaData.civLayer,
                                                  ardaProvinces);
  mapTerrain();
}

void ArdaGen::genLocationType(const Fwg::Civilization::LocationType &type) {
  using namespace Arda::Civilization;
  using namespace Fwg::Civilization;
  // Mapping from location types to topography types
  static const std::unordered_map<LocationType, TopographyType> typeMap = {
      {LocationType::City, TopographyType::CITY},
      {LocationType::Port, TopographyType::PORTCITY},
      {LocationType::Farm, TopographyType::FARMLAND},
      {LocationType::Mine, TopographyType::MINE},
      {LocationType::Forest, TopographyType::FORESTRY},
  };
  ardaData.civLayer.clear(typeMap.at(type));
  Fwg::Civilization::Locations::generateLocationCategory(
      areaData.regions, terrainData, climateData, provinceMap, areaData,
      ardaConfig.locationConfig, type);
  locationMap = Arda::Gfx::displayLocations(areaData.regions, worldMap);
  Fwg::Gfx::Png::save(locationMap,
                      Fwg::Cfg::Values().mapsPath + "//world//locations.png");
  Arda::Civilization::applyCivilisationTopography(ardaData.civLayer,
                                                  ardaProvinces);
}

void ArdaGen::detectLocationType(const Fwg::Civilization::LocationType &type) {
  auto topotype = Arda::Civilization::TopographyType::CITY;
  if (type == Fwg::Civilization::LocationType::Farm) {
    topotype = Arda::Civilization::TopographyType::FARMLAND;
  } else if (type == Fwg::Civilization::LocationType::Forest) {
    topotype = Arda::Civilization::TopographyType::FORESTRY;
  } else if (type == Fwg::Civilization::LocationType::Mine) {
    topotype = Arda::Civilization::TopographyType::MINE;
  } else if (type == Fwg::Civilization::LocationType::Port) {
    topotype = Arda::Civilization::TopographyType::PORTCITY;
  } else if (type == Fwg::Civilization::LocationType::City) {
    topotype = Arda::Civilization::TopographyType::CITY;
  }

  Fwg::Civilization::Locations::detectLocationsFromPixels(
      ardaData.civLayer.getAll(topotype), provinceMap, areaData,
      areaData.regions, type);
  //Arda::Civilization::applyCivilisationTopography(ardaData.civLayer,
  //                                                ardaProvinces);
  mapTerrain();
}

void ArdaGen::loadLocations(const Fwg::Gfx::Image &inputImage) {
  auto &cfg = Fwg::Cfg::Values();
  clearLocations();
  loadNaturalFeatures(cfg, inputImage);
  detectLocationType(Fwg::Civilization::LocationType::Farm);
  detectLocationType(Fwg::Civilization::LocationType::City);
  detectLocationType(Fwg::Civilization::LocationType::Port);
  //detectLocationType(Fwg::Civilization::LocationType::Mine);
  //detectLocationType(Fwg::Civilization::LocationType::Forest);
  Arda::Civilization::applyCivilisationTopography(ardaData.civLayer,
                                                  ardaProvinces);
  locationMap = Arda::Gfx::displayLocations(areaData.regions, worldMap);
  mapTerrain();
}

void ArdaGen::genNavmesh(
    const std::vector<Fwg::Civilization::Locations::AreaLocationSet> &inputSet,
    const std::vector<std::shared_ptr<Fwg::Areas::Area>>
        &inputNavigationAreas) {
  // no input means we default to all locations in a region
  if (inputSet.size() == 0) {
    std::vector<Fwg::Civilization::Locations::AreaLocationSet> defaultInputSet;
    for (auto &region : areaData.regions) {
      Fwg::Civilization::Locations::AreaLocationSet regionLocationSet;
      regionLocationSet.area = region;
      for (auto &location : region->locations) {
        regionLocationSet.locations.push_back(location);
      }
      defaultInputSet.push_back(regionLocationSet);
    }
    Fwg::Civilization::Locations::generateConnections(
        defaultInputSet, terrainData, climateData, inputNavigationAreas);
  } else {
    Fwg::Civilization::Locations::generateConnections(
        inputSet, terrainData, climateData, inputNavigationAreas);
  }
  // navmeshMap = Fwg::Gfx::displayConnections(areaData.regions, locationMap);
}

bool ArdaGen::genWastelands(Fwg::Cfg &config) {
  Arda::NaturalFeatures::detectWastelands(terrainData, climateData, ardaRegions,
                                          ardaData.civLayer, config);
  auto worldOverlayMap = Arda::Gfx::displayWorldOverlayMap(
      climateData, worldMap, ardaData.civLayer);
  Fwg::Gfx::Png::save(worldOverlayMap, config.mapsPath + "worldOverlayMap.png",
                      true);

  return true;
}

void ArdaGen::generateStrategicRegions(
    std::function<std::shared_ptr<SuperRegion>()> factory) {
  Arda::Areas::generateStrategicRegions(factory, superRegions, ardaRegions,
                                        ardaConfig.superRegionFactor);
  Civilization::nameSuperRegions(superRegions, ardaRegions);
}

void ArdaGen::loadStrategicRegions(
    std::function<std::shared_ptr<SuperRegion>()> factory,
    const Fwg::Gfx::Image &inputImage) {
  Fwg::Utils::Logging::logLine("Loading Strategic Regions from Image");
  Fwg::Utils::Logging::logLine("Nothing happens...");
  Arda::Areas::loadStrategicRegions(inputImage, factory, superRegions,
                                    ardaRegions, terrainData);
  // Civilization::nameSuperRegions(superRegions, ardaRegions);
  Civilization::nameSuperRegions(superRegions, ardaRegions);
}

void ArdaGen::generateStateSpecifics() {
  Arda::Areas::saveRegions(ardaRegions,
                           Fwg::Cfg::Values().mapsPath + "//areas//",
                           Arda::Gfx::visualiseRegions(ardaRegions));
}

void ArdaGen::evaluateCountries() {}

Image ArdaGen::visualiseCountries(Fwg::Gfx::Image &countryBmp,
                                  const Fwg::Gfx::Image &worldMap,
                                  const int ID) {
  Logging::logLine("Drawing borders");
  auto &config = Fwg::Cfg::Values();
  const auto stateBorderColour = Fwg::Gfx::Colour(0, 0, 0);
  if (!countryBmp.initialised() || countryBmp.size() != config.processingArea) {
    countryBmp = Image(config.width, config.height, 24);
  }
  if (ID > -1) {
    const auto &region = ardaRegions[ID];
    Fwg::Gfx::Colour countryColour(0, 0, 0);

    if (region->owner) {
      countryColour = region->owner->colour;

      for (const auto &prov : region->provinces) {
        for (const auto pix : prov->getNonOwningPixelView()) {
          // do not overwrite state borders
          if (countryBmp.imageData[pix] != stateBorderColour) {
            countryBmp.imageData[pix] = countryColour;
          }
        }
      }
    } else {
      for (const auto &prov : region->provinces) {
        for (const auto pix : prov->getNonOwningPixelView()) {
          countryBmp.imageData[pix] = worldMap[pix];
        }
      }
    }
  } else {
    // Parallel loop over regions
    std::for_each(std::execution::par, ardaRegions.begin(), ardaRegions.end(),
                  [&](const auto &region) {
                    auto countryColour = Fwg::Gfx::Colour(0, 0, 0);

                    if (region->owner) {
                      countryColour = region->owner->colour;

                      // Fill provinces
                      for (const auto &prov : region->provinces) {
                        for (const auto pix : prov->getNonOwningPixelView()) {
                          countryBmp.imageData[pix] = countryColour;
                        }
                      }
                    } else {
                      for (const auto &prov : region->provinces) {
                        for (const auto pix : prov->getNonOwningPixelView()) {
                          countryBmp.imageData[pix] = worldMap[pix];
                        }
                      }
                    }
                  });

    Png::save(countryBmp, Fwg::Cfg::Values().mapsPath +
                              "countries/countries_no_borders.png");
    std::vector<std::shared_ptr<Country>> countriesVector;
    for (auto const &country : countries) {
      countriesVector.push_back(country.second);
    }
    Fwg::Gfx::applyAreaBorders(countryBmp, countriesVector);
    Png::save(countryBmp,
              Fwg::Cfg::Values().mapsPath + "countries/countries_borders.png");
    Fwg::Gfx::applyAreaBorders(countryBmp, ardaRegions);
    Png::save(countryBmp, Fwg::Cfg::Values().mapsPath +
                              "countries/countries_state_borders.png");
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
      for (const auto pix : prov->getNonOwningPixelView()) {
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

void ArdaGen::gatherStatistics() {
  // first gather all resources
  ardaStats.totalResources.clear();
  ardaStats.totalWorldPopulation = 0;
  ardaStats.totalWorldGdp = 0;
  for (const auto &reg : ardaRegions) {
    ardaStats.totalWorldPopulation += reg->totalPopulation;
    ardaStats.totalWorldGdp += reg->gdp;
    for (const auto &res : reg->resources) {
      if (ardaStats.totalResources.find(res.first) ==
          ardaStats.totalResources.end()) {
        ardaStats.totalResources[res.first] = 0;
      }
      ardaStats.totalResources[res.first] += res.second.amount;
    }
  }
}

void ArdaGen::printStatistics() {
  Logging::logLine("Printing Statistics");
  gatherStatistics();
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