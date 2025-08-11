#include "ArdaGen.h"
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
  if (factory != nullptr) {
    Arda::Countries::generateCountries(factory, numCountries, ardaRegions,
                                       countries, ardaProvinces, civData,
                                       nData);
  }
  //  first gather generic neighbours, they will be mapped to hoi4 countries
  //  in mapCountries
  Arda::Countries::evaluateCountryNeighbours(areaData.regions, ardaRegions,
                                             countries);
  Fwg::Utils::Logging::logLine("Visualising Countries");
  visualiseCountries(countryMap);
  Fwg::Gfx::Png::save(countryMap,
                      Fwg::Cfg::Values().mapsPath + "countries.png");
  mapCountries();
  evaluateCountries();
  Arda::Countries::saveCountries(
      countries, Fwg::Cfg::Values().mapsPath + "//areas//", Arda::Gfx::visualiseCountries(countries));
}

void ArdaGen::loadCountries(std::function<std::shared_ptr<Country>()> factory,
                            const Fwg::Gfx::Bitmap &inputImage) {
  // generate country data
  if (factory != nullptr) {
    Arda::Countries::loadCountries(factory, ardaRegions, countries, civData,
                                   nData, inputImage, countryMappingPath);
  }
  Arda::Countries::evaluateCountryNeighbours(areaData.regions, ardaRegions,
                                             countries);
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

void ArdaGen::generateStrategicRegions(
    std::function<std::shared_ptr<SuperRegion>()> factory) {
  Arda::Areas::generateStrategicRegions(factory, superRegions, ardaRegions,
                                        superRegionFactor);
  Civilization::nameSuperRegions(superRegions, ardaRegions);
}

void ArdaGen::loadStrategicRegions(
    std::function<std::shared_ptr<SuperRegion>()> factory,
    const Fwg::Gfx::Bitmap &inputImage) {
  Civilization::nameSuperRegions(superRegions, ardaRegions);
}

void ArdaGen::generateStateSpecifics() {
  Arda::Areas::saveRegions(ardaRegions,
                           Fwg::Cfg::Values().mapsPath + "//areas//",
                           Arda::Gfx::visualiseRegions(ardaRegions));
}

void ArdaGen::evaluateCountries() {}

Bitmap ArdaGen::visualiseCountries(Fwg::Gfx::Bitmap &countryBmp, const int ID) {
  Logging::logLine("Drawing borders");
  auto &config = Fwg::Cfg::Values();
  if (!countryBmp.initialised()) {
    countryBmp = Bitmap(config.width, config.height, 24);
  }
  if (ID > -1) {
    const auto &region = ardaRegions[ID];
    Fwg::Gfx::Colour countryColour(0, 0, 0);

    if (region->owner) {
      countryColour = region->owner->colour;
    }

    const auto borderColour = countryColour * 0.0;

    for (const auto &prov : region->provinces) {
      const auto mixedColour = countryColour * 0.9 + prov->colour * 0.1;

      for (const auto pix : prov->getNonOwningPixelView()) {
        countryBmp.imageData[pix] = mixedColour;
      }
    }

    for (auto pix : region->borderPixels) {
      countryBmp.imageData[pix] = borderColour;
    }
  } else {
    Fwg::Gfx::Bitmap noBorderCountries(config.width, config.height, 24);

    // Parallel loop over regions
    std::for_each(std::execution::par, ardaRegions.begin(), ardaRegions.end(),
                  [&](const auto &region) {
                    auto countryColour = Fwg::Gfx::Colour(0, 0, 0);

                    if (region->owner) {
                      countryColour = region->owner->colour;
                    }

                    // Fill provinces
                    for (const auto &prov : region->provinces) {
                      for (const auto pix : prov->getNonOwningPixelView()) {
                        countryBmp.imageData[pix] =
                            countryColour * 0.9 + prov->colour * 0.1;

                        noBorderCountries.imageData[pix] = countryColour;
                      }
                    }

                    // Fill borders
                    for (auto pix : region->borderPixels) {
                      countryBmp.imageData[pix] = countryColour * 0.0;
                    }
                  });

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