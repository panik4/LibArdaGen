#include "civilisation/NaturalFeatures.h"

namespace Arda::NaturalFeatures {
void detectMarshes(Fwg::Terrain::TerrainData &terrainData,
                   Fwg::Climate::ClimateData &climateData,
                   Arda::Civilization::CivilizationLayer &civLayer,
                   const Fwg::Cfg &config) {
  const auto &heightMap = terrainData.detailedHeightMap;
  std::vector<float> marshMap(terrainData.landFormIds.size(), 0.0f);
  //for (size_t i = 0; i < terrainData.landFormIds.size(); i++) {
  //  const auto &landForm = terrainData.landFormIds[i];
  //  const auto &climateType =
  //      climateData.climates[i].getChances(0).second; // get dominant climate
  //  const auto &climate = climateData.climateClassDefinitions.at((int)climateType);
  //  // add altitude factor
  //  if (landForm.altitude > 0.0f && landForm.altitude < 0.1f &&
  //      landForm.inclination < 0.05f) {
  //    // add climate factor
  //    marshMap[i] += climate.referenceHumidity * 0.5f;
  //    // add inclination factor
  //    marshMap[i] += (1.0f - landForm.inclination) * 0.5f;
  //  }
  //}
  // debug visualize marsh map
  if (config.debugLevel > -1) {
    Fwg::Gfx::Bitmap marshBitmap(config.width, config.height, 24);
    for (size_t i = 0; i < marshMap.size(); i++) {
      marshBitmap.setColourAtIndex(
          i, Fwg::Gfx::Colour(static_cast<unsigned char>(0),
                              static_cast<unsigned char>(0),
                              static_cast<unsigned char>(std::min<float>(
                                  255.0f, marshMap[i] * 255.0f))));
    }
    Fwg::Gfx::Png::save(marshBitmap, config.mapsPath + "marshes.png", false);
  }
}
std::vector<float>
detectWastelands(Fwg::Terrain::TerrainData &terrainData,
                 Fwg::Climate::ClimateData &climateData,
                 std::vector<std::shared_ptr<Arda::ArdaRegion>> &ardaRegions,
                 Arda::Civilization::CivilizationLayer &civLayer,
                 const Fwg::Cfg &config) {
  // sum up factors, such as altitude, climate zone habitability
  std::vector<float> wastelandMap(terrainData.landFormIds.size(), 0.0f);
  // for (size_t i = 0; i < terrainData.landFormIds.size(); i++) {
  //   const auto &landForm = terrainData.landFormIds[i];
  //   const auto &climateType =
  //       climateData.climates[i].getChances(0).second; // get dominant climate
  //   const auto &climate = climateData.climateClassDefinitions.at((int)climateType);
  //   // add altitude factor
  //   if (landForm.altitude > 0.2) {
  //     wastelandMap[i] += (landForm.altitude - 0.2) * 0.33f;
  //   }
  //   // add climate factor
  //   wastelandMap[i] += -climate.arability * 0.33f;
  //   // add inclination factor
  //   wastelandMap[i] += landForm.inclination * 0.33f;
  //   wastelandMap[i] += (1.0f - climateData.habitabilities[i]);
  // }
  //// normalize the wasteland map
  //// Fwg::Utils::normalizeVector(wastelandMap, 0.0f, 1.0f);

  //// visualize wasteland map
  // if (config.debugLevel > 1) {
  //   Fwg::Gfx::Bitmap wastelandBitmap(config.width, config.height, 24);
  //   for (size_t i = 0; i < wastelandMap.size(); i++) {
  //     wastelandBitmap.setColourAtIndex(
  //         i, Fwg::Gfx::Colour(static_cast<unsigned char>(0),
  //                             static_cast<unsigned char>(std::min<float>(
  //                                 255.0f, wastelandMap[i] * 255.0f)),
  //                             static_cast<unsigned char>(0)));
  //   }
  //   Fwg::Gfx::Png::save(wastelandBitmap, config.mapsPath + "wastelands.png",
  //                       false);
  // }
  // civLayer.wastelandChance = wastelandMap;
  for (auto &region : ardaRegions) {
    float totalRegionHabitability = 0.0f;
    for (const auto &province : region->ardaProvinces) {
      // get average habitability of the province, if below 0.2, mark as
      // wasteland
      float totalHabitability = 0.0f;
      for (const auto &pix : province->pixels) {
        totalHabitability += climateData.habitabilities[pix];
        totalRegionHabitability += climateData.habitabilities[pix];
      }
      float avgHabitability =
          totalHabitability / static_cast<float>(province->pixels.size());
      if (avgHabitability < 0.1f) {
        for (const auto &pix : province->pixels) {
          wastelandMap[pix] = 1.0f;
        }
        province->topographyTypes.insert(
            Arda::Civilization::TopographyType::WASTELAND);
      }
    }
    float avgRegionHabitability =
        totalRegionHabitability /
        static_cast<float>(region->getNonOwningPixelView().size());
    if (avgRegionHabitability < config.lowDensityHabitatThreshold) {
      region->topographyTypes.insert(
          Arda::Civilization::TopographyType::WASTELAND);
      std::cout << "Region " << region->name
                << " marked as WASTELAND due to low habitability of "
                << avgRegionHabitability << std::endl;
    }
  }
  return wastelandMap;
}

void NaturalFeatures::loadNaturalFeatures(
    Fwg::Cfg &config, const Fwg::Gfx::Bitmap &inputFeatures,
    Arda::Civilization::CivilizationLayer &civLayer) {
  std::map<Fwg::Gfx::Colour, Arda::Civilization::TopographyType> colourMap = {
      {config.topographyOverlayColours.at("marsh"),
       Arda::Civilization::TopographyType::MARSH},
      {config.topographyOverlayColours.at("agriculture"),
       Arda::Civilization::TopographyType::FARMLAND},
      {config.topographyOverlayColours.at("wasteland"),
       Arda::Civilization::TopographyType::WASTELAND},
      {config.topographyOverlayColours.at("urban"),
       Arda::Civilization::TopographyType::CITY},
      {config.topographyOverlayColours.at("portcity"),
       Arda::Civilization::TopographyType::PORTCITY},
      {config.topographyOverlayColours.at("forestry"),
       Arda::Civilization::TopographyType::FORESTRY},
      {config.topographyOverlayColours.at("mining"),
       Arda::Civilization::TopographyType::MINE}

  };
  if (inputFeatures.size() == config.bitmapSize) {
    for (int i = 0; i < inputFeatures.size(); i++) {
      const auto &colour = inputFeatures[i];
      auto it = colourMap.find(colour);
      if (it != colourMap.end()) {
        civLayer.set(i, it->second);
      }
    }
  }
}

} // namespace Arda::NaturalFeatures