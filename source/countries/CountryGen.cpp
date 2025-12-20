#include "countries/CountryGen.h"

namespace Arda::Countries {
std::shared_ptr<ArdaRegion> &
findStartRegion(std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions) {
  std::vector<std::shared_ptr<ArdaRegion>> freeRegions;
  for (const auto &ardaRegion : ardaRegions)
    if (!ardaRegion->assigned && !ardaRegion->isSea() &&
        !ardaRegion->isLake() &&
        !ardaRegion->topographyTypes.count(
            Arda::Civilization::TopographyType::WASTELAND))
      freeRegions.push_back(ardaRegion);

  if (freeRegions.size() == 0)
    return ardaRegions[0];

  const auto &startRegion = Fwg::Utils::selectRandom(freeRegions);
  return ardaRegions[startRegion->ID];
}

void generateCountries(
    const Arda::Utils::GenerationAge &generationAge,
    std::function<std::shared_ptr<Country>()> factory, int numCountries,
    std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions,
    std::map<std::string, std::shared_ptr<Country>> &countries,
    std::vector<std::shared_ptr<Arda::ArdaProvince>> &ardaProvinces,
    Civilization::CivilizationData &civData, Arda::Names::NameData &nData) {
  countries.clear();
  for (auto &region : ardaRegions) {
    region->assigned = false;
    region->owner = nullptr;
  }
  Fwg::Utils::Logging::logLine("Generating Countries");

  for (auto i = 0; i < numCountries; i++) {
    auto country = factory();
    country->ID = i;
    country->tag = std::to_string(i);
    country->name = "DUMMY";
    country->adjective = "";
    country->flag = Gfx::Flag(82, 52);

    countries.emplace(country->tag, country);
  }
  distributeCountries(generationAge, ardaRegions, countries, ardaProvinces,
                      civData, nData);
  // remove all countries without regions
  std::erase_if(countries, [](const auto &entry) {
    return entry.second->ownedRegions.empty();
  });
}

void distributeCountries(
    const Arda::Utils::GenerationAge &generationAge,
    std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions,
    std::map<std::string, std::shared_ptr<Country>> &countries,
    std::vector<std::shared_ptr<Arda::ArdaProvince>> &ardaProvinces,
    Civilization::CivilizationData &civData, Arda::Names::NameData &nData) {

  auto &config = Fwg::Cfg::Values();

  Fwg::Utils::Logging::logLine("Distributing Countries");
  for (auto &countryEntry : countries) {
    auto &country = countryEntry.second;
    country->ownedRegions.clear();
    auto startRegion(findStartRegion(ardaRegions));
    if (startRegion->assigned || startRegion->isSea() || startRegion->isLake())
      continue;
    country->assignRegions(6, ardaRegions, startRegion, ardaProvinces);
    if (!country->ownedRegions.size())
      continue;
    // get the dominant culture in the country by iterating over all regions
    // and counting the number of provinces with the same culture
    country->gatherCultureShares();
    auto culture = country->getPrimaryCulture();
    if (culture == nullptr) {
      Fwg::Utils::Logging::logLine("No culture found for country " +
                                   country->tag +
                                   ", cannot give meaningful name");
    } else {
      auto language = culture->language;
      country->name = language->generateGenericCapitalizedWord();
      country->adjective = language->getAdjectiveForm(country->name);
      country->tag =
          Arda::Names::generateTag(country->name, nData.disallowedTokens);
    }
    for (auto &region : country->ownedRegions) {
      region->owner = country;
    }
  }
  Fwg::Utils::Logging::logLine("Distributing Assigning Regions");

  if (countries.size()) {
    for (auto &ardaRegion : ardaRegions) {
      if (!ardaRegion->isSea() && !ardaRegion->assigned &&
          !ardaRegion->isLake() &&
          !ardaRegion->topographyTypes.count(
              Arda::Civilization::TopographyType::WASTELAND)) {
        auto gR = Fwg::Utils::getNearestAssignedLand(
            ardaRegions, ardaRegion, config.width, config.height);
        gR->owner->addRegion(ardaRegion);
        ardaRegion->owner = gR->owner;
      }
    }
  }
  Fwg::Utils::Logging::logLine("Distributing Evaluating Populations");
  for (auto &country : countries) {
    country.second->evaluatePopulations(civData.worldPopulationFactorSum);
    country.second->gatherCultureShares();
  }
  generateCountrySpecifics(generationAge, countries);
}

void evaluateCountryNeighbours(
    std::vector<std::shared_ptr<Fwg::Areas::Region>> &baseRegions,
    std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions,
    std::map<std::string, std::shared_ptr<Country>> &countries) {
  Fwg::Utils::Logging::logLine("Evaluating Country Neighbours");
  Fwg::Areas::Regions::evaluateRegionNeighbours(baseRegions);

  for (auto &c : countries) {
    for (const auto &gR : c.second->ownedRegions) {
      if (gR->neighbours.size() != baseRegions[gR->ID]->neighbours.size())
        throw(std::exception("Fatal: Neighbour count mismatch, terminating"));
      // now compare if all IDs in those neighbour vectors match
      for (int i = 0; i < gR->neighbours.size(); i++) {
        if (gR->neighbours[i] != baseRegions[gR->ID]->neighbours[i])
          throw(std::exception("Fatal: Neighbour mismatch, terminating"));
      }

      for (const auto &neighbourRegion : gR->neighbours) {
        // TO DO: Investigate rare crash issue with index being out of range
        if (ardaRegions[neighbourRegion]->owner == nullptr)
          continue;
        if (neighbourRegion < ardaRegions.size() &&
            ardaRegions[neighbourRegion]->owner->tag != c.second->tag) {
          c.second->neighbours.insert(ardaRegions[neighbourRegion]->owner);
        }
      }
    }
  }
}

void loadCountries(const Arda::Utils::GenerationAge &generationAge,
                   std::function<std::shared_ptr<Country>()> factory,
                   std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions,
                   std::map<std::string, std::shared_ptr<Country>> &countries,
                   Civilization::CivilizationData &civData,
                   Arda::Names::NameData &nData,
                   const Fwg::Gfx::Image &inputImage,
                   const std::string &mappingPath) {
  int counter = 0;
  countries.clear();
  std::vector<std::string> mappingFileLines;
  Fwg::Utils::ColourTMap<std::vector<std::string>> mapOfCountries;
  try {
    mappingFileLines = Fwg::Parsing::getLines(mappingPath);
    for (auto &line : mappingFileLines) {
      auto tokens = Fwg::Parsing::getTokens(line, ';');
      auto colour = Fwg::Gfx::Colour(std::stoi(tokens[0]), std::stoi(tokens[1]),
                                     std::stoi(tokens[2]));
      mapOfCountries.setValue(colour, tokens);
    }
  } catch (std::exception e) {
    Fwg::Utils::Logging::logLine(
        "Exception while parsing country input, ", e.what(),
        " continuing with randomly generated countries");
  }
  Fwg::Utils::ColourTMap<std::vector<std::shared_ptr<Arda::ArdaRegion>>>
      mapOfRegions;
  for (auto &region : ardaRegions) {
    if (region->isSea() || region->isLake())
      continue;

    Fwg::Utils::ColourTMap<int> likeliestOwner;
    Fwg::Gfx::Colour selectedCol;

    for (auto province : region->ardaProvinces) {
      if (!province->isSea()) {
        //  we have the colour already
        auto colour = inputImage[province->pixels[0]];

        if (likeliestOwner.find(colour)) {
          likeliestOwner[colour] += province->pixels.size();

        } else {
          likeliestOwner.setValue(colour, province->pixels.size());
        }
        int max = 0;

        for (auto &potOwner : likeliestOwner.getMap()) {
          if (potOwner.second > max) {
            max = potOwner.second;
            selectedCol = potOwner.first;
          }
        }
      }
    }
    if (mapOfRegions.find(selectedCol)) {
      mapOfRegions[selectedCol].push_back(region);
    } else {
      mapOfRegions.setValue(selectedCol, {region});
    }
  }
  for (auto &entry : mapOfRegions.getMap()) {
    auto entryCol = entry.first;
    if (mapOfCountries.find(entryCol)) {
      auto tokens = mapOfCountries[entryCol];
      auto colour = Fwg::Gfx::Colour(std::stoi(tokens[0]), std::stoi(tokens[1]),
                                     std::stoi(tokens[2]));

      auto country = factory();
      country->ID = counter++;
      country->tag = tokens[3];
      country->name = tokens[4];
      country->adjective = tokens[5];
      country->flag = Arda::Gfx::Flag(82, 52);
      country->colour = colour;
      for (auto &region : entry.second) {
        country->addRegion(region);
      }
      countries.insert({country->tag, country});
    } else {
      auto country = factory();
      country->ID = counter++;
      country->tag = std::to_string(counter);
      country->name = "";
      country->adjective = "";
      country->flag = Arda::Gfx::Flag(82, 52);
      country->colour = entryCol;
      for (auto &region : entry.second) {
        country->addRegion(region);
      }
      countries.insert({country->tag, country});
    }
  }
  for (auto &country : countries) {
    country.second->gatherCultureShares();
    auto culture = country.second->getPrimaryCulture();
    auto language = culture->language;
    // only generate name and tag if this country was not in the input
    // mappings
    if (!country.second->name.size()) {
      country.second->name = language->generateGenericCapitalizedWord();
      country.second->tag = Arda::Names::generateTag(country.second->name,
                                                     nData.disallowedTokens);
    }
    country.second->adjective =
        language->getAdjectiveForm(country.second->name);
    for (auto &region : country.second->ownedRegions) {
      region->owner = country.second;
    }
    country.second->evaluatePopulations(civData.worldPopulationFactorSum);
    generateCountrySpecifics(generationAge, countries);
  }
}

void saveCountries(std::map<std::string, std::shared_ptr<Country>> &countries,
                   const std::string &mappingPath,
                   const Fwg::Gfx::Image &countryImage) {
  std::string fileContent = "#r;g;b;tag;name;adjective\n";
  for (const auto &country : countries) {
    fileContent += country.second->exportLine();
    fileContent += "\n";
  }
  Fwg::Parsing::writeFile(mappingPath + "//countryMappings.txt", fileContent);
  Fwg::Gfx::Png::save(countryImage, mappingPath + "//countries.png");
}

void generateCountrySpecifics(
    const Arda::Utils::GenerationAge &generationAge,
    std::map<std::string, std::shared_ptr<Country>> &countries) {
  // military: navalFocus, airFocus, landFocus
  for (auto &countryEntry : countries) {
    auto &country = countryEntry.second;
    // military focus: first gather info about position of the country, taking
    // coastline into account
    auto coastalRegions = 0.0;
    for (auto &region : country->ownedRegions) {
      if (region->coastal) {
        coastalRegions++;
      }
    }
    // naval focus goes from 0-50%. If we have a lot of coastal regions, we
    // focus on naval
    country->navalFocus = std::clamp(
        (coastalRegions / country->ownedRegions.size() * 100.0), 0.0, 50.0);
    // only allow air focus in world war age
    if (generationAge == Arda::Utils::GenerationAge::WorldWar) {
      // TODO: Increase if our position is very remote?
      // now let's get the air focus, which primarily depends on randomness,
      // should be between 5 and 35%
      country->airFocus = RandNum::getRandom(5.0, 35.0);
    }
    // land focus is the rest
    country->landFocus = 100.0 - country->navalFocus - country->airFocus;
  }
}
} // namespace Arda::Countries