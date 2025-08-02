#include "countries/CountryGen.h"

namespace Arda::Countries {
std::shared_ptr<ArdaRegion> &
findStartRegion(std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions) {
  std::vector<std::shared_ptr<ArdaRegion>> freeRegions;
  for (const auto &ardaRegion : ardaRegions)
    if (!ardaRegion->assigned && !ardaRegion->isSea() && !ardaRegion->isLake())
      freeRegions.push_back(ardaRegion);

  if (freeRegions.size() == 0)
    return ardaRegions[0];

  const auto &startRegion = Fwg::Utils::selectRandom(freeRegions);
  return ardaRegions[startRegion->ID];
}

void generateCountries(
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
  auto &config = Fwg::Cfg::Values();
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
  distributeCountries(ardaRegions, countries, ardaProvinces, civData, nData);
}

void distributeCountries(
    std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions,
    std::map<std::string, std::shared_ptr<Country>>& countries,
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
    auto language = culture->language;
    country->name = language->generateGenericCapitalizedWord();
    country->adjective = language->getAdjectiveForm(country->name);
    country->tag =
        Arda::Names::generateTag(country->name, nData.disallowedTokens);
    for (auto &region : country->ownedRegions) {
      region->owner = country;
    }
  }
  Fwg::Utils::Logging::logLine("Distributing Assigning Regions");

  if (countries.size()) {
    for (auto &ardaRegion : ardaRegions) {
      if (!ardaRegion->isSea() && !ardaRegion->assigned &&
          !ardaRegion->isLake()) {
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

}

void evaluateCountryNeighbours(
    std::vector<Fwg::Areas::Region> &baseRegions,
    std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions,
    std::map<std::string, std::shared_ptr<Country>> &countries) {
  Fwg::Utils::Logging::logLine("Evaluating Country Neighbours");
  Fwg::Areas::Regions::evaluateRegionNeighbours(baseRegions);

  for (auto &c : countries) {
    for (const auto &gR : c.second->ownedRegions) {
      if (gR->neighbours.size() != baseRegions[gR->ID].neighbours.size())
        throw(std::exception("Fatal: Neighbour count mismatch, terminating"));
      // now compare if all IDs in those neighbour vectors match
      for (int i = 0; i < gR->neighbours.size(); i++) {
        if (gR->neighbours[i] != baseRegions[gR->ID].neighbours[i])
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

void evaluateCountries() {}
void generateCountrySpecifics() {};
} // namespace Arda::Countries