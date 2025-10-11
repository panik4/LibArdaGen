#include "rendering/ArdaVisualisation.h"

namespace Arda::Gfx {
Fwg::Gfx::Bitmap displayDevelopment(
    const std::vector<std::shared_ptr<Arda::ArdaProvince>> provinces) {
  auto &config = Fwg::Cfg::Values();
  Fwg::Gfx::Bitmap averageDevelopment(config.width, config.height, 24);
  for (const auto &province : provinces) {
    for (const auto pixel : province->getNonOwningPixelView()) {
      averageDevelopment.setColourAtIndex(
          pixel, (unsigned char)(province->averageDevelopment * 255.0f));
    }
  }
  if (config.debugLevel > 5)
    Fwg::Gfx::Png::save(averageDevelopment,
                        config.mapsPath + "world//development.png");
  return averageDevelopment;
}
Fwg::Gfx::Bitmap displayPopulation(
    const std::vector<std::shared_ptr<Arda::ArdaProvince>> provinces) {
  const auto &config = Fwg::Cfg::Values();
  Fwg::Gfx::Bitmap populationMap(config.width, config.height, 24);
  populationMap.fill(Fwg::Gfx::Colour(0, 0, 0));
  for (const auto &prov : provinces) {
    if (prov->isSea())
      continue;
    for (const auto pixel : prov->getNonOwningPixelView()) {
      populationMap.setColourAtIndex(pixel, config.colours.at("population") *
                                                prov->populationDensity);
    }
  }
  return populationMap;
}
Fwg::Gfx::Bitmap
displayCultureGroups(const Arda::Civilization::CivilizationData &civData) {
  auto &config = Fwg::Cfg::Values();
  Fwg::Gfx::Bitmap cultureMap(config.width, config.height, 24);
  for (auto &cultureGroup : civData.cultureGroups) {
    for (auto &ardaRegion : cultureGroup->getRegions())
      // add only the main culture at this time
      for (auto &province : ardaRegion->ardaProvinces) {
        for (const auto pix : province->getNonOwningPixelView()) {
          cultureMap.setColourAtIndex(pix, cultureGroup->getColour());
        }
      }
  }

  Fwg::Gfx::Png::save(cultureMap,
                      Fwg::Cfg::Values().mapsPath + "/world/cultureGroups.png");
  return cultureMap;
}

Fwg::Gfx::Bitmap
displayCultures(const std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions) {
  auto &config = Fwg::Cfg::Values();
  Fwg::Gfx::Bitmap cultureMap(config.width, config.height, 24);
  // now write the cultures to the culture map
  for (auto &ardaRegion : ardaRegions) {
    if (ardaRegion->isSea() || ardaRegion->isLake())
      continue;
    for (auto &culture : ardaRegion->cultures) {
      for (auto &province : ardaRegion->ardaProvinces) {
        for (const auto pix : province->getNonOwningPixelView()) {
          cultureMap.setColourAtIndex(pix, culture.first->colour);
        }
      }
    }
  }

  Fwg::Gfx::Png::save(cultureMap,
                      Fwg::Cfg::Values().mapsPath + "/world/cultures.png");
  return cultureMap;
}

Fwg::Gfx::Bitmap
displayReligions(const std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions) {
  auto &config = Fwg::Cfg::Values();
  Fwg::Gfx::Bitmap religionMap(config.width, config.height, 24);
  // now write the cultures to the culture map
  for (auto &ardaRegion : ardaRegions) {
    if (ardaRegion->isSea() || ardaRegion->isLake())
      continue;
    for (auto &religion : ardaRegion->religions) {
      for (auto &province : ardaRegion->ardaProvinces) {
        for (const auto pix : province->getNonOwningPixelView()) {
          religionMap.setColourAtIndex(pix, religion.first->colour);
        }
      }
    }
  }

  Fwg::Gfx::Png::save(religionMap,
                      Fwg::Cfg::Values().mapsPath + "/world/religions.png");
  return religionMap;
}

Fwg::Gfx::Bitmap displayLanguageGroups(
    const std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions) {
  auto &config = Fwg::Cfg::Values();
  Fwg::Gfx::Bitmap languageMap(config.width, config.height, 24);

  return Fwg::Gfx::Bitmap();
}

Fwg::Gfx::Bitmap
displayLocations(std::vector<std::shared_ptr<Fwg::Areas::Region>> &regions,
                 Fwg::Gfx::Bitmap worldMap) {
  for (auto &region : regions) {
    for (auto &location : region->locations) {
      for (auto pixel : location->pixels) {
        worldMap.setColourAtIndex(
            pixel,
            Fwg::Civilization::Location::locationColours.at(location->type));
      }
    }
  }
  return worldMap;
}

Fwg::Gfx::Bitmap displayConnections(
    const std::vector<std::shared_ptr<Fwg::Areas::Region>> &regions,
    Fwg::Gfx::Bitmap connectionMap) {
  auto &config = Fwg::Cfg::Values();
  // now visualise connections again
  for (auto &region : regions) {
    for (auto &loc : region->locations) {
      for (auto &conn : loc->connections) {
        for (int x = 0; x < conn.second.connectingPixels.size(); x++) {
          connectionMap.setColourAtIndex(conn.second.connectingPixels[x],
                                         {255, 255, 255 - x});
          // Paint sourceLoc and destLoc
          connectionMap.setColourAtIndex(
              conn.second.source->position.weightedCenter, {0, 255, 255});
          connectionMap.setColourAtIndex(
              conn.second.destination->position.weightedCenter, {255, 0, 0});
        }
      }
    }
  }

  Fwg::Gfx::Png::save(connectionMap, config.mapsPath + "world/connections.png");
  return connectionMap;
}

Fwg::Gfx::Bitmap
displayWorldOverlayMap(const Fwg::Climate::ClimateData &climateData,
                       const Fwg::Gfx::Bitmap &worldMap,
                       const Arda::Civilization::CivilizationLayer &civLayer) {
  Fwg::Utils::Logging::logLine("Visualising World Overlay Map");

  // overlay the wasteland on the world map
  Fwg::Gfx::Bitmap overlayMap(worldMap);
  const auto &config = Fwg::Cfg::Values();
  for (size_t i = 0; i < civLayer.wastelandChance.size(); i++) {
    if (civLayer.wastelandChance[i] > config.wastelandThreshold) {
      overlayMap.setColourAtIndex(
          i, overlayMap[i] * 0.5f + config.colours.at("wasteland") *
                                        (0.5f * civLayer.wastelandChance[i]));
    }
  }

  return overlayMap;
}

Fwg::Gfx::Bitmap visualiseStrategicRegions(
    Fwg::Gfx::Bitmap &superRegionMap,
    const std::vector<std::shared_ptr<Arda::SuperRegion>> &superRegions,
    const int ID) {
  if (!superRegionMap.initialised()) {
    superRegionMap = Fwg::Gfx::Bitmap(Fwg::Cfg::Values().width,
                                      Fwg::Cfg::Values().height, 24);
  }
  if (ID > -1) {
    auto &strat = superRegions[ID];
    for (auto &reg : strat->ardaRegions) {
      for (auto &prov : reg->ardaProvinces) {
        for (auto &pix : prov->pixels) {
          superRegionMap.setColourAtIndex(pix, strat->colour);
        }
      }
      for (auto &pix : reg->borderPixels) {
        superRegionMap.setColourAtIndex(pix, strat->colour * 0.5);
      }
    }
  } else {
    auto noBorderMap = Fwg::Gfx::Bitmap(Fwg::Cfg::Values().width,
                                        Fwg::Cfg::Values().height, 24);
    for (auto &strat : superRegions) {

      for (auto &reg : strat->ardaRegions) {
        for (auto &prov : reg->ardaProvinces) {
          for (auto &pix : prov->pixels) {
            superRegionMap.setColourAtIndex(pix, strat->colour);
            if (ID == -1) {
              noBorderMap.setColourAtIndex(pix, strat->colour);
            }
          }
        }
        for (auto &pix : reg->borderPixels) {
          if (strat->centerOutsidePixels) {
            superRegionMap.setColourAtIndex(pix,
                                            Fwg::Gfx::Colour(255, 255, 255));
          } else {
            superRegionMap.setColourAtIndex(pix, strat->colour * 0.5);
          }
        }
      }
    }
    Fwg::Gfx::Png::save(noBorderMap, Fwg::Cfg::Values().mapsPath +
                                         "superRegions_no_borders.png");
    Fwg::Gfx::Png::save(superRegionMap,
                        Fwg::Cfg::Values().mapsPath + "superRegions.png");
  }
  return superRegionMap;
}

Fwg::Gfx::Bitmap
visualiseRegions(const std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions) {
  auto regionMap =
      Fwg::Gfx::Bitmap(Fwg::Cfg::Values().width, Fwg::Cfg::Values().height, 24);

  for (const auto &region : ardaRegions) {
    for (const auto pix : region->getNonOwningPixelView()) {
      regionMap.setColourAtIndex(pix, region->colour);
    }
  }
  return regionMap;
}

Fwg::Gfx::Bitmap visualiseCountries(
    const std::map<std::string, std::shared_ptr<Country>> &countries) {
  Fwg::Utils::Logging::logLine("Drawing borders");
  auto &config = Fwg::Cfg::Values();
  auto countryBmp = Fwg::Gfx::Bitmap(config.width, config.height, 24);

  for (auto &country : countries) {
    for (auto &region : country.second->ownedRegions) {
      auto countryColour = country.second->colour;
      // Fill provinces
      for (const auto &prov : region->ardaProvinces) {
        for (const auto pix : prov->getNonOwningPixelView()) {
          countryBmp.setColourAtIndex(pix, countryColour);
        }
      }
    }
  }

  return countryBmp;
}

} // namespace Arda::Gfx
