#pragma once

#include "simulation/Simulation.h"

namespace Arda::Simulation {

class SimulationExporter {
public:
  [[nodiscard]] static SimulationExport exportFinalState(
      const State &state, const std::map<ProvinceId, RegionId> &provinceRegions,
      const SimulationPolityHistory &polityHistory,
      const std::vector<Event> &events, const std::vector<WarEvent> &wars,
      const std::map<ProvinceId, size_t> &provinceAreas);
};

} // namespace Arda::Simulation
