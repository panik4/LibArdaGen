#pragma once
#include "areas/SuperRegion.h"
#include "civilisation/ArdaCivilisations.h"
#include "rendering/Visualization.h"

namespace Arda::Gfx {
Fwg::Gfx::Bitmap
displayCultureGroups(const Arda::Civilization::CivilizationData &civData);
Fwg::Gfx::Bitmap
displayCultures(const std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions);
Fwg::Gfx::Bitmap visualiseStrategicRegions(
    Fwg::Gfx::Bitmap &superRegionMap,
    const std::vector<std::shared_ptr<Arda::SuperRegion>> &superRegions,
    const int ID = -1);

} // namespace Arda::Gfx