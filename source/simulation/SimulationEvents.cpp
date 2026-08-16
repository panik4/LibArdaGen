#include "simulation/SimulationEvents.h"

#include "simulation/SimulationState.h"

namespace Arda::Simulation::events {

void apply(State &state, const Event &event) {
  state.year = event.year;
  switch (event.type) {
  case EventType::CreatePolity:
    state.polities[event.polityId] = {
        event.polityId, event.year, std::nullopt,
        std::nullopt,   {},         event.description == "tribe",
        event.colour,   -1,         -1};
    break;
  case EventType::CreateSuccessorPolity:
    state.polities[event.polityId] = {
        event.polityId,
        event.year,
        std::nullopt,
        event.parentId >= 0 ? std::optional<PolityId>(event.parentId)
                            : std::nullopt,
        {},
        false,
        event.colour,
        event.parentId >= 0 && state.polities.contains(event.parentId)
            ? state.polities.at(event.parentId).primaryCulture
            : -1,
        -1};
    if (event.parentId >= 0 && state.polities.contains(event.parentId))
      state.polities.at(event.parentId).successorIds.push_back(event.polityId);
    break;
  case EventType::ConvertCulturePopulation:
    if (auto province = state.provinces.find(event.provinceId);
        province != state.provinces.end()) {
      const auto sourceCulture = static_cast<CultureId>(event.parentId);
      province->second.culturePopulations[sourceCulture] =
          std::max(0.0, province->second.culturePopulations[sourceCulture] -
                            event.value);
      province->second.culturePopulations[event.cultureId] += event.value;
      ::Arda::Simulation::state::refreshDominantCulture(province->second);
    }
    break;
  case EventType::UpdatePolityStrength:
    state.polityStrengths[event.polityId] = {event.value, event.secondaryValue,
                                             event.score};
    break;
  case EventType::InitializeProvince:
    state.provinces[event.provinceId] = {event.polityId, event.cultureId,
                                         event.religionId, event.value,
                                         event.secondaryValue};
    state.provinces[event.provinceId].culturePopulations[event.cultureId] =
        event.value;
    state.provinces[event.provinceId].coastal = event.coastal;
    state.provinces[event.provinceId].island = event.island;
    state.provinces[event.provinceId].overseas = event.overseas;
    state.provinces[event.provinceId].colony = event.colony;
    if (event.polityId != NoPolity && event.coastal)
      state.coastalProvinceIds[event.polityId].insert(event.provinceId);
    state.cultures.try_emplace(event.cultureId,
                               CultureLineage{event.cultureId, event.provinceId,
                                              event.year, std::nullopt,
                                              event.colour});
    if (event.polityId != NoPolity && state.polities.contains(event.polityId)) {
      auto &polity = state.polities.at(event.polityId);
      if (polity.primaryCulture < 0)
        polity.primaryCulture = event.cultureId;
      if (polity.capitalProvince < 0)
        ::Arda::Simulation::state::setCapital(state, event.polityId,
                                               event.provinceId);
    }
    break;
  case EventType::TransferProvince:
  case EventType::ConsolidateRegion:
    if (auto province = state.provinces.find(event.provinceId);
        province != state.provinces.end()) {
      const auto previousOwner = province->second.owner;
      province->second.owner = event.polityId;
      province->second.overseas = event.overseas;
      province->second.colony = event.colony;
      if (province->second.coastal) {
        if (previousOwner != NoPolity)
          state.coastalProvinceIds[previousOwner].erase(event.provinceId);
        if (event.polityId != NoPolity)
          state.coastalProvinceIds[event.polityId].insert(event.provinceId);
      }
      ::Arda::Simulation::state::relocateCapital(state, event.previousPolityId);
      if (event.polityId != NoPolity &&
          state.polities.contains(event.polityId) &&
          state.polities.at(event.polityId).primaryCulture < 0)
        state.polities.at(event.polityId).primaryCulture =
            province->second.culture;
      if (event.polityId != NoPolity &&
          state.polities.contains(event.polityId) &&
          state.polities.at(event.polityId).capitalProvince < 0)
        ::Arda::Simulation::state::setCapital(state, event.polityId,
                                               event.provinceId);
    }
    break;
  case EventType::SetCapital:
    if (state.polities.contains(event.polityId))
      ::Arda::Simulation::state::setCapital(state, event.polityId,
                                             event.provinceId);
    break;
  case EventType::SetPrimaryCulture:
    if (state.polities.contains(event.polityId))
      state.polities.at(event.polityId).primaryCulture = event.cultureId;
    break;
  case EventType::DissolvePolity:
    if (auto polity = state.polities.find(event.polityId);
        polity != state.polities.end())
      polity->second.dissolvedYear = event.year;
    break;
  case EventType::CreateCulture:
    state.cultures.try_emplace(
        event.cultureId,
        CultureLineage{event.cultureId, event.provinceId, event.year,
                       event.parentId >= 0
                           ? std::optional<CultureId>(event.parentId)
                           : std::nullopt,
                       event.colour});
    break;
  case EventType::SetCulture:
    if (auto province = state.provinces.find(event.provinceId);
        province != state.provinces.end())
      province->second.culture = event.cultureId;
    break;
  case EventType::CreateReligion:
    state.religions.try_emplace(event.religionId,
                                ReligionLineage{event.religionId,
                                                event.provinceId, event.year,
                                                std::nullopt, event.colour});
    if (event.parentId >= 0)
      state.religions.at(event.religionId).parentId = event.parentId;
    break;
  case EventType::SetReligion:
    if (auto province = state.provinces.find(event.provinceId);
        province != state.provinces.end())
      province->second.religion = event.religionId;
    break;
  case EventType::UpdatePopulation:
    if (auto province = state.provinces.find(event.provinceId);
        province != state.provinces.end()) {
      const auto previousPopulation = province->second.population;
      province->second.population = event.value;
      if (previousPopulation > 0.0) {
        const auto factor = event.value / previousPopulation;
        for (auto &[cultureId, population] :
             province->second.culturePopulations)
          population *= factor;
      }
    }
    break;
  case EventType::UpdateDevelopment:
    if (auto province = state.provinces.find(event.provinceId);
        province != state.provinces.end())
      province->second.development = event.value;
    break;
  case EventType::UpdateCarryingCapacity:
    if (auto province = state.provinces.find(event.provinceId);
        province != state.provinces.end())
      province->second.carryingCapacity = event.value;
    break;
  case EventType::MigratePopulation:
    // if (auto source = state.provinces.find(event.provinceId);
    //     source != state.provinces.end())
    //   source->second.population =
    //       std::max(0.0, source->second.population - event.value);
    // if (auto target = state.provinces.find(event.regionId);
    //     target != state.provinces.end())
    //   target->second.population += event.value;
    break;
  case EventType::SetRegionalPhase:
    state.regionalPhases[event.regionId] =
        static_cast<RegionalPhase>(static_cast<int>(event.value));
    break;
  case EventType::SetSuperRegion:
    state.regionSuperRegions[event.regionId] =
        static_cast<SuperRegionId>(event.value);
    break;
  case EventType::CreateSuperRegion:
    state.superRegions.try_emplace(
        event.regionId, DevelopmentSuperRegion{event.regionId, event.polityId});
    break;
  case EventType::SetSuperRegionPhase:
    if (auto superRegion =
            state.superRegions.find(static_cast<SuperRegionId>(event.regionId));
        superRegion != state.superRegions.end())
      superRegion->second.phase =
          static_cast<SuperRegionPhase>(static_cast<int>(event.value));
    break;
  case EventType::MigrateCulturePopulation:
    // if (auto source = state.provinces.find(event.provinceId);
    //     source != state.provinces.end())
    //   source->second.culturePopulations[event.cultureId] =
    //       std::max(0.0, source->second.culturePopulations[event.cultureId] -
    //                         event.value);
    // if (auto target = state.provinces.find(event.regionId);
    //     target != state.provinces.end())
    //   target->second.culturePopulations[event.cultureId] += event.value;
    // if (auto source = state.provinces.find(event.provinceId);
    //     source != state.provinces.end())
    //   source->second.culture = state.dominantCultureOf(event.provinceId);
    // if (auto target = state.provinces.find(event.regionId);
    //     target != state.provinces.end())
    //   target->second.culture = state.dominantCultureOf(event.regionId);
    break;
  case EventType::ColonizeProvince:
    if (auto province = state.provinces.find(event.provinceId);
        province != state.provinces.end()) {
      const auto previousOwner = province->second.owner;
      province->second.owner = event.polityId;
      province->second.overseas = true;
      province->second.colony = true;
      if (province->second.coastal) {
        if (previousOwner != NoPolity)
          state.coastalProvinceIds[previousOwner].erase(event.provinceId);
        if (event.polityId != NoPolity)
          state.coastalProvinceIds[event.polityId].insert(event.provinceId);
      }
      ::Arda::Simulation::state::relocateCapital(state, event.previousPolityId);
      if (event.polityId != NoPolity &&
          state.polities.contains(event.polityId) &&
          state.polities.at(event.polityId).primaryCulture < 0)
        state.polities.at(event.polityId).primaryCulture =
            province->second.culture;
    }
    break;
  default:
    break;
  }
}

} // namespace Arda::Simulation::events
