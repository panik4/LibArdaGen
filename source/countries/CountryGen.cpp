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

  const auto &startRegion = Fwg::Utils::Random::selectRandom(freeRegions);
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
  // return state of disallowed tokens to original, as we might have modified it
  nData.disallowedTokens = nData.originalDisallowedTokens;
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
        auto gR = Fwg::Utils::Proc::getNearestAssignedLand(
            ardaRegions, ardaRegion, config.width, config.height);
        gR->owner->addRegion(ardaRegion);
        ardaRegion->owner = gR->owner;
      }
    }
  }
  Fwg::Utils::Logging::logLine("Distributing Evaluating Populations");
  for (auto &country : countries) {
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
    c.second->neighbourCountries.clear();
    c.second->neighbours.clear();

    for (const auto &gR : c.second->ownedRegions) {
      if (gR->neighbours.size() != baseRegions[gR->ID]->neighbours.size())
        throw(
            std::runtime_error("Fatal: Neighbour count mismatch, terminating"));
      // now compare if all IDs in those neighbour vectors match
      for (int i = 0; i < gR->neighbours.size(); i++) {
        if (gR->neighbours[i] != baseRegions[gR->ID]->neighbours[i])
          throw(std::runtime_error("Fatal: Neighbour mismatch, terminating"));
      }

      for (const auto &neighbourRegion : gR->neighbours) {
        // TO DO: Investigate rare crash issue with index being out of range
        auto neighbourCountry = ardaRegions[neighbourRegion->ID]->owner;
        if (neighbourCountry == nullptr)
          continue;
        if (neighbourRegion->ID < ardaRegions.size() &&
            neighbourCountry->tag != c.second->tag) {
          c.second->neighbourCountries.insert(neighbourCountry);
          c.second->neighbours.push_back(neighbourCountry);
        }
      }
    }
  }
}

void loadCountriesFromText(
    const Arda::Utils::GenerationAge &generationAge,
    std::function<std::shared_ptr<Country>()> factory,
    std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions,
    std::map<std::string, std::shared_ptr<Country>> &countries,
    Civilization::CivilizationData &civData, Arda::Names::NameData &nData,
    const std::string &countryMappings) {

  int counter = 0;
  // countries.clear();
  std::vector<std::string> mappingFileLines;
  Fwg::Utils::ColourTMap<std::vector<std::string>> inputCountryMap;
  try {
    mappingFileLines = Fwg::Parsing::splitLines(countryMappings);
    for (auto &line : mappingFileLines) {
      auto tokens = Fwg::Parsing::getTokens(line, ';');
      auto colour = Fwg::Gfx::Colour(std::stoi(tokens[0]), std::stoi(tokens[1]),
                                     std::stoi(tokens[2]));
      inputCountryMap.setValue(colour, tokens);
    }
  } catch (std::exception &e) {
    Fwg::Utils::Logging::logLine(
        "Exception while parsing country input, ", e.what(),
        " continuing with randomly generated countries");
  }

  Fwg::Utils::ColourTMap<std::shared_ptr<Country>> existingCountryMap;
  for (auto &country : countries) {
    existingCountryMap.setValue(country.second->colour, country.second);
  }

  // now we check if we already have countries that exist with this colour. This
  // way we can just rename the existing ones and apply the other inputs from
  // the textfile, without further modifying anything else
  for (auto &countryMapEntry : inputCountryMap.getMap()) {

    auto tokens = countryMapEntry.second;
    auto colour = Fwg::Gfx::Colour(std::stoi(tokens[0]), std::stoi(tokens[1]),
                                   std::stoi(tokens[2]));

    if (existingCountryMap.contains(colour)) {
      auto &existingCountry = existingCountryMap.at(colour);
      existingCountry->tag = inputCountryMap.at(colour)[3];
      existingCountry->name = inputCountryMap.at(colour)[4];
      existingCountry->adjective = inputCountryMap.at(colour)[5];
    } else {
      // we always expect to find every single country that we currently have
      // in the mapentry. We log that we can't find it, and create an empty
      // country

      auto country = factory();
      country->ID = counter++;
      country->tag = tokens[3];
      country->name = tokens[4];
      country->adjective = tokens[5];
      country->flag = Arda::Gfx::Flag(82, 52);
      country->colour = colour;
      countries.insert({country->tag, country});
    }
  }
}

void loadCountries(const Arda::Utils::GenerationAge &generationAge,
                   std::function<std::shared_ptr<Country>()> factory,
                   std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions,
                   std::map<std::string, std::shared_ptr<Country>> &countries,
                   Civilization::CivilizationData &civData,
                   Arda::Names::NameData &nData,
                   const Fwg::Gfx::Image &inputImage) {
  int counter = 0;
  countries.clear();
  // return state of disallowed tokens to original, as we might have modified it
  nData.disallowedTokens = nData.originalDisallowedTokens;
  Fwg::Utils::ColourTMap<std::vector<std::shared_ptr<Arda::ArdaRegion>>>
      mapOfRegions;
  for (auto &region : ardaRegions) {
    if (region->isSea() || region->isLake())
      continue;

    Fwg::Utils::ColourTMap<int> likeliestOwner;
    Fwg::Gfx::Colour selectedCol;

    // Count ownership using all pixels belonging to the region
    for (const auto pixelIndex : region->getNonOwningPixelView()) {
      auto colour = inputImage[pixelIndex];

      if (likeliestOwner.contains(colour)) {
        likeliestOwner[colour]++;
      } else {
        likeliestOwner.setValue(colour, 1);
      }
    }

    // Determine dominant colour once, after accumulation
    int max = -1;
    for (const auto &potOwner : likeliestOwner.getMap()) {
      if (potOwner.second > max) {
        max = potOwner.second;
        selectedCol = potOwner.first;
      }
    }

    // Assign region to dominant colour
    if (mapOfRegions.contains(selectedCol)) {
      mapOfRegions[selectedCol].push_back(region);
    } else {
      mapOfRegions.setValue(selectedCol, {region});
    }
  }
  for (auto &entry : mapOfRegions.getMap()) {
    auto entryCol = entry.first;

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
    generateCountrySpecifics(generationAge, countries);
  }
}

void deriveCountries(
    Simulation::SimulationExport &simulationExport,
    const Arda::Utils::GenerationAge &generationAge,
    std::function<std::shared_ptr<Country>()> factory, int numCountries,
    std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions,
    std::map<std::string, std::shared_ptr<Country>> &countries,
    std::vector<std::shared_ptr<Arda::ArdaProvince>> &ardaProvinces,
    Civilization::CivilizationData &civData, Arda::Names::NameData &nData) {
  (void)numCountries;
  (void)civData;
  countries.clear();
  nData.disallowedTokens = nData.originalDisallowedTokens;
  for (const auto &region : ardaRegions) {
    if (!region)
      continue;
    region->assigned = false;
    region->owner = nullptr;
  }

  std::map<int, std::shared_ptr<Arda::ArdaProvince>> provincesById;
  for (const auto &province : ardaProvinces)
    if (province)
      provincesById.emplace(province->ID, province);

  std::map<int, std::shared_ptr<ArdaRegion>> regionsById;
  for (const auto &region : ardaRegions)
    if (region)
      regionsById.emplace(region->ID, region);

  std::map<int, std::shared_ptr<Country>> countriesByPolity;
  for (const auto &[polityId, polityExport] : simulationExport.polities) {
    auto country = factory();
    country->ID = polityId;
    country->tag = "P" + std::to_string(polityId);
    country->name = "Polity " + std::to_string(polityId);
    country->adjective = country->name;
    country->flag = Gfx::Flag(82, 52);
    country->colour = polityExport.polity.colour;
    countriesByPolity.emplace(polityId, country);
    countries.emplace(country->tag, country);
  }

  for (const auto &[provinceId, provinceExport] : simulationExport.provinces) {
    const auto province = provincesById.find(provinceId);
    if (province == provincesById.end())
      continue;
    province->second->owner = "P" + std::to_string(provinceExport.polityId);
    const auto country = countriesByPolity.find(provinceExport.polityId);
    if (country != countriesByPolity.end())
      country->second->ownedProvinces.push_back(province->second);
  }

  for (const auto &[polityId, polityExport] : simulationExport.polities) {
    const auto country = countriesByPolity.find(polityId);
    if (country == countriesByPolity.end())
      continue;
    for (const auto regionId : polityExport.regionIds) {
      const auto region = regionsById.find(regionId);
      if (region == regionsById.end())
        continue;
      country->second->ownedRegions.push_back(region->second);
      region->second->owner = country->second;
      region->second->assigned = true;
    }
  }

  for (auto &[polityId, country] : countriesByPolity) {
    std::sort(country->ownedProvinces.begin(), country->ownedProvinces.end(),
              [](const auto &left, const auto &right) {
                return left->ID < right->ID;
              });
    std::sort(country->ownedRegions.begin(), country->ownedRegions.end(),
              [](const auto &left, const auto &right) {
                return left->ID < right->ID;
              });
    country->gatherCultureShares();
    std::shared_ptr<ArdaRegion> historicalNameRegion;
    const auto peak = simulationExport.historicalPolityPeaks.find(polityId);
    for (const auto regionId :
         peak == simulationExport.historicalPolityPeaks.end()
             ? std::vector<Simulation::RegionId>{}
             : peak->second.regionIds) {
      const auto region = regionsById.find(regionId);
      if (region != regionsById.end() && !region->second->name.empty() &&
          (!historicalNameRegion || region->second->totalPopulation >
                                        historicalNameRegion->totalPopulation))
        historicalNameRegion = region->second;
    }
    if (historicalNameRegion)
      country->name = historicalNameRegion->name;
    else if (const auto culture = country->getPrimaryCulture())
      country->name = culture->language->generateGenericCapitalizedWord();
    if (const auto culture = country->getPrimaryCulture())
      country->adjective = culture->language->getAdjectiveForm(country->name);
    country->tag =
        Arda::Names::generateTag(country->name, nData.disallowedTokens);
    countries.erase("P" + std::to_string(polityId));
    countries[country->tag] = country;

    for (const auto &province : country->ownedProvinces)
      province->owner = country->tag;
  }

  generateCountrySpecifics(generationAge, countries);
}

void saveCountries(std::map<std::string, std::shared_ptr<Country>> &countries,
                   const std::string &mappingPath) {
  std::string fileContent = "#r;g;b;tag;name;adjective\n";
  for (const auto &country : countries) {
    fileContent += country.second->exportLine();
    fileContent += "\n";
  }
  Fwg::Parsing::writeFile(mappingPath + "/countryMappings.txt", fileContent);
}

void generateCountrySpecifics(
    const Arda::Utils::GenerationAge &generationAge,
    std::map<std::string, std::shared_ptr<Country>> &countries) {
  // military: navalFocus, airFocus, landFocus
  for (int countryID = 0; auto &countryEntry : countries) {
    auto &country = countryEntry.second;
    country->ID = countryID++;

    // sort owned regions by ID
    std::sort(country->ownedRegions.begin(), country->ownedRegions.end(),
              [](const auto &a, const auto &b) { return a->ID < b->ID; });

    // military focus: first gather info about position of the country, taking
    // coastline into account
    auto coastalRegions = 0.0;
    for (auto &region : country->ownedRegions) {
      if (region->isCoastalToOcean()) {
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