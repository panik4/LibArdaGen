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
  generateCultures(civData, ardaProvinces);
  Arda::Gfx::displayCultureGroups(ardaProvinces);
  Arda::Gfx::displayCultures(ardaProvinces);
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
    if (prov->isLand() && !prov->topographyTypes.count(
                              Arda::Civilization::TopographyType::WASTELAND)) {
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
    for (auto &province : continent->ardaProvinces) {
      province->averageDevelopment = 0.001f;
      // skip lake and sea provinces, also wastelands
      if (!province->isLand() ||
          province->topographyTypes.count(
              Arda::Civilization::TopographyType::WASTELAND))
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
  for (auto &gR : regions) {
    gR->worldPopulationShare = 0.0;
    gR->totalPopulation = 0;
    if (!gR->topographyTypes.count(
            Arda::Civilization::TopographyType::WASTELAND)) {
      for (auto &province : gR->ardaProvinces) {
        if (province->isLand()) {
          auto sum = province->populationDensity *
                     province->getNonOwningPixelView().size();
          province->worldPopulationShare = sum / worldPopulationFactorSum;
        } else {
          province->worldPopulationShare = 0.0;
        }
        gR->worldPopulationShare += province->worldPopulationShare;
        province->population =
            std::lround(province->worldPopulationShare *
                        static_cast<double>(targetWorldPopulation));
        gR->totalPopulation += province->population;
      }
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
    if (prov->isSea() || prov->topographyTypes.count(
                             Arda::Civilization::TopographyType::WASTELAND))
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
    province->populationDensity = 0.001f;
    // skip lake and sea provinces
    if (province->isLand() &&
        !province->topographyTypes.count(
            Arda::Civilization::TopographyType::WASTELAND)) {
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
  if (ardaProvinces.empty()) {
    Fwg::Utils::Logging::logLine(
        "SEVERE ERROR: No provinces available for religion generation.");
    return;
  }
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

void generateCultures(
    CivilizationData &civData,
    std::vector<std::shared_ptr<ArdaProvince>> &ardaProvinces) {
  civData.cultures.clear();
  civData.cultureGroups.clear();

  auto &config = Fwg::Cfg::Values();

  int numGroups = 20;    // number of culture groups
  int numCultures = 200; // total number of cultures to spawn

  // -------------------------------------------------------------------------
  // 1. Generate culture groups (X random starting points)
  // -------------------------------------------------------------------------
  for (int i = 0; i < numGroups; i++) {

    auto colour = Fwg::Gfx::Colour(0, 0, 0);
    colour.randomize();

    CultureGroup group{"", colour};

    // random center province
    auto center = Fwg::Utils::selectRandom(ardaProvinces);
    group.setCenter(center);

    auto groupPtr = std::make_shared<CultureGroup>(group);
    civData.cultureGroups.push_back(groupPtr);
  }

  // -------------------------------------------------------------------------
  // 2. Generate cultures (Y random starting points)
  //    And assign each culture to its closest culture group
  // -------------------------------------------------------------------------
  for (int i = 0; i < numCultures; i++) {

    auto centerProv = Fwg::Utils::selectRandom(ardaProvinces);

    auto cult = std::make_shared<Culture>();
    cult->colour.randomize();
    cult->language = std::make_shared<Arda::Language>();
    cult->centerOfCulture = centerProv;

    // find closest group center
    float bestDist = 100000.0f;
    std::shared_ptr<CultureGroup> bestGroup = nullptr;

    for (auto &group : civData.cultureGroups) {

      auto groupCenter = group->getCenter();
      float d = Fwg::getPositionDistance(groupCenter->position,
                                         centerProv->position, config.width);

      if (d < bestDist) {
        bestDist = d;
        bestGroup = group;
      }
    }

    // assign the culture to the closest group
    cult->cultureGroup = bestGroup;
    bestGroup->addCulture(cult);

    civData.cultures.push_back(cult);
  }

  // -------------------------------------------------------------------------
  // 3. Assign each province to the nearest culture
  // -------------------------------------------------------------------------
  for (auto &prov : ardaProvinces) {

    prov->cultures.clear();

    if (prov->isSea() || prov->isLake())
      continue;

    float bestDist = 100000.0f;
    std::shared_ptr<Culture> bestCulture = nullptr;

    for (auto &cult : civData.cultures) {

      const auto cultureCenter = cult->centerOfCulture;

      float d = Fwg::getPositionDistance(cultureCenter->position,
                                         prov->position, config.width);

      if (d < bestDist) {
        bestDist = d;
        bestCulture = cult;
      }
    }

    if (bestCulture) {
      prov->cultures.insert({bestCulture, 1.0f});
    }
  }

  // -------------------------------------------------------------------------
  // 4. Determine visual type for each group, apply to its cultures
  // -------------------------------------------------------------------------
  for (auto &group : civData.cultureGroups) {
    group->determineVisualType();

    for (auto &cult : group->getCultures()) {
      cult->visualType = group->getVisualType();
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

  // Clear all relevant layers
  for (const auto &[_, topoType] : typeMap) {
    civLayer.clear(topoType);
  }

  // Apply topography by location type
  for (const auto &province : provinces) {
    // if (!province->isLand())
    //   continue;

    for (const auto &location : province->locations) {
      auto it = typeMap.find(location->type);
      if (it == typeMap.end())
        continue;

      for (int pix : location->pixels)
        civLayer.set(pix, it->second);
    }
  }
}

bool sanityChecks(const CivilizationData &civData) { return true; }

} // namespace Arda::Civilization