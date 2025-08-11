#pragma once
#include "FastWorldGenerator.h"
#include "areas/ArdaRegion.h"
#include "areas/ArdaContinent.h"
#include "civilisation/ArdaCivilisations.h"
#include "rendering/ArdaVisualisation.h"
#include "culture/CultureGroup.h"
#include "culture/Religion.h"
#include "areas/AreaGen.h"

namespace Arda::Civilization {
// generic preparations. However, if desired, there are necessary preparations
// for every game such as reading in the existing worldmap, states, regions,
// provinces etc
void generateWorldCivilizations(
    std::vector<std::shared_ptr<ArdaRegion>> &regions,
    std::vector<std::shared_ptr<Arda::ArdaProvince>> &ardaProvinces,
    CivilizationData &civData, std::vector<ArdaContinent> &continents,
    std::vector<std::shared_ptr<SuperRegion>> &superRegions);
void generateReligions(
    CivilizationData &civData,
    std::vector<std::shared_ptr<Arda::ArdaProvince>> &ardaProvinces);
void generateCultures(CivilizationData &civData,
                      std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions);
void distributeLanguages(CivilizationData &civData);
// calculating amount of population in states
void generatePopulationFactors(CivilizationData &civData,
                               std::vector<std::shared_ptr<ArdaRegion>> &regions);
// determine development from habitability, population density and randomness
void generateDevelopment(std::vector<std::shared_ptr<ArdaRegion>> &regions);
// determine development from habitability, population density and randomness
void generateEconomicActivity(CivilizationData &civData,
                              std::vector<std::shared_ptr<ArdaRegion>> &regions);
// determine importance from population, development and economicActivity
void generateImportance(std::vector<std::shared_ptr<ArdaRegion>> &regions);
// after having generated cultures, generate names for the regions
void nameRegions(std::vector<std::shared_ptr<ArdaRegion>> &regions);
void nameSuperRegions(
    std::vector<std::shared_ptr<SuperRegion>> &superRegion,
    std::vector<std::shared_ptr<Arda::ArdaRegion>> &ardaRegions);
    // after having generated cultures, generate names for the continents
void nameContinents(std::vector<ArdaContinent> &continents,
                    std::vector<std::shared_ptr<ArdaRegion>> &regions);

bool sanityChecks(const CivilizationData &civData);

} // namespace Arda::Civilization
