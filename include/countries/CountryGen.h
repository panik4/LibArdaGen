#pragma once
#include "RandNum.h"
#include "areas/ArdaContinent.h"
#include "areas/ArdaProvince.h"
#include "areas/ArdaRegion.h"
#include "areas/SuperRegion.h"
#include "civilisation/CivilizationGeneration.h"
#include "countries/Country.h"
#include "culture/NameUtils.h"
#include "flags/Flag.h"
#include "utils/ArdaUtils.h"
#include "utils/Logging.h"
#include <map>
namespace Arda::Countries {

// ardaRegions are used for every single game,
std::shared_ptr<ArdaRegion> &
findStartRegion(std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions);

// and countries are always created the same way

void generateCountries(
    std::function<std::shared_ptr<Country>()> factory, int numCountries,
    std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions,
    std::map<std::string, std::shared_ptr<Country>> &countries,
    std::vector<std::shared_ptr<Arda::ArdaProvince>> &ardaProvinces,
    Civilization::CivilizationData &civData, Arda::Names::NameData &nData);

void distributeCountries(
    std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions,
    std::map<std::string, std::shared_ptr<Country>> &countries,
    std::vector<std::shared_ptr<Arda::ArdaProvince>> &ardaProvinces,
    Civilization::CivilizationData &civData, Arda::Names::NameData &nData);
// see which country neighbours which
void evaluateCountryNeighbours(
    std::vector<Fwg::Areas::Region> &baseRegions,
    std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions,
    std::map<std::string, std::shared_ptr<Country>> &countries);
// virtual void generateCountrySpecifics();

}; // namespace Arda::Countries
