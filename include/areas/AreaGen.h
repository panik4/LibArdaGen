#pragma once
#include "areas/ArdaProvince.h"
#include "areas/ArdaRegion.h"
#include "areas/SuperRegion.h"
#include "utils/ArdaUtils.h"
#include <map>
namespace Arda::Areas {
void loadStrategicRegions(
    const Fwg::Gfx::Image &inputImage,
    std::function<std::shared_ptr<SuperRegion>()> factory,
    std::vector<std::shared_ptr<SuperRegion>> &superRegions,
    std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions,
    const Fwg::Terrain::TerrainData &terrainData);
void generateStrategicRegions(
    std::function<std::shared_ptr<SuperRegion>()> factory,
    std::vector<std::shared_ptr<SuperRegion>> &superRegions,
    std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions,
    const float &superRegionFactor);
void saveRegions(std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions,
                 const std::string &mappingPath,
                 const Fwg::Gfx::Image &regionImage);

} // namespace Arda::Areas