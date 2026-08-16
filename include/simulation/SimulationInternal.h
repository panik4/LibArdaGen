#pragma once

#include "simulation/Simulation.h"
#include <functional>

namespace Arda::Simulation::detail {

struct NormalizedInput {
  struct Environment {
    double habitability = 0.5;
    double arableLand = 0.5;
    double inclination = 0.0;
    bool coastal = false;
    bool island = false;
    size_t area = 0;
  };

  std::map<ProvinceId, std::shared_ptr<ArdaProvince>> provinces;
  std::map<ProvinceId, RegionId> provinceRegions;
  std::map<RegionId, std::vector<ProvinceId>> regions;
  std::map<ProvinceId, std::vector<ProvinceId>> neighbours;
  std::map<ProvinceId, int> provinceContinents;
  std::set<RegionId> islandRegions;
  SeaRouteMap seaRoutesFrom;
  SeaRouteMap seaRoutesTo;
  std::map<RegionId, int> regionContinents;
  std::map<RegionId, std::vector<RegionId>> regionNeighbours;
  std::map<ProvinceId, Environment> environments;
  std::vector<ValidationError> errors;
};

using AppendEvent = std::function<void(Event &&)>;

} // namespace Arda::Simulation::detail
