#pragma once
#include "FastWorldGenerator.h"
#include "areas/ArdaContinent.h"
#include "areas/ArdaRegion.h"
#include "areas/AreaGen.h"
#include "civilisation/ArdaCivilisations.h"
#include "civilisation/CivilisationLayer.h"
#include "culture/CultureGroup.h"
#include "culture/Religion.h"
#include "language/LanguageGenerator.h"
#include "rendering/ArdaVisualisation.h"
namespace Arda::Civilization {
// generic preparations. However, if desired, there are necessary preparations
// for every game such as reading in the existing worldmap, states, regions,
// provinces etc

void generateEconomyData(
    CivilizationData &civData,
    std::vector<std::shared_ptr<Arda::ArdaProvince>> &ardaProvinces,
    std::vector<std::shared_ptr<Arda::ArdaRegion>> &regions,
    std::vector<std::shared_ptr<ArdaContinent>> &continents,
    const double targetWorldGdp);
void generateCultureData(
    CivilizationData &civData,
    std::vector<std::shared_ptr<Arda::ArdaProvince>> &ardaProvinces,
    std::vector<std::shared_ptr<Arda::ArdaRegion>> &regions,
    std::vector<std::shared_ptr<ArdaContinent>> &continents,
    std::vector<std::shared_ptr<SuperRegion>> &superRegions);
void generateFullCivilisationData(
    std::vector<std::shared_ptr<ArdaRegion>> &regions,
    std::vector<std::shared_ptr<Arda::ArdaProvince>> &ardaProvinces,
    CivilizationData &civData,
    std::vector<std::shared_ptr<ArdaContinent>> &continents,
    std::vector<std::shared_ptr<SuperRegion>> &superRegions,
    const double targetWorldPopulation, const double targetGdp);
void generateReligions(
    CivilizationData &civData,
    std::vector<std::shared_ptr<Arda::ArdaProvince>> &ardaProvinces);
void generateCultures(
    CivilizationData &civData,
    std::vector<std::shared_ptr<ArdaProvince>> &ardaProvinces);
void generateLanguageGroup(std::shared_ptr<CultureGroup> &cultureGroup);
void distributeLanguages(CivilizationData &civData);

void loadDevelopment(
    const Fwg::Gfx::Image &developmentMap,
    const std::vector<std::shared_ptr<Arda::ArdaProvince>> &provinces,
    const std::vector<std::shared_ptr<ArdaRegion>> &regions,
    const std::vector<std::shared_ptr<ArdaContinent>> &continents);
// determine development from habitability, population density and randomness
void generateDevelopment(
    const std::vector<std::shared_ptr<Arda::ArdaProvince>> &provinces,
    const std::vector<std::shared_ptr<ArdaRegion>> &regions,
    const std::vector<std::shared_ptr<ArdaContinent>> &continents);
void loadPopulation(
    const Fwg::Gfx::Image &populationMap,
    const std::vector<std::shared_ptr<Arda::ArdaProvince>> &provinces,
    const std::vector<std::shared_ptr<ArdaRegion>> &regions,
    const std::vector<std::shared_ptr<ArdaContinent>> &continents,
    double targetWorldPopulation);

void generatePopulation(
    CivilizationData &civData,
    const std::vector<std::shared_ptr<Arda::ArdaProvince>> &provinces,
    const std::vector<std::shared_ptr<ArdaRegion>> &regions,
    const std::vector<std::shared_ptr<ArdaContinent>> &continents,
    double targetWorldPopulation);

void generateEconomicActivity(
    CivilizationData &civData,
    const std::vector<std::shared_ptr<Arda::ArdaProvince>> &provinces,
    const std::vector<std::shared_ptr<ArdaRegion>> &regions,
    const std::vector<std::shared_ptr<ArdaContinent>> &continents,
    const double targetWorldGdp);
// determine importance from population, development and gdp
void generateImportance(std::vector<std::shared_ptr<ArdaRegion>> &regions);
// after having generated cultures, generate names for the regions
void nameRegions(std::vector<std::shared_ptr<ArdaRegion>> &regions);
void nameSuperRegions(
    std::vector<std::shared_ptr<SuperRegion>> &superRegion,
    std::vector<std::shared_ptr<Arda::ArdaRegion>> &ardaRegions);
// after having generated cultures, generate names for the continents
void nameContinents(std::vector<std::shared_ptr<ArdaContinent>> &continents,
                    std::vector<std::shared_ptr<ArdaRegion>> &regions);
void applyCivilisationTopography(
    Arda::Civilization::CivilizationLayer &civLayer,
    const std::vector<std::shared_ptr<Arda::ArdaProvince>> &provinces);



bool sanityChecks(const CivilizationData &civData);

} // namespace Arda::Civilization
