#pragma once
#include "rendering/Visualization.h"
#include "civilisation/ArdaCivilisations.h"

namespace Arda::Gfx {
Fwg::Gfx::Bitmap
displayCultureGroups(const Arda::Civilization::CivilizationData &civData);
Fwg::Gfx::Bitmap
displayCultures(const std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions);
	
	



} // namespace Arda::Gfx