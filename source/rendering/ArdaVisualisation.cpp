#include "rendering/ArdaVisualisation.h"

namespace Arda::Gfx {
Fwg::Gfx::Image displayDevelopment(
    const std::vector<std::shared_ptr<Arda::ArdaProvince>> provinces) {
  auto &config = Fwg::Cfg::Values();
  Fwg::Gfx::Image averageDevelopment(config.width, config.height, 24);
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
Fwg::Gfx::Image displayPopulation(
    const std::vector<std::shared_ptr<Arda::ArdaProvince>> provinces) {
  const auto &config = Fwg::Cfg::Values();
  Fwg::Gfx::Image populationMap(config.width, config.height, 24);
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
Fwg::Gfx::Image
displayTopography(const Arda::Civilization::CivilizationLayer &civLayer,
                  Fwg::Gfx::Image worldMap) {
  auto cityIndices = civLayer.getAll(Arda::Civilization::TopographyType::CITY);
  auto portcityIndices =
      civLayer.getAll(Arda::Civilization::TopographyType::PORTCITY);
  auto farmIndices =
      civLayer.getAll(Arda::Civilization::TopographyType::FARMLAND);
  auto marshIndices =
      civLayer.getAll(Arda::Civilization::TopographyType::MARSH);
  for (const auto pix : cityIndices) {
    worldMap.setColourAtIndex(
        pix, Fwg::Cfg::Values().topographyOverlayColours.at("urban"));
  }
  for (const auto pix : portcityIndices) {
    worldMap.setColourAtIndex(
        pix, Fwg::Cfg::Values().topographyOverlayColours.at("portcity"));
  }
  for (const auto pix : farmIndices) {
    worldMap.setColourAtIndex(
        pix, Fwg::Cfg::Values().topographyOverlayColours.at("agriculture"));
  }
  for (const auto pix : marshIndices) {
    worldMap.setColourAtIndex(
        pix, Fwg::Cfg::Values().topographyOverlayColours.at("marsh"));
  }
  return worldMap;
}
Fwg::Gfx::Image displayCultureGroups(
    const std::vector<std::shared_ptr<ArdaProvince>> &ardaProvinces) {
  auto &config = Fwg::Cfg::Values();
  Fwg::Gfx::Image cultureMap(config.width, config.height, 24);
  for (auto &ardaProvince : ardaProvinces) {
    if (ardaProvince->isSea() || ardaProvince->isLake())
      continue;
    for (auto &culture : ardaProvince->cultures) {
      for (const auto pix : ardaProvince->getNonOwningPixelView()) {
        cultureMap.setColourAtIndex(pix,
                                    culture.first->cultureGroup->getColour());
      }
    }
  }

  Fwg::Gfx::Png::save(cultureMap,
                      Fwg::Cfg::Values().mapsPath + "/world/cultureGroups.png");
  return cultureMap;
}

Fwg::Gfx::Image displayCultures(
    const std::vector<std::shared_ptr<ArdaProvince>> &ardaProvinces) {
  auto &config = Fwg::Cfg::Values();
  Fwg::Gfx::Image cultureMap(config.width, config.height, 24);
  // now write the cultures to the culture map
  for (auto &ardaProvince : ardaProvinces) {
    if (ardaProvince->isSea() || ardaProvince->isLake())
      continue;
    for (auto &culture : ardaProvince->cultures) {
      for (const auto pix : ardaProvince->getNonOwningPixelView()) {
        cultureMap.setColourAtIndex(pix, culture.first->colour);
      }
    }
  }

  Fwg::Gfx::Png::save(cultureMap,
                      Fwg::Cfg::Values().mapsPath + "/world/cultures.png");
  return cultureMap;
}

Fwg::Gfx::Image displayReligions(
    const std::vector<std::shared_ptr<ArdaProvince>> &ardaProvinces) {
  auto &config = Fwg::Cfg::Values();
  Fwg::Gfx::Image religionMap(config.width, config.height, 24);
  // now write the cultures to the culture map
  for (auto &ardaProvince : ardaProvinces) {
    if (ardaProvince->isSea() || ardaProvince->isLake())
      continue;
    for (auto &religion : ardaProvince->religions) {
      for (const auto pix : ardaProvince->getNonOwningPixelView()) {
        religionMap.setColourAtIndex(pix, religion.first->colour);
      }
    }
  }

  Fwg::Gfx::Png::save(religionMap,
                      Fwg::Cfg::Values().mapsPath + "/world/religions.png");
  return religionMap;
}

Fwg::Gfx::Image displayLanguageGroups(
    const std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions) {
  auto &config = Fwg::Cfg::Values();
  Fwg::Gfx::Image languageMap(config.width, config.height, 24);

  return Fwg::Gfx::Image();
}

Fwg::Gfx::Image
displayLocations(std::vector<std::shared_ptr<Fwg::Areas::Region>> &regions,
                 Fwg::Gfx::Image worldMap) {
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

Fwg::Gfx::Image displayConnections(
    const std::vector<std::shared_ptr<Fwg::Areas::Region>> &regions,
    Fwg::Gfx::Image connectionMap) {
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

Fwg::Gfx::Image
displayWorldOverlayMap(const Fwg::Climate::ClimateData &climateData,
                       const Fwg::Gfx::Image &worldMap,
                       const Arda::Civilization::CivilizationLayer &civLayer) {
  Fwg::Utils::Logging::logLine("Visualising World Overlay Map");

  // overlay the wasteland on the world map
  Fwg::Gfx::Image overlayMap(worldMap);
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

Fwg::Gfx::Image generateStrategicRegionTemplate(
    const std::vector<std::shared_ptr<Fwg::Areas::Province>> &provinces,
    const std::vector<std::shared_ptr<Fwg::Areas::Region>> &regions) {
  const auto &cfg = Fwg::Cfg::Values();
  Fwg::Gfx::Image resultImage(cfg.width, cfg.height, 24);
  Fwg::Gfx::regionMap(regions, provinces, resultImage);
  Fwg::Gfx::applyAreaBorders(resultImage, regions, {255, 255, 255});

  Fwg::Gfx::Png::save(
      resultImage, cfg.mapsPath + "areas/strategicRegionTemplate.png", false);
  return resultImage;
}

Fwg::Gfx::Image visualiseStrategicRegions(
    Fwg::Gfx::Image &superRegionMap,
    const std::vector<std::shared_ptr<Arda::SuperRegion>> &superRegions,
    const int ID) {
  if (!superRegionMap.initialised()) {
    superRegionMap = Fwg::Gfx::Image(Fwg::Cfg::Values().width,
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
    auto noBorderMap = Fwg::Gfx::Image(Fwg::Cfg::Values().width,
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

Fwg::Gfx::Image
visualiseRegions(const std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions) {
  auto regionMap =
      Fwg::Gfx::Image(Fwg::Cfg::Values().width, Fwg::Cfg::Values().height, 24);

  for (const auto &region : ardaRegions) {
    for (const auto pix : region->getNonOwningPixelView()) {
      regionMap.setColourAtIndex(pix, region->colour);
    }
  }
  return regionMap;
}

Fwg::Gfx::Image visualiseCountries(
    const std::map<std::string, std::shared_ptr<Country>> &countries) {
  Fwg::Utils::Logging::logLine("Drawing borders");
  auto &config = Fwg::Cfg::Values();
  auto countryBmp = Fwg::Gfx::Image(config.width, config.height, 24);

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
