#include "rendering/ArdaVisualisation.h"

namespace Arda::Gfx {

Fwg::Gfx::Bitmap Arda::Gfx::displayCultureGroups(
    const Arda::Civilization::CivilizationData &civData) {
  auto &config = Fwg::Cfg::Values();
  Fwg::Gfx::Bitmap cultureMap(config.width, config.height, 24);
  for (auto &cultureGroup : civData.cultureGroups) {
    for (auto &ardaRegion : cultureGroup->getRegions())
      // add only the main culture at this time
      for (auto &province : ardaRegion->ardaProvinces) {
        for (auto pix : province->baseProvince->pixels) {
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
        for (auto pix : province->baseProvince->pixels) {
          cultureMap.setColourAtIndex(pix, culture.first->colour);
        }
      }
    }
  }

  Fwg::Gfx::Png::save(cultureMap,
                      Fwg::Cfg::Values().mapsPath + "/world/cultures.png");
  return cultureMap;
}

} // namespace Arda::Gfx
