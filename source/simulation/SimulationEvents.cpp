#include "simulation/SimulationEvents.h"

#include "simulation/SimulationState.h"

namespace Arda::Simulation::events {

void apply(State &state, const Event &event) {
  state.year = event.year;
  switch (event.type) {
  case EventType::CreatePolity:
    {
      auto &polity = ::Arda::Simulation::state::ensurePolity(state,
                                                               event.polityId);
      polity = {};
      polity.initialized = true;
      polity.id = event.polityId;
      polity.foundedYear = event.year;
      polity.isTribe = event.description == "tribe";
      polity.colour = event.colour;
    }
    break;
  case EventType::CreateSuccessorPolity:
    {
      auto &polity = ::Arda::Simulation::state::ensurePolity(state,
                                                               event.polityId);
      polity = {};
      polity.initialized = true;
      polity.id = event.polityId;
      polity.foundedYear = event.year;
      polity.predecessorId = event.parentId >= 0
                                 ? std::optional<PolityId>(event.parentId)
                                 : std::nullopt;
      polity.colour = event.colour;
      if (const auto *parent =
              ::Arda::Simulation::state::findPolity(state, event.parentId)) {
        polity.primaryCulture = parent->primaryCulture;
        ::Arda::Simulation::state::findPolity(state, event.parentId)
            ->successorIds.push_back(event.polityId);
      }
    }
    break;
  case EventType::ConvertCulturePopulation:
    if (auto *province = ::Arda::Simulation::state::findProvince(
            state, event.provinceId)) {
      const auto sourceCulture = static_cast<CultureId>(event.parentId);
      province->culturePopulations[sourceCulture] =
          std::max(0.0, province->culturePopulations[sourceCulture] -
                            event.value);
      province->culturePopulations[event.cultureId] += event.value;
      ::Arda::Simulation::state::refreshDominantCulture(*province);
    }
    break;
  case EventType::UpdatePolityStrength:
    state.polityStrengths[event.polityId] = {event.value, event.secondaryValue,
                                             event.score};
    break;
  case EventType::InitializeProvince:
    {
      auto &province = ::Arda::Simulation::state::ensureProvince(
          state, event.provinceId);
      province = {};
      province.initialized = true;
      province.owner = event.polityId;
      province.culture = event.cultureId;
      province.religion = event.religionId;
      province.population = event.value;
      province.development = event.secondaryValue;
      province.culturePopulations[event.cultureId] = event.value;
      province.landMassID = event.landMassID;
      province.continentId = event.continentId;
      province.coastal = event.coastal;
      province.island = event.island;
      province.overseas = event.overseas;
      province.colony = event.colony;
    }
    if (event.polityId != NoPolity && event.coastal)
      state.coastalProvinceIds[event.polityId].insert(event.provinceId);
    state.cultures.try_emplace(event.cultureId,
                               CultureLineage{event.cultureId, event.provinceId,
                                              event.year, std::nullopt,
                                              event.colour});
    if (auto *polity = ::Arda::Simulation::state::findPolity(
            state, event.polityId)) {
      if (polity->primaryCulture < 0)
        polity->primaryCulture = event.cultureId;
      if (polity->capitalProvince < 0)
        ::Arda::Simulation::state::setCapital(state, event.polityId,
                                               event.provinceId);
    }
    break;
  case EventType::TransferProvince:
  case EventType::ConsolidateRegion:
    if (auto *province = ::Arda::Simulation::state::findProvince(
            state, event.provinceId)) {
      const auto previousOwner = province->owner;
      province->owner = event.polityId;
      province->overseas = event.overseas;
      province->colony = event.colony;
      if (province->coastal) {
        if (previousOwner != NoPolity)
          state.coastalProvinceIds[previousOwner].erase(event.provinceId);
        if (event.polityId != NoPolity)
          state.coastalProvinceIds[event.polityId].insert(event.provinceId);
      }
      ::Arda::Simulation::state::relocateCapital(state, event.previousPolityId);
      auto *newOwner =
          ::Arda::Simulation::state::findPolity(state, event.polityId);
      if (newOwner && newOwner->primaryCulture < 0)
        newOwner->primaryCulture = province->culture;
      if (newOwner && newOwner->capitalProvince < 0)
        ::Arda::Simulation::state::setCapital(state, event.polityId,
                                               event.provinceId);
    }
    break;
  case EventType::SetCapital:
    if (::Arda::Simulation::state::findPolity(state, event.polityId))
      ::Arda::Simulation::state::setCapital(state, event.polityId,
                                             event.provinceId);
    break;
  case EventType::SetPrimaryCulture:
    if (auto *polity = ::Arda::Simulation::state::findPolity(
            state, event.polityId))
      polity->primaryCulture = event.cultureId;
    break;
  case EventType::DissolvePolity:
    if (auto *polity = ::Arda::Simulation::state::findPolity(
            state, event.polityId))
      polity->dissolvedYear = event.year;
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
    if (auto *province = ::Arda::Simulation::state::findProvince(
            state, event.provinceId))
      province->culture = event.cultureId;
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
    if (auto *province = ::Arda::Simulation::state::findProvince(
            state, event.provinceId))
      province->religion = event.religionId;
    break;
  case EventType::UpdatePopulation:
    if (auto *province = ::Arda::Simulation::state::findProvince(
            state, event.provinceId)) {
      const auto previousPopulation = province->population;
      province->population = event.value;
      if (previousPopulation > 0.0) {
        const auto factor = event.value / previousPopulation;
        for (auto &[cultureId, population] :
             province->culturePopulations)
          population *= factor;
      }
    }
    break;
  case EventType::UpdateDevelopment:
    if (auto *province = ::Arda::Simulation::state::findProvince(
            state, event.provinceId))
      province->development = event.value;
    break;
  case EventType::UpdateCarryingCapacity:
    if (auto *province = ::Arda::Simulation::state::findProvince(
            state, event.provinceId))
      province->carryingCapacity = event.value;
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
    if (auto *province = ::Arda::Simulation::state::findProvince(
            state, event.provinceId)) {
      const auto previousOwner = province->owner;
      province->owner = event.polityId;
      province->overseas = true;
      province->colony = true;
      if (province->coastal) {
        if (previousOwner != NoPolity)
          state.coastalProvinceIds[previousOwner].erase(event.provinceId);
        if (event.polityId != NoPolity)
          state.coastalProvinceIds[event.polityId].insert(event.provinceId);
      }
      ::Arda::Simulation::state::relocateCapital(state, event.previousPolityId);
      if (auto *newOwner = ::Arda::Simulation::state::findPolity(
              state, event.polityId);
          newOwner && newOwner->primaryCulture < 0)
        newOwner->primaryCulture = province->culture;
    }
    break;
  default:
    break;
  }
}

} // namespace Arda::Simulation::events
