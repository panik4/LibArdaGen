#include "simulation/SimulationRun.h"

#include "simulation/SimulationEvents.h"

namespace Arda::Simulation::detail {

SimulationRun::SimulationRun()
    : append([this](Event &&event) { appendEvent(std::move(event)); }) {}

void SimulationRun::appendEvent(Event event) {
  if (event.type == EventType::InitializeProvince) {
    const auto provinceId = event.provinceId;
    event.coastal = normalized.environments.at(provinceId).coastal;
    event.island = normalized.environments.at(provinceId).island;
  }
  const auto index = result.events.append(std::move(event));
  const auto type = result.events.header(index).type;
  ++result.eventCounts[type];
  events::apply(state, result.events, index);
  if (type == EventType::InitializeProvince ||
      type == EventType::TransferProvince ||
      type == EventType::ColonizeProvince ||
      type == EventType::ConsolidateRegion) {
    result.events.visit(index, [&]<EventType Type>(
                                   const EventHeader &,
                                   const EventPayload<Type> &storedEvent) {
      if constexpr (Type == EventType::InitializeProvince ||
                    Type == EventType::TransferProvince ||
                    Type == EventType::ColonizeProvince ||
                    Type == EventType::ConsolidateRegion) {
        PolityId previousPolityId = NoPolity;
        if constexpr (Type != EventType::InitializeProvince)
          previousPolityId = storedEvent.previousPolityId;
        if (previousPolityId != NoPolity)
          result.polityHistory.currentProvinceIds[previousPolityId].erase(
              storedEvent.provinceId);
        if (storedEvent.polityId != NoPolity) {
          auto &current =
              result.polityHistory.currentProvinceIds[storedEvent.polityId];
          current.insert(storedEvent.provinceId);
          auto &peak =
              result.polityHistory.peakProvinceIds[storedEvent.polityId];
          if (current.size() > peak.size())
            peak.assign(current.begin(), current.end());
          if (storedEvent.regionId >= 0)
            result.polityHistory.historicalRegionOwners[storedEvent.regionId]
                .insert(storedEvent.polityId);
        }
      }
    });
  }
}

} // namespace Arda::Simulation::detail
