#pragma once
#include "areas/ArdaContinent.h"
#include "areas/ArdaProvince.h"
#include "areas/ArdaRegion.h"
#include "areas/SuperRegion.h"
#include <memory>
#include <vector>

namespace Arda::Areas {
// Typed references to the shared geography, plus Arda's superregions.
struct AreaData {
  std::vector<std::shared_ptr<ArdaContinent>> continents;
  std::vector<std::shared_ptr<ArdaRegion>> regions;
  std::vector<std::shared_ptr<ArdaProvince>> provinces;
  std::vector<std::shared_ptr<SuperRegion>> superRegions;
};
} // namespace Arda::Areas
