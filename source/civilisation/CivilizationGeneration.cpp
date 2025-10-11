#include "civilisation/CivilizationGeneration.h"

namespace Arda::Civilization {
void generateEconomyData(
    CivilizationData &civData,
    std::vector<std::shared_ptr<Arda::ArdaProvince>> &ardaProvinces,
    std::vector<std::shared_ptr<Arda::ArdaRegion>> &regions,
    std::vector<std::shared_ptr<ArdaContinent>> &continents,
    const double targetWorldGdp) {
  generateEconomicActivity(civData, ardaProvinces, regions, continents,
                           targetWorldGdp);
  generateImportance(regions);
}

void generateCultureData(
    CivilizationData &civData,
    std::vector<std::shared_ptr<Arda::ArdaProvince>> &ardaProvinces,
    std::vector<std::shared_ptr<Arda::ArdaRegion>> &regions,
    std::vector<std::shared_ptr<ArdaContinent>> &continents,
    std::vector<std::shared_ptr<SuperRegion>> &superRegions) {
  generateReligions(civData, ardaProvinces);
  generateCultures(civData, regions);
  Arda::Gfx::displayCultureGroups(civData);
  Arda::Gfx::displayCultures(regions);
  distributeLanguages(civData);
  nameRegions(regions);
  nameContinents(continents, regions);
  Arda::Areas::saveRegions(regions, Fwg::Cfg::Values().mapsPath + "//areas//",
                           Arda::Gfx::visualiseRegions(regions));
}

void generateFullCivilisationData(
    std::vector<std::shared_ptr<Arda::ArdaRegion>> &regions,
    std::vector<std::shared_ptr<Arda::ArdaProvince>> &ardaProvinces,
    CivilizationData &civData,
    std::vector<std::shared_ptr<ArdaContinent>> &continents,
    std::vector<std::shared_ptr<SuperRegion>> &superRegions,
    const double targetWorldPopulation, const double targetWorldGdp) {
  generateDevelopment(ardaProvinces, regions, continents);
  generatePopulation(civData, ardaProvinces, regions, continents,
                     targetWorldPopulation);
  generateEconomyData(civData, ardaProvinces, regions, continents,
                      targetWorldPopulation);
  generateCultureData(civData, ardaProvinces, regions, continents,
                      superRegions);
}

void loadDevelopment(
    const Fwg::Gfx::Bitmap &developmentMap,
    const std::vector<std::shared_ptr<Arda::ArdaProvince>> &provinces,
    const std::vector<std::shared_ptr<ArdaRegion>> &regions,
    const std::vector<std::shared_ptr<ArdaContinent>> &continents) {

  for (auto &prov : provinces) {
    if (prov->isLand()) {
      prov->averageDevelopment =
          (double)developmentMap[prov->position.weightedCenter].getBlue() /
          255.0;
    }
  }
}

void generateDevelopment(
    const std::vector<std::shared_ptr<Arda::ArdaProvince>> &provinces,
    const std::vector<std::shared_ptr<ArdaRegion>> &regions,
    const std::vector<std::shared_ptr<ArdaContinent>> &continents) {
  Fwg::Utils::Logging::logLine("Generating State Development");
  auto &config = Fwg::Cfg::Values();

  const auto height = config.height;
  const auto width = config.width;
  FastNoiseLite noiseGenerator;
  noiseGenerator.SetFractalOctaves(11);
  noiseGenerator.SetFractalGain(0.5);
  noiseGenerator.SetNoiseType(
      FastNoiseLite::NoiseType_ValueCubic); // Set the desired noise type
  noiseGenerator.SetFractalType(FastNoiseLite::FractalType_PingPong);
  // sparse trees means more frequent, but smaller patches
  noiseGenerator.SetSeed(config.mapSeed);
  noiseGenerator.SetFrequency(0.02 / config.sizeFactor);
  auto developmentNoise = Fwg::Noise::genNoise(noiseGenerator, height, width,
                                               0.0, 0.0, 0.0, 0.2, 0.0);
  for (auto &continent : continents) {
    if (config.randomDevelopment) {
      continent->developmentModifier = RandNum::getRandom<double>(
          config.minimumDevelopment, config.maximumDevelopment);
    }
    for (auto &province : continent->provinces) {
      province->averageDevelopment = 0.0f;
      // skip lake and sea provinces
      if (!province->isLand())
        continue;
      // get the average developmentNoise for the province
      const auto provincePixels = province->getNonOwningPixelView();
      for (const auto pixel : provincePixels) {
        province->averageDevelopment +=
            (float)developmentNoise[pixel] / provincePixels.size();
      }
      province->averageDevelopment /= provincePixels.size();

      province->averageDevelopment = std::clamp(
          province->averageDevelopment +
              0.8 * province->habitability * continent->developmentModifier,
          0.0, 1.0);
      if (province->averageDevelopment < 0.0f) {
        Fwg::Utils::Logging::logLine("ERROR: averageDevelopment is negative: ",
                                     province->averageDevelopment);
      } else if (province->averageDevelopment > 1.0f) {
        Fwg::Utils::Logging::logLine(
            "ERROR: averageDevelopment is greater than 1: ",
            province->averageDevelopment);
      }
    }
  }
}

void postProcessPopulation(
    const std::vector<std::shared_ptr<Arda::ArdaProvince>> &provinces,
    const std::vector<std::shared_ptr<ArdaRegion>> &regions,
    const std::vector<std::shared_ptr<ArdaContinent>> &continents,
    const double worldPopulationFactorSum, const double targetWorldPopulation) {
  // now calculate each provinces share of the world population
  for (auto &province : provinces) {
    if (province->isLand()) {
      auto sum = province->populationDensity *
                 province->getNonOwningPixelView().size();
      province->worldPopulationShare = sum / worldPopulationFactorSum;

    } else {
      province->worldPopulationShare = 0.0;
    }
    province->population =
        std::lround(province->worldPopulationShare *
                    static_cast<double>(targetWorldPopulation));
  }

  for (auto &gR : regions) {
    gR->worldPopulationShare = 0.0;
    gR->totalPopulation = 0;
    for (auto &province : gR->ardaProvinces) {
      gR->worldPopulationShare += province->worldPopulationShare;
      gR->totalPopulation += province->population;
    }
  }
}

void loadPopulation(
    const Fwg::Gfx::Bitmap &populationMap,
    const std::vector<std::shared_ptr<Arda::ArdaProvince>> &provinces,
    const std::vector<std::shared_ptr<ArdaRegion>> &regions,
    const std::vector<std::shared_ptr<ArdaContinent>> &continents,
    double targetWorldPopulation) {
  Fwg::Utils::Logging::logLine("Civilization::Populating the world\n");
  double worldPopulationFactorSum = 0.0;
  for (auto &prov : provinces) {
    prov->populationDensity = 0.0f;
    if (prov->isSea())
      continue;
    prov->populationDensity =
        (double)populationMap[prov->position.weightedCenter].getRed() / 255.0;

    // add all the population density times the size to get the total of the
    // world
    worldPopulationFactorSum +=
        prov->populationDensity * prov->getNonOwningPixelView().size();
  }
  postProcessPopulation(provinces, regions, continents,
                        worldPopulationFactorSum, targetWorldPopulation);
}

void generatePopulation(
    CivilizationData &civData,
    const std::vector<std::shared_ptr<Arda::ArdaProvince>> &provinces,
    const std::vector<std::shared_ptr<ArdaRegion>> &regions,
    const std::vector<std::shared_ptr<ArdaContinent>> &continents,
    double targetWorldPopulation) {
  Fwg::Utils::Logging::logLine("Generating Population");
  const auto &config = Fwg::Cfg::Values();

  double worldPopulationFactorSum = 0.0;
  for (auto &province : provinces) {
    province->populationDensity = 0.0f;
    // skip lake and sea provinces
    if (province->isLand()) {
      province->populationDensity = std::clamp(
          static_cast<double>(province->habitability) *
              ((config.developmentInfluence) +
               (config.developmentInfluence * province->averageDevelopment)),
          0.0, 1.0);
      // add all the population density times the size to get the total of the
      // world
      worldPopulationFactorSum += province->populationDensity *
                                  province->getNonOwningPixelView().size();
      if (province->populationDensity < 0.0f) {
        Fwg::Utils::Logging::logLine("ERROR: populationDensity is negative: ",
                                     province->populationDensity);
      }
    }
  }

  postProcessPopulation(provinces, regions, continents,
                        worldPopulationFactorSum, targetWorldPopulation);
  civData.worldPopulationFactorSum = worldPopulationFactorSum;
}

/* Very simple calculation of economic activity. The modules can override this
 * to implement their own, more complex calculations
 */
void generateEconomicActivity(
    CivilizationData &civData,
    const std::vector<std::shared_ptr<Arda::ArdaProvince>> &provinces,
    const std::vector<std::shared_ptr<ArdaRegion>> &regions,
    const std::vector<std::shared_ptr<ArdaContinent>> &continents,
    const double targetWorldGdp) {
  double worldEconomicActivitySum = 0.0;
  for (auto &province : provinces) {
    worldEconomicActivitySum +=
        province->averageDevelopment * province->populationDensity;
  }
  civData.worldEconomicActivitySum = worldEconomicActivitySum;
  // now calculate each provinces share of the world economic activity
  for (auto &province : provinces) {
    province->gdp = province->averageDevelopment * province->populationDensity;
    if (worldEconomicActivitySum > 0.0) {
      province->worldGdpShare = province->gdp / worldEconomicActivitySum;
    } else {
      province->worldGdpShare = 0.0;
    }
    province->gdp = std::lround(province->worldGdpShare * targetWorldGdp);
  }
  for (auto &gR : regions) {
    gR->worldEconomicActivityShare = 0.0;
    gR->gdp = 0.0;
    for (auto &province : gR->ardaProvinces) {
      gR->gdp += province->gdp;
      gR->worldEconomicActivityShare += province->worldGdpShare;
    }
  }
}

void generateReligions(
    CivilizationData &civData,
    std::vector<std::shared_ptr<Arda::ArdaProvince>> &ardaProvinces) {
  auto &config = Fwg::Cfg::Values();
  civData.religions.clear();
  Fwg::Gfx::Bitmap religionMap(config.width, config.height, 24);
  for (int i = 0; i < 8; i++) {
    Religion r;
    r.name = "";
    std::transform(r.name.begin(), r.name.end(), r.name.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    do {
      r.centerOfReligion = Fwg::Utils::selectRandom(ardaProvinces)->ID;
    } while (!ardaProvinces[r.centerOfReligion]->isLand());
    r.colour.randomize();
    civData.religions.push_back(std::make_shared<Religion>(r));
  }

  for (auto &ardaProvince : ardaProvinces) {
    if (!ardaProvince->isLand())
      continue;
    auto closestReligion = 0;
    auto distance = 100000000.0;
    for (auto x = 0; x < civData.religions.size(); x++) {
      auto &religion = civData.religions[x];
      auto religionCenter = ardaProvinces[religion->centerOfReligion];
      auto nDistance = Fwg::getPositionDistance(
          religionCenter->position, ardaProvince->position, config.width);
      if (Fwg::Utils::switchIfComparator(nDistance, distance, std::less())) {
        closestReligion = x;
      }
    }
    //// add only the main religion at this time
    // ardaProvince->religions[civData.religions[closestReligion]] = 1.0;
    // for (auto pix : ardaProvince->pixels) {
    //   religionMap.setColourAtIndex(pix,
    //                                civData.religions[closestReligion]->colour);
    // }
  }
  Fwg::Gfx::Png::save(religionMap, config.mapsPath + "world/religions.png");
}

void generateCultures(CivilizationData &civData,
                      std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions) {
  civData.cultures.clear();
  civData.cultureGroups.clear();
  auto &config = Fwg::Cfg::Values();
  int x = 20;
  int y = 5;
  int z = 15;

  // Generate x culture groups
  for (int i = 0; i < x; i++) {
    auto colour = Fwg::Gfx::Colour(0, 0, 0);
    colour.randomize();

    CultureGroup cultureGroup{"", colour};

    // randomly select a reguion to be the center of the culture group
    cultureGroup.setCenter(Fwg::Utils::selectRandom(ardaRegions));
    // add the region to the culture group
    cultureGroup.addRegion(cultureGroup.getCenter());
    auto cgPtr = std::make_shared<CultureGroup>(cultureGroup);
    civData.cultureGroups.push_back(cgPtr);
    // Generate y to z cultures per culture group
    int numCultures = RandNum::getRandom(y, z);
    for (int j = 0; j < numCultures; j++) {
      Culture culture;
      culture.name = "";
      std::transform(culture.name.begin(), culture.name.end(),
                     culture.name.begin(),
                     [](unsigned char c) { return std::tolower(c); });

      culture.colour.randomize();
      culture.language = std::make_shared<Arda::Language>();
      culture.cultureGroup = cgPtr;
      cgPtr->addCulture(std::make_shared<Culture>(culture));
    }
  }

  // now distribute these culturegroups to the regions
  for (auto &ardaRegion : ardaRegions) {
    if (ardaRegion->isSea() || ardaRegion->isLake())
      continue;
    auto closestCultureGroup = 0;
    auto distance = 100000000.0;
    for (auto x = 0; x < civData.cultureGroups.size(); x++) {
      auto cultureGroup = civData.cultureGroups[x];
      auto cultureCenter = cultureGroup->getCenter();
      auto nDistance = Fwg::getPositionDistance(
          cultureCenter->position, ardaRegion->position, config.width);
      if (Fwg::Utils::switchIfComparator(nDistance, distance, std::less())) {
        closestCultureGroup = x;
      }
    }
    civData.cultureGroups[closestCultureGroup]->addRegion(ardaRegion);
  }

  // randomly distribute culture centers inside the culturegroup
  for (auto &cultureGroup : civData.cultureGroups) {
    auto regs = cultureGroup->getRegions();

    for (auto &culture : cultureGroup->getCultures()) {
      culture->centerOfCulture = Fwg::Utils::selectRandom(regs)->ID;
    }
  }

  // now subdivide all culturegroups into cultures
  for (auto &cultureGroup : civData.cultureGroups) {
    for (auto &region : cultureGroup->getRegions()) {
      region->cultures.clear();
      auto closestCulture = 0;
      auto distance = 100000000.0;
      for (auto x = 0; x < cultureGroup->getCultures().size(); x++) {
        auto culture = cultureGroup->getCultures()[x];
        auto cultureCenter = ardaRegions[culture->centerOfCulture];
        auto nDistance = Fwg::getPositionDistance(
            cultureCenter->position, region->position, config.width);
        if (Fwg::Utils::switchIfComparator(nDistance, distance, std::less())) {
          closestCulture = x;
        }
      }
      region->cultures.insert({cultureGroup->getCultures()[closestCulture],
                               region->totalPopulation});
    }
  }
  // now calculate the visual type of the culture groups and set the cultures of
  // the culture group to it
  for (auto &cultureGroup : civData.cultureGroups) {
    cultureGroup->determineVisualType();
    for (auto &culture : cultureGroup->getCultures()) {
      culture->visualType = cultureGroup->getVisualType();
    }
  }
}

void generateLanguageGroup(std::shared_ptr<CultureGroup> &cultureGroup) {

  //// now generate at least as many languages as we have cultures
  // cultureGroup->getLanguageGroup()->generate(cultureGroup->getCultures().size(),
  //                                            Fwg::Cfg::Values().resourcePath
  //                                            +
  //                                                "//names//languageGroups//");
  //  languageGroup->generate(cultureGroup->getCultures().size());
}

void distributeLanguages(CivilizationData &civData) {
  LanguageGenerator languageGenerator(Fwg::Cfg::Values().resourcePath +
                                      "//names//languageGroups//");
  // assign a language group to each culture group
  for (auto &cultureGroup : civData.cultureGroups) {
    auto languageGroup = std::make_shared<Arda::LanguageGroup>(
        languageGenerator.generateLanguageGroup(
            cultureGroup->getCultures().size(), {}));
    civData.languageGroups.push_back(languageGroup);
    cultureGroup->setLanguageGroup(languageGroup);
    // generateLanguageGroup(cultureGroup);
    //  now assign each culture a language
  }
}

void generateImportance(std::vector<std::shared_ptr<ArdaRegion>> &regions) {
  double worldImportanceSum = 0.0;
  for (auto &region : regions) {
    region->importanceScore =
        region->worldEconomicActivityShare + region->worldPopulationShare;
    worldImportanceSum += region->importanceScore;
  }
  for (auto &region : regions) {
    region->relativeImportance = region->importanceScore / worldImportanceSum;
  }
}

void nameRegions(std::vector<std::shared_ptr<ArdaRegion>> &regions) {
  // take all regions and name them by taking their dominant cultures language
  // and generating a name
  for (auto &region : regions) {
    auto culture = region->getPrimaryCulture();
    if (culture == nullptr) {
      continue;
    }
    auto language = culture->language;
    region->name = language->generateAreaName("");
    for (auto &province : region->ardaProvinces) {
      province->name = language->generateAreaName("");
    }
  }
}
void nameSuperRegions(
    std::vector<std::shared_ptr<SuperRegion>> &superRegion,
    std::vector<std::shared_ptr<Arda::ArdaRegion>> &ardaRegions) {
  // take all regions and name them by taking their dominant cultures language
  // and generating a name
  for (auto &superRegion : superRegion) {
    // first find a reference region: check if any of our owned region has a
    // primary culture
    std::shared_ptr<Arda::ArdaRegion> namegivingRegion;
    for (auto &region : superRegion->ardaRegions) {
      auto culture = region->getPrimaryCulture();
      if (culture != nullptr) {
        namegivingRegion = region;
      }
    }
    if (namegivingRegion == nullptr) {
      float distance = 100000000.0;
      // search all regions, check the closest with a culture that is not null
      for (auto &region : ardaRegions) {
        auto culture = region->getPrimaryCulture();
        if (culture != nullptr) {
          auto nDistance =
              Fwg::getPositionDistance(region->position, superRegion->position,
                                       Fwg::Cfg::Values().width);
          if (nDistance < distance) {
            namegivingRegion = region;
            distance = nDistance;
          }
        }
      }
    }
    if (namegivingRegion == nullptr) {
      superRegion->name = "SuperRegion " + std::to_string(superRegion->ID);
      continue;
    } else {
      auto culture = namegivingRegion->getPrimaryCulture();
      if (culture == nullptr) {
        superRegion->name = "SuperRegion " + std::to_string(superRegion->ID);
        continue;
      }
      auto language = culture->language;
      superRegion->name =
          language->generateAreaName(superRegion->isSea() ? "sea" : "");
    }
  }
}

void nameContinents(std::vector<std::shared_ptr<ArdaContinent>> &continents,
                    std::vector<std::shared_ptr<ArdaRegion>> &regions) {
  // take all continents and name them by taking their dominant cultures
  // language and generating a name
  for (auto &continent : continents) {
    if (continent->regions.size()) {
      auto ID = continent->regions[0]->ID;
      auto culture = regions[ID]->getPrimaryCulture();
      if (culture == nullptr) {
        continue;
      }
      auto language = culture->language;
      continent->name = language->generateAreaName("");
      continent->adjective = language->getAdjectiveForm(continent->name);
    } else {
      continent->name = std::to_string(continent->ID);
      continent->adjective = std::to_string(continent->ID);
    }
  }
}
void applyCivilisationTopography(
    Arda::Civilization::CivilizationLayer &civLayer,
    const std::vector<std::shared_ptr<Arda::ArdaProvince>> &provinces) {
  civLayer.clear(Arda::Civilization::TopographyType::CITY);
  civLayer.clear(Arda::Civilization::TopographyType::FARMLAND);
  for (auto &province : provinces) {
    if (!province->isLand())
      continue;
    for (auto &location : province->locations) {
      if (location->type == Fwg::Civilization::LocationType::City ||
          location->type == Fwg::Civilization::LocationType::Port) {
        for (auto &pix : location->pixels) {
          civLayer.set(pix, Arda::Civilization::TopographyType::CITY);
        }
      } else if (location->type == Fwg::Civilization::LocationType::Farm) {
        for (auto &pix : location->pixels) {
          civLayer.set(pix, Arda::Civilization::TopographyType::FARMLAND);
        }
      }
    }
  }
}
namespace Wastelands {

std::vector<float>
detectWastelands(Fwg::Terrain::TerrainData &terrainData,
                 Fwg::Climate::ClimateData &climateData,
                 Arda::Civilization::CivilizationLayer &civLayer,
                 const Fwg::Cfg &config) {
  // sum up factors, such as altitude, climate zone habitability
  std::vector<float> wastelandMap(terrainData.landForms.size(), 0.0f);
  for (size_t i = 0; i < terrainData.landForms.size(); i++) {
    const auto &landForm = terrainData.landForms[i];
    const auto &climateType =
        climateData.climates[i].getChances(0).second; // get dominant climate
    const auto &climate = climateData.climateTypes.at((int)climateType);
    // add altitude factor
    // if (landForm.altitude > 0.2) {
    //  wastelandMap[i] += (landForm.altitude - 0.2) * 0.33f;
    //}
    // add climate factor
    // wastelandMap[i] += -climate.arability * 0.33f;
    // add inclination factor
    // wastelandMap[i] += landForm.inclination * 0.33f;
    wastelandMap[i] += (1.0f - climateData.habitabilities[i]);
  }
  // normalize the wasteland map
  // Fwg::Utils::normalizeVector(wastelandMap, 0.0f, 1.0f);

  // visualize wasteland map
  if (config.debugLevel > 1) {
    Fwg::Gfx::Bitmap wastelandBitmap(config.width, config.height, 24);
    for (size_t i = 0; i < wastelandMap.size(); i++) {
      wastelandBitmap.setColourAtIndex(
          i, Fwg::Gfx::Colour(static_cast<unsigned char>(0),
                              static_cast<unsigned char>(std::min<float>(
                                  255.0f, wastelandMap[i] * 255.0f)),
                              static_cast<unsigned char>(0)));
    }
    Fwg::Gfx::Png::save(wastelandBitmap, config.mapsPath + "wastelands.png",
                        false);
  }
  civLayer.wastelandChance = wastelandMap;
  return wastelandMap;
}

} // namespace Wastelands

bool sanityChecks(const CivilizationData &civData) { return true; }

} // namespace Arda::Civilization