#pragma once
#include "areas/SuperRegion.h"
#include "civilisation/ArdaCivilisations.h"
#include "civilisation/CivilisationLayer.h"
#include "countries/Country.h"
#include "rendering/Visualization.h"

namespace Arda::Gfx {
Fwg::Gfx::Image displayDevelopment(
    const std::vector<std::shared_ptr<Arda::ArdaProvince>> provinces);
Fwg::Gfx::Image displayPopulation(
    const std::vector<std::shared_ptr<Arda::ArdaProvince>> provinces);
Fwg::Gfx::Image
displayTopography(const Arda::Civilization::CivilizationLayer &civLayer,
                  Fwg::Gfx::Image worldMap);
Fwg::Gfx::Image displayCultureGroups(
    const std::vector<std::shared_ptr<ArdaProvince>> &ardaProvinces);
Fwg::Gfx::Image displayCultures(
    const std::vector<std::shared_ptr<ArdaProvince>> &ardaProvinces);
Fwg::Gfx::Image displayReligions(
    const std::vector<std::shared_ptr<ArdaProvince>> &ardaProvinces);
Fwg::Gfx::Image displayLanguageGroups(
    const std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions);
Fwg::Gfx::Image
displayLanguages(const std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions);

Fwg::Gfx::Image
displayLocations(std::vector<std::shared_ptr<Fwg::Areas::Region>> &regions,
                 Fwg::Gfx::Image worldMap);

Fwg::Gfx::Image displayConnections(
    const std::vector<std::shared_ptr<Fwg::Areas::Region>> &regions,
    Fwg::Gfx::Image connectionMap);

Fwg::Gfx::Image
displayWorldOverlayMap(const Fwg::Climate::ClimateData &climateData,
                       const Fwg::Gfx::Image &worldMap,
                       const Arda::Civilization::CivilizationLayer &civLayer);

Fwg::Gfx::Image visualiseStrategicRegions(
    Fwg::Gfx::Image &superRegionMap,
    const std::vector<std::shared_ptr<Arda::SuperRegion>> &superRegions,
    const int ID = -1);

Fwg::Gfx::Image
visualiseRegions(const std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions);

Fwg::Gfx::Image visualiseCountries(
    const std::map<std::string, std::shared_ptr<Country>> &countries);

} // namespace Arda::Gfx