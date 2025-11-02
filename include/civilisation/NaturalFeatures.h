
#include "FastWorldGenerator.h"
#include "civilisation/CivilisationLayer.h"
#include "areas/ArdaRegion.h"
namespace Arda::NaturalFeatures {

void detectMarshes(Fwg::Terrain::TerrainData &terrainData,
                   Fwg::Climate::ClimateData &climateData,
                   Arda::Civilization::CivilizationLayer &civLayer,
                   const Fwg::Cfg &config);

std::vector<float>
detectWastelands(Fwg::Terrain::TerrainData &terrainData,
    Fwg::Climate::ClimateData &climateData, std::vector<std::shared_ptr<Arda::ArdaRegion>> &ardaRegions,
                 Arda::Civilization::CivilizationLayer &civLayer,
                 const Fwg::Cfg &config);


void loadNaturalFeatures(Fwg::Cfg &config,
                         const Fwg::Gfx::Bitmap &inputFeatures,
                         Arda::Civilization::CivilizationLayer &civLayer);

} // namespace Wastelands