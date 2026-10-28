#include "simulation/SimulationEvents.h"

#include "simulation/SimulationState.h"

namespace Arda::Simulation::events {

namespace {
void updatePopulation(State &state, ProvinceId provinceId, double value) {
  if (auto *province =
          ::Arda::Simulation::state::findProvince(state, provinceId)) {
    const auto previousPopulation = province->population;
    province->population = value;
    if (previousPopulation > 0.0) {
      const auto factor = value / previousPopulation;
      for (auto &[cultureId, population] : province->culturePopulations)
        population *= factor;
    }
  }
}

void updateProvinceValue(State &state, ProvinceId provinceId, double value,
                         double ProvinceState::*member) {
  if (auto *province =
          ::Arda::Simulation::state::findProvince(state, provinceId))
    province->*member = value;
}

template <class Payload>
void assignPoliticalFields(Polity &polity, const Payload &event) {
  polity.governmentForm = event.governmentForm;
  polity.centralizationAffinity = event.centralizationAffinity;
  polity.lastGovernmentTransitionYear = event.lastGovernmentTransitionYear;
  polity.lastTraitEra = event.lastTraitEra;
  polity.politicalPower = event.politicalPower;
  polity.desiredPoliticalPower = event.desiredPoliticalPower;
  polity.politicalTraits = event.politicalTraits;
  polity.politicalPressures = event.politicalPressures;
  polity.governmentPopularity = event.governmentPopularity;
  polity.militaryInfluence = event.militaryInfluence;
  polity.desiredMilitaryInfluence = event.desiredMilitaryInfluence;
  polity.partyControl = event.partyControl;
  polity.desiredPartyControl = event.desiredPartyControl;
  polity.centralization = event.centralization;
  polity.armyTradition = event.armyTradition;
  polity.navyTradition = event.navyTradition;
  polity.stability = event.stability;
  polity.politicalMovements = event.politicalMovements;
}

template <class Payload>
void applyProvinceTransfer(State &state, const Payload &event) {
  if (auto *province =
          ::Arda::Simulation::state::findProvince(state, event.provinceId)) {
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
}

void applyPayload(State &state, const EventHeader &header,
                  const EventPayload<EventType::CreatePolity> &event,
                  std::string_view description) {
  {
    auto &polity =
        ::Arda::Simulation::state::ensurePolity(state, event.polityId);
    polity = {};
    polity.initialized = true;
    polity.id = event.polityId;
    polity.foundedYear = header.year;
    polity.isTribe = description == "tribe";
    polity.colour = event.colour;
    assignPoliticalFields(polity, event);
  }
}

void applyPayload(State &state, const EventHeader &header,
                  const EventPayload<EventType::CreateSuccessorPolity> &event,
                  std::string_view) {
  {
    auto &polity =
        ::Arda::Simulation::state::ensurePolity(state, event.polityId);
    polity = {};
    polity.initialized = true;
    polity.id = event.polityId;
    polity.foundedYear = header.year;
    polity.predecessorId = event.parentId >= 0
                               ? std::optional<PolityId>(event.parentId)
                               : std::nullopt;
    polity.colour = event.colour;
    polity.isTribe = event.governmentForm == GovernmentForm::Tribal;
    assignPoliticalFields(polity, event);
    if (const auto *parent =
            ::Arda::Simulation::state::findPolity(state, event.parentId)) {
      polity.primaryCulture = parent->primaryCulture;
      ::Arda::Simulation::state::findPolity(state, event.parentId)
          ->successorIds.push_back(event.polityId);
    }
  }
}

void applyPayload(State &state, const EventHeader &,
                  const EventPayload<EventType::PolityUpdate> &event,
                  std::string_view) {
  if (auto *polity =
          ::Arda::Simulation::state::findPolity(state, event.polityId)) {
    assignPoliticalFields(*polity, event);
    polity->isTribe = event.governmentForm == GovernmentForm::Tribal;
  }
}

void applyPayload(
    State &state, const EventHeader &,
    const EventPayload<EventType::ConvertCulturePopulation> &event,
    std::string_view) {
  if (auto *province =
          ::Arda::Simulation::state::findProvince(state, event.provinceId)) {
    const auto sourceCulture = static_cast<CultureId>(event.parentId);
    province->culturePopulations[sourceCulture] = std::max(
        0.0, province->culturePopulations[sourceCulture] - event.value);
    province->culturePopulations[event.cultureId] += event.value;
    ::Arda::Simulation::state::refreshDominantCulture(*province);
  }
}

void applyPayload(State &state, const EventHeader &,
                  const EventPayload<EventType::UpdatePolityStrength> &event,
                  std::string_view) {
  state.polityStrengths[event.polityId] = {event.value, event.secondaryValue,
                                           event.score};
}

void applyPayload(State &state, const EventHeader &header,
                  const EventPayload<EventType::InitializeProvince> &event,
                  std::string_view) {
  {
    auto &province =
        ::Arda::Simulation::state::ensureProvince(state, event.provinceId);
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
  state.cultures.try_emplace(
      event.cultureId, CultureLineage{event.cultureId, event.provinceId,
                                      header.year, std::nullopt, event.colour});
  if (auto *polity =
          ::Arda::Simulation::state::findPolity(state, event.polityId)) {
    if (polity->primaryCulture < 0)
      polity->primaryCulture = event.cultureId;
    if (polity->capitalProvince < 0)
      ::Arda::Simulation::state::setCapital(state, event.polityId,
                                            event.provinceId);
  }
}

void applyPayload(State &state, const EventHeader &,
                  const EventPayload<EventType::TransferProvince> &event,
                  std::string_view) {
  applyProvinceTransfer(state, event);
}

void applyPayload(State &state, const EventHeader &,
                  const EventPayload<EventType::ConsolidateRegion> &event,
                  std::string_view) {
  applyProvinceTransfer(state, event);
}

void applyPayload(State &state, const EventHeader &,
                  const EventPayload<EventType::SetCapital> &event,
                  std::string_view) {
  if (::Arda::Simulation::state::findPolity(state, event.polityId))
    ::Arda::Simulation::state::setCapital(state, event.polityId,
                                          event.provinceId);
}

void applyPayload(State &state, const EventHeader &,
                  const EventPayload<EventType::SetPrimaryCulture> &event,
                  std::string_view) {
  if (auto *polity =
          ::Arda::Simulation::state::findPolity(state, event.polityId))
    polity->primaryCulture = event.cultureId;
}

void applyPayload(State &state, const EventHeader &header,
                  const EventPayload<EventType::DissolvePolity> &event,
                  std::string_view) {
  if (auto *polity =
          ::Arda::Simulation::state::findPolity(state, event.polityId))
    polity->dissolvedYear = header.year;
}

void applyPayload(State &state, const EventHeader &header,
                  const EventPayload<EventType::CreateCulture> &event,
                  std::string_view) {
  state.cultures.try_emplace(
      event.cultureId,
      CultureLineage{event.cultureId, event.provinceId, header.year,
                     event.parentId >= 0
                         ? std::optional<CultureId>(event.parentId)
                         : std::nullopt,
                     event.colour});
}

void applyPayload(State &state, const EventHeader &,
                  const EventPayload<EventType::SetCulture> &event,
                  std::string_view) {
  if (auto *province =
          ::Arda::Simulation::state::findProvince(state, event.provinceId))
    province->culture = event.cultureId;
}

void applyPayload(State &state, const EventHeader &header,
                  const EventPayload<EventType::CreateReligion> &event,
                  std::string_view) {
  state.religions.try_emplace(event.religionId,
                              ReligionLineage{event.religionId,
                                              event.provinceId, header.year,
                                              std::nullopt, event.colour});
  if (event.parentId >= 0)
    state.religions.at(event.religionId).parentId = event.parentId;
}

void applyPayload(State &state, const EventHeader &,
                  const EventPayload<EventType::SetReligion> &event,
                  std::string_view) {
  if (auto *province =
          ::Arda::Simulation::state::findProvince(state, event.provinceId))
    province->religion = event.religionId;
}

void applyPayload(State &state, const EventHeader &,
                  const EventPayload<EventType::UpdatePopulation> &event,
                  std::string_view) {
  updatePopulation(state, event.provinceId, event.value);
}

void applyPayload(State &state, const EventHeader &,
                  const EventPayload<EventType::UpdateDevelopment> &event,
                  std::string_view) {
  updateProvinceValue(state, event.provinceId, event.value,
                      &ProvinceState::development);
}

void applyPayload(State &state, const EventHeader &,
                  const EventPayload<EventType::UpdateCarryingCapacity> &event,
                  std::string_view) {
  updateProvinceValue(state, event.provinceId, event.value,
                      &ProvinceState::carryingCapacity);
}

void applyPayload(State &, const EventHeader &,
                  const EventPayload<EventType::MigratePopulation> &,
                  std::string_view) {
  // Informational event; application intentionally has no state effect.
}

void applyPayload(State &state, const EventHeader &,
                  const EventPayload<EventType::SetRegionalPhase> &event,
                  std::string_view) {
  state.regionalPhases[event.regionId] =
      static_cast<RegionalPhase>(static_cast<int>(event.value));
}

void applyPayload(State &state, const EventHeader &,
                  const EventPayload<EventType::SetSuperRegion> &event,
                  std::string_view) {
  state.regionSuperRegions[event.regionId] =
      static_cast<SuperRegionId>(event.value);
}

void applyPayload(State &state, const EventHeader &,
                  const EventPayload<EventType::CreateSuperRegion> &event,
                  std::string_view) {
  state.superRegions.try_emplace(
      event.regionId, DevelopmentSuperRegion{event.regionId, event.polityId});
}

void applyPayload(State &state, const EventHeader &,
                  const EventPayload<EventType::SetSuperRegionPhase> &event,
                  std::string_view) {
  if (auto superRegion =
          state.superRegions.find(static_cast<SuperRegionId>(event.regionId));
      superRegion != state.superRegions.end())
    superRegion->second.phase =
        static_cast<SuperRegionPhase>(static_cast<int>(event.value));
}

void applyPayload(State &, const EventHeader &,
                  const EventPayload<EventType::MigrateCulturePopulation> &,
                  std::string_view) {
  // Informational event; application intentionally has no state effect.
}

void applyPayload(State &state, const EventHeader &,
                  const EventPayload<EventType::ColonizeProvince> &event,
                  std::string_view) {
  if (auto *province =
          ::Arda::Simulation::state::findProvince(state, event.provinceId)) {
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
    if (auto *newOwner =
            ::Arda::Simulation::state::findPolity(state, event.polityId);
        newOwner && newOwner->primaryCulture < 0)
      newOwner->primaryCulture = province->culture;
  }
}

} // namespace

void apply(State &state, const EventStore &events, size_t index) {
  events.visit(index, [&]<EventType Type>(const EventHeader &header,
                                          const EventPayload<Type> &payload) {
    state.year = header.year;
    std::string_view description;
    if constexpr (Type == EventType::CreatePolity)
      description = events.description(header.descriptionId);
    applyPayload(state, header, payload, description);
  });
}

} // namespace Arda::Simulation::events
