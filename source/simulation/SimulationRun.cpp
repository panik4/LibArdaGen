#include "simulation/SimulationRun.h"

#include "simulation/SimulationEvents.h"

namespace Arda::Simulation::detail {

SimulationRun::SimulationRun()
    : append([this](Event &&event) { appendEvent(std::move(event)); }) {}

void SimulationRun::appendEvent(Event event) {
  result.events.emplace_back(std::move(event));
  auto &storedEvent = result.events.back();
  if (storedEvent.type == EventType::InitializeProvince) {
    const auto provinceId = storedEvent.provinceId;
    storedEvent.coastal = normalized.environments.at(provinceId).coastal;
    storedEvent.island = normalized.environments.at(provinceId).island;
  }
  ++result.eventCounts[storedEvent.type];
  events::apply(state, storedEvent);
  if (storedEvent.type == EventType::InitializeProvince ||
      storedEvent.type == EventType::TransferProvince ||
      storedEvent.type == EventType::ColonizeProvince ||
      storedEvent.type == EventType::ConsolidateRegion) {
    if (storedEvent.previousPolityId != NoPolity)
      result.polityHistory.currentProvinceIds[storedEvent.previousPolityId]
          .erase(storedEvent.provinceId);
    if (storedEvent.polityId != NoPolity) {
      auto &current =
          result.polityHistory.currentProvinceIds[storedEvent.polityId];
      current.insert(storedEvent.provinceId);
      auto &peak = result.polityHistory.peakProvinceIds[storedEvent.polityId];
      if (current.size() > peak.size())
        peak.assign(current.begin(), current.end());
      if (storedEvent.regionId >= 0)
        result.polityHistory.historicalRegionOwners[storedEvent.regionId]
            .insert(storedEvent.polityId);
    }
  }
}

} // namespace Arda::Simulation::detail
