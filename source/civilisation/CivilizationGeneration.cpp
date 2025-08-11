#include "civilisation/CivilizationGeneration.h"

namespace Arda::Civilization {

void generateWorldCivilizations(
    std::vector<std::shared_ptr<Arda::ArdaRegion>> &regions,
    std::vector<std::shared_ptr<Arda::ArdaProvince>> &ardaProvinces,
    CivilizationData &civData, std::vector<ArdaContinent> &continents,
    std::vector<std::shared_ptr<SuperRegion>> &superRegions) {
  generatePopulationFactors(civData, regions);
  generateDevelopment(regions);
  generateEconomicActivity(civData, regions);
  generateReligions(civData, ardaProvinces);
  generateCultures(civData, regions);
  Arda::Gfx::displayCultureGroups(civData);
  Arda::Gfx::displayCultures(regions);
  distributeLanguages(civData);
  for (auto &region : regions) {
    region->sumPopulations();
  }
  nameRegions(regions);
  generateImportance(regions);
  nameContinents(continents, regions);
  Arda::Areas::saveRegions(regions,
                           Fwg::Cfg::Values().mapsPath + "//areas//",
                           Arda::Gfx::visualiseRegions(regions));
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
    } while (!ardaProvinces[r.centerOfReligion]->baseProvince->isLand());
    r.colour.randomize();
    civData.religions.push_back(std::make_shared<Religion>(r));
  }

  for (auto &ardaProvince : ardaProvinces) {
    if (!ardaProvince->baseProvince->isLand())
      continue;
    auto closestReligion = 0;
    auto distance = 100000000.0;
    for (auto x = 0; x < civData.religions.size(); x++) {
      auto &religion = civData.religions[x];
      auto religionCenter = ardaProvinces[religion->centerOfReligion];
      auto nDistance = Fwg::getPositionDistance(
          religionCenter->baseProvince->position,
          ardaProvince->baseProvince->position, config.width);
      if (Fwg::Utils::switchIfComparator(nDistance, distance, std::less())) {
        closestReligion = x;
      }
    }
    //// add only the main religion at this time
    // ardaProvince->religions[civData.religions[closestReligion]] = 1.0;
    // for (auto pix : ardaProvince->baseProvince->pixels) {
    //   religionMap.setColourAtIndex(pix,
    //                                civData.religions[closestReligion]->colour);
    // }
  }
  Fwg::Gfx::Png::save(religionMap, config.mapsPath + "world/religions.png");
}

void generateCultures(CivilizationData &civData,
                      std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions) {
  civData.cultures.clear();
  auto &config = Fwg::Cfg::Values();
  int x = 20;
  int y = 5;
  int z = 15;

  // gather all regions which have no culture group assigned
  std::vector<std::shared_ptr<ArdaRegion>> unassignedRegions;

  // Generate x culture groups
  for (int i = 0; i < x; i++) {
    auto colour = Fwg::Gfx::Colour(0, 0, 0);
    colour.randomize();

    CultureGroup cultureGroup{"", colour};

    // randomly select a reguion to be the center of the culture group
    cultureGroup.setCenter(Fwg::Utils::selectRandom(ardaRegions));
    // add the region to the culture group
    cultureGroup.addRegion(cultureGroup.getCenter());

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
      culture.cultureGroup = std::make_shared<CultureGroup>(cultureGroup);
      cultureGroup.addCulture(std::make_shared<Culture>(culture));
    }

    civData.cultureGroups.push_back(
        std::make_shared<CultureGroup>(cultureGroup));
  }

  // now distribute these culturegroups to the regions
  for (auto &ardaRegion : ardaRegions) {
    if (ardaRegion->isSea() || ardaRegion->isLake())
      continue;
    auto closestCultureGroup = 0;
    auto distance = 100000000.0;
    for (auto x = 0; x < civData.cultureGroups.size(); x++) {
      auto &cultureGroup = civData.cultureGroups[x];
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
      region->cultureShares.insert(
          {cultureGroup->getCultures()[closestCulture], 1.0});
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

void distributeLanguages(CivilizationData &civData) {
  // assign a language group to each culture group
  for (auto &cultureGroup : civData.cultureGroups) {
    auto languageGroup = std::make_shared<Arda::LanguageGroup>();
    civData.languageGroups.push_back(languageGroup);
    cultureGroup->setLanguageGroup(languageGroup);
    // now generate at least as many languages as we have cultures
    languageGroup->generate(cultureGroup->getCultures().size(),
                            Fwg::Cfg::Values().resourcePath +
                                "//names//languageGroups//");
    // languageGroup->generate(cultureGroup->getCultures().size());
    //  now assign each culture a language
    for (auto i = 0; i < cultureGroup->getCultures().size(); i++) {
      cultureGroup->getCultures()[i]->language = languageGroup->languages[i];
      // generate a variety of names for the language
      languageGroup->languages[i]->fillAllLists();
    }
  }
}

void generatePopulationFactors(
    CivilizationData &civData,
    std::vector<std::shared_ptr<ArdaRegion>> &regions) {
  Fwg::Utils::Logging::logLine("Generating Population");
  double worldPopulationFactorSum = 0.0;
  for (auto &gR : regions) {
    gR->populationFactor = 0.0;
    for (auto &gProv : gR->ardaProvinces) {
      // calculate the population factor. We use both the size of the province
      // and the population density
      gProv->popFactor =
          gProv->baseProvince->populationDensity * gProv->baseProvince->size();
      // the game region has its population increased by the population factor
      // of the province, therefore a product of the size of all provinces and
      // their respective population densities
      gR->populationFactor += gProv->popFactor;
    }
    // to track the share of this region of the total
    worldPopulationFactorSum += gR->populationFactor;
  }
  // calculate the share of the world population for each region
  for (auto &gR : regions) {
    gR->worldPopulationShare = gR->populationFactor / worldPopulationFactorSum;
  }
  civData.worldPopulationFactorSum = worldPopulationFactorSum;
}
void generateDevelopment(std::vector<std::shared_ptr<ArdaRegion>> &regions) {
  Fwg::Utils::Logging::logLine("Generating State Development");
  auto &config = Fwg::Cfg::Values();
  Fwg::Gfx::Bitmap developmentFactor(config.width, config.height, 24);
  for (auto &region : regions) {
    auto totalPop = region->populationFactor;
    region->developmentFactor = 0.0;
    if (region->isSea() || region->isLake())
      continue;
    // we want to calculate the average development of the region, by taking the
    // average of the development of the provinces weighted by the popFactor
    // share of the totalPop
    for (auto &province : region->ardaProvinces) {
      auto popFactor = province->popFactor;
      region->developmentFactor +=
          province->baseProvince->developmentFactor * popFactor / totalPop;
    }
  }
}
/* Very simple calculation of economic activity. The modules can override this
 * to implement their own, more complex calculations
 */
void generateEconomicActivity(
    CivilizationData &civData,
    std::vector<std::shared_ptr<ArdaRegion>> &regions) {
  double worldEconomicActivitySum = 0.0;
  for (auto &region : regions) {
    region->economicActivity =
        region->developmentFactor * region->worldPopulationShare;
    worldEconomicActivitySum += region->economicActivity;
  }
  for (auto &region : regions) {
    region->worldEconomicActivityShare =
        region->economicActivity / worldEconomicActivitySum;
  }
  civData.worldEconomicActivitySum = worldEconomicActivitySum;
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

void nameContinents(std::vector<ArdaContinent> &continents,
                    std::vector<std::shared_ptr<ArdaRegion>> &regions) {
  // take all continents and name them by taking their dominant cultures
  // language and generating a name
  for (auto &continent : continents) {
    if (continent.regions.size()) {
      auto ID = continent.regions[0]->ID;
      auto culture = regions[ID]->getPrimaryCulture();
      if (culture == nullptr) {
        continue;
      }
      auto language = culture->language;
      continent.name = language->generateAreaName("");
      continent.adjective = language->getAdjectiveForm(continent.name);
    } else {
      continent.name = std::to_string(continent.ID);
      continent.adjective = std::to_string(continent.ID);
    }
  }
}

bool sanityChecks(const CivilizationData &civData) { return true; }

} // namespace Arda::Civilization