#include "rendering/ArdaVisualisation.h"

namespace Arda::Gfx {

Fwg::Gfx::Bitmap
displayCultureGroups(const Arda::Civilization::CivilizationData &civData) {
  auto &config = Fwg::Cfg::Values();
  Fwg::Gfx::Bitmap cultureMap(config.width, config.height, 24);
  for (auto &cultureGroup : civData.cultureGroups) {
    for (auto &ardaRegion : cultureGroup->getRegions())
      // add only the main culture at this time
      for (auto &province : ardaRegion->ardaProvinces) {
        for (const auto pix : province->baseProvince->getNonOwningPixelView()) {
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
    for (auto &culture : ardaRegion->cultureShares) {
      for (auto &province : ardaRegion->ardaProvinces) {
        for (const auto pix : province->baseProvince->getNonOwningPixelView()) {
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
        for (const auto pix : province->baseProvince->getNonOwningPixelView()) {
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
        for (auto &pix : prov->baseProvince->pixels) {
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
          for (auto &pix : prov->baseProvince->pixels) {
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
        for (const auto pix : prov->baseProvince->getNonOwningPixelView()) {
          countryBmp.setColourAtIndex(pix, countryColour);
        }
      }
    }
  }

  return countryBmp;
}

} // namespace Arda::Gfx
