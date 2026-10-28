#include "simulation/Simulation.h"
#include <limits>

namespace Arda::Simulation {

size_t EventStore::append(Event &&event) {
  auto found = descriptionIds.find(event.description);
  if (found == descriptionIds.end()) {
    if (descriptions.size() >= std::numeric_limits<uint32_t>::max())
      throw std::length_error("Too many simulation event descriptions");
    const auto id = static_cast<uint32_t>(descriptions.size());
    descriptions.push_back(event.description);
    try {
      found = descriptionIds.emplace(descriptions.back(), id).first;
    } catch (...) {
      descriptions.pop_back();
      throw;
    }
  }
  const auto descriptionId = found->second;
  const auto index = chronology.size();
  const auto appendRecord = [&]<class Payload>(std::vector<Payload> &records,
                                               Payload payload) {
    records.push_back(std::move(payload));
    try {
      chronology.push_back(
          {event.year, event.type, records.size() - 1, descriptionId});
    } catch (...) {
      records.pop_back();
      throw;
    }
  };
  switch (event.type) {
  case EventType::InitializeProvince: {
    auto &records = recordsInitializeProvince;
    appendRecord(records,
                 {std::move(event.provinceId), std::move(event.regionId),
                  std::move(event.polityId), std::move(event.cultureId),
                  std::move(event.religionId), std::move(event.value),
                  std::move(event.secondaryValue), std::move(event.colour),
                  std::move(event.coastal), std::move(event.island),
                  std::move(event.overseas), std::move(event.colony),
                  std::move(event.landMassID), std::move(event.continentId)});
    break;
  }
  case EventType::CreatePolity: {
    auto &records = recordsCreatePolity;
    appendRecord(records, {std::move(event.polityId),
                           std::move(event.parentId),
                           std::move(event.colour),
                           std::move(event.armyTradition),
                           std::move(event.navyTradition),
                           std::move(event.stability),
                           std::move(event.centralization),
                           std::move(event.governmentForm),
                           std::move(event.centralizationAffinity),
                           std::move(event.lastGovernmentTransitionYear),
                           std::move(event.politicalPower),
                           std::move(event.desiredPoliticalPower),
                           std::move(event.politicalTraits),
                           std::move(event.politicalPressures),
                           std::move(event.governmentPopularity),
                           std::move(event.lastTraitEra),
                           std::move(event.militaryInfluence),
                           std::move(event.desiredMilitaryInfluence),
                           std::move(event.partyControl),
                           std::move(event.desiredPartyControl),
                           std::move(event.politicalMovements)});
    break;
  }
  case EventType::CreateSuccessorPolity: {
    auto &records = recordsCreateSuccessorPolity;
    appendRecord(records, {std::move(event.polityId),
                           std::move(event.parentId),
                           std::move(event.colour),
                           std::move(event.armyTradition),
                           std::move(event.navyTradition),
                           std::move(event.stability),
                           std::move(event.centralization),
                           std::move(event.governmentForm),
                           std::move(event.centralizationAffinity),
                           std::move(event.lastGovernmentTransitionYear),
                           std::move(event.politicalPower),
                           std::move(event.desiredPoliticalPower),
                           std::move(event.politicalTraits),
                           std::move(event.politicalPressures),
                           std::move(event.governmentPopularity),
                           std::move(event.lastTraitEra),
                           std::move(event.militaryInfluence),
                           std::move(event.desiredMilitaryInfluence),
                           std::move(event.partyControl),
                           std::move(event.desiredPartyControl),
                           std::move(event.politicalMovements)});
    break;
  }
  case EventType::TransferProvince: {
    auto &records = recordsTransferProvince;
    appendRecord(records,
                 {std::move(event.provinceId), std::move(event.regionId),
                  std::move(event.polityId), std::move(event.previousPolityId),
                  std::move(event.warId), std::move(event.overseas),
                  std::move(event.colony)});
    break;
  }
  case EventType::DissolvePolity: {
    auto &records = recordsDissolvePolity;
    appendRecord(records, {std::move(event.polityId)});
    break;
  }
  case EventType::CreateCulture: {
    auto &records = recordsCreateCulture;
    appendRecord(records,
                 {std::move(event.provinceId), std::move(event.cultureId),
                  std::move(event.parentId), std::move(event.colour)});
    break;
  }
  case EventType::SetCulture: {
    auto &records = recordsSetCulture;
    appendRecord(records,
                 {std::move(event.provinceId), std::move(event.cultureId)});
    break;
  }
  case EventType::SetPrimaryCulture: {
    auto &records = recordsSetPrimaryCulture;
    appendRecord(records,
                 {std::move(event.polityId), std::move(event.cultureId)});
    break;
  }
  case EventType::CreateReligion: {
    auto &records = recordsCreateReligion;
    appendRecord(records,
                 {std::move(event.provinceId), std::move(event.religionId),
                  std::move(event.parentId), std::move(event.colour)});
    break;
  }
  case EventType::SetReligion: {
    auto &records = recordsSetReligion;
    appendRecord(records,
                 {std::move(event.provinceId), std::move(event.religionId)});
    break;
  }
  case EventType::UpdatePopulation: {
    auto &records = recordsUpdatePopulation;
    appendRecord(records,
                 {std::move(event.provinceId), std::move(event.value)});
    break;
  }
  case EventType::UpdatePolityStrength: {
    auto &records = recordsUpdatePolityStrength;
    appendRecord(records,
                 {std::move(event.polityId), std::move(event.value),
                  std::move(event.secondaryValue), std::move(event.score)});
    break;
  }
  case EventType::UpdateDevelopment: {
    auto &records = recordsUpdateDevelopment;
    appendRecord(records,
                 {std::move(event.provinceId), std::move(event.value)});
    break;
  }
  case EventType::UpdateCarryingCapacity: {
    auto &records = recordsUpdateCarryingCapacity;
    appendRecord(records,
                 {std::move(event.provinceId), std::move(event.regionId),
                  std::move(event.value), std::move(event.secondaryValue)});
    break;
  }
  case EventType::MigratePopulation: {
    auto &records = recordsMigratePopulation;
    appendRecord(records, {std::move(event.provinceId),
                           std::move(event.regionId), std::move(event.value)});
    break;
  }
  case EventType::SetRegionalPhase: {
    auto &records = recordsSetRegionalPhase;
    appendRecord(records, {std::move(event.regionId), std::move(event.value)});
    break;
  }
  case EventType::SetSuperRegion: {
    auto &records = recordsSetSuperRegion;
    appendRecord(records, {std::move(event.regionId), std::move(event.value)});
    break;
  }
  case EventType::CreateSuperRegion: {
    auto &records = recordsCreateSuperRegion;
    appendRecord(records,
                 {std::move(event.regionId), std::move(event.polityId)});
    break;
  }
  case EventType::SetSuperRegionPhase: {
    auto &records = recordsSetSuperRegionPhase;
    appendRecord(records, {std::move(event.regionId), std::move(event.value)});
    break;
  }
  case EventType::MigrateCulturePopulation: {
    auto &records = recordsMigrateCulturePopulation;
    appendRecord(records,
                 {std::move(event.provinceId), std::move(event.regionId),
                  std::move(event.cultureId), std::move(event.value)});
    break;
  }
  case EventType::ConvertCulturePopulation: {
    auto &records = recordsConvertCulturePopulation;
    appendRecord(records,
                 {std::move(event.provinceId), std::move(event.cultureId),
                  std::move(event.parentId), std::move(event.value)});
    break;
  }
  case EventType::ColonizeProvince: {
    auto &records = recordsColonizeProvince;
    appendRecord(records,
                 {std::move(event.provinceId), std::move(event.regionId),
                  std::move(event.polityId), std::move(event.previousPolityId),
                  std::move(event.warId), std::move(event.overseas),
                  std::move(event.colony)});
    break;
  }
  case EventType::ConsolidateRegion: {
    auto &records = recordsConsolidateRegion;
    appendRecord(records,
                 {std::move(event.provinceId), std::move(event.regionId),
                  std::move(event.polityId), std::move(event.previousPolityId),
                  std::move(event.warId), std::move(event.overseas),
                  std::move(event.colony)});
    break;
  }
  case EventType::SetCapital: {
    auto &records = recordsSetCapital;
    appendRecord(records,
                 {std::move(event.provinceId), std::move(event.polityId)});
    break;
  }
  case EventType::PolityUpdate: {
    auto &records = recordsPolityUpdate;
    appendRecord(
        records,
        {std::move(event.polityId), std::move(event.armyTradition),
         std::move(event.navyTradition), std::move(event.stability),
         std::move(event.centralization), std::move(event.governmentForm),
         std::move(event.centralizationAffinity),
         std::move(event.lastGovernmentTransitionYear),
         std::move(event.politicalPower),
         std::move(event.desiredPoliticalPower),
         std::move(event.politicalTraits), std::move(event.politicalPressures),
         std::move(event.governmentPopularity), std::move(event.lastTraitEra),
         std::move(event.militaryInfluence),
         std::move(event.desiredMilitaryInfluence),
         std::move(event.partyControl), std::move(event.desiredPartyControl),
         std::move(event.politicalMovements)});
    break;
  }
  default:
    throw std::invalid_argument("Unknown simulation event type");
  }
  return index;
}

namespace {
void copyPayload(Event &event,
                 const EventPayload<EventType::InitializeProvince> &payload) {
  event.provinceId = payload.provinceId;
  event.regionId = payload.regionId;
  event.polityId = payload.polityId;
  event.cultureId = payload.cultureId;
  event.religionId = payload.religionId;
  event.value = payload.value;
  event.secondaryValue = payload.secondaryValue;
  event.colour = payload.colour;
  event.coastal = payload.coastal;
  event.island = payload.island;
  event.overseas = payload.overseas;
  event.colony = payload.colony;
  event.landMassID = payload.landMassID;
  event.continentId = payload.continentId;
}

void copyPayload(Event &event,
                 const EventPayload<EventType::CreatePolity> &payload) {
  event.polityId = payload.polityId;
  event.parentId = payload.parentId;
  event.colour = payload.colour;
  event.armyTradition = payload.armyTradition;
  event.navyTradition = payload.navyTradition;
  event.stability = payload.stability;
  event.centralization = payload.centralization;
  event.governmentForm = payload.governmentForm;
  event.centralizationAffinity = payload.centralizationAffinity;
  event.lastGovernmentTransitionYear = payload.lastGovernmentTransitionYear;
  event.politicalPower = payload.politicalPower;
  event.desiredPoliticalPower = payload.desiredPoliticalPower;
  event.politicalTraits = payload.politicalTraits;
  event.politicalPressures = payload.politicalPressures;
  event.governmentPopularity = payload.governmentPopularity;
  event.lastTraitEra = payload.lastTraitEra;
  event.militaryInfluence = payload.militaryInfluence;
  event.desiredMilitaryInfluence = payload.desiredMilitaryInfluence;
  event.partyControl = payload.partyControl;
  event.desiredPartyControl = payload.desiredPartyControl;
  event.politicalMovements = payload.politicalMovements;
}

void copyPayload(
    Event &event,
    const EventPayload<EventType::CreateSuccessorPolity> &payload) {
  event.polityId = payload.polityId;
  event.parentId = payload.parentId;
  event.colour = payload.colour;
  event.armyTradition = payload.armyTradition;
  event.navyTradition = payload.navyTradition;
  event.stability = payload.stability;
  event.centralization = payload.centralization;
  event.governmentForm = payload.governmentForm;
  event.centralizationAffinity = payload.centralizationAffinity;
  event.lastGovernmentTransitionYear = payload.lastGovernmentTransitionYear;
  event.politicalPower = payload.politicalPower;
  event.desiredPoliticalPower = payload.desiredPoliticalPower;
  event.politicalTraits = payload.politicalTraits;
  event.politicalPressures = payload.politicalPressures;
  event.governmentPopularity = payload.governmentPopularity;
  event.lastTraitEra = payload.lastTraitEra;
  event.militaryInfluence = payload.militaryInfluence;
  event.desiredMilitaryInfluence = payload.desiredMilitaryInfluence;
  event.partyControl = payload.partyControl;
  event.desiredPartyControl = payload.desiredPartyControl;
  event.politicalMovements = payload.politicalMovements;
}

void copyPayload(Event &event,
                 const EventPayload<EventType::TransferProvince> &payload) {
  event.provinceId = payload.provinceId;
  event.regionId = payload.regionId;
  event.polityId = payload.polityId;
  event.previousPolityId = payload.previousPolityId;
  event.warId = payload.warId;
  event.overseas = payload.overseas;
  event.colony = payload.colony;
}

void copyPayload(Event &event,
                 const EventPayload<EventType::DissolvePolity> &payload) {
  event.polityId = payload.polityId;
}

void copyPayload(Event &event,
                 const EventPayload<EventType::CreateCulture> &payload) {
  event.provinceId = payload.provinceId;
  event.cultureId = payload.cultureId;
  event.parentId = payload.parentId;
  event.colour = payload.colour;
}

void copyPayload(Event &event,
                 const EventPayload<EventType::SetCulture> &payload) {
  event.provinceId = payload.provinceId;
  event.cultureId = payload.cultureId;
}

void copyPayload(Event &event,
                 const EventPayload<EventType::SetPrimaryCulture> &payload) {
  event.polityId = payload.polityId;
  event.cultureId = payload.cultureId;
}

void copyPayload(Event &event,
                 const EventPayload<EventType::CreateReligion> &payload) {
  event.provinceId = payload.provinceId;
  event.religionId = payload.religionId;
  event.parentId = payload.parentId;
  event.colour = payload.colour;
}

void copyPayload(Event &event,
                 const EventPayload<EventType::SetReligion> &payload) {
  event.provinceId = payload.provinceId;
  event.religionId = payload.religionId;
}

void copyPayload(Event &event,
                 const EventPayload<EventType::UpdatePopulation> &payload) {
  event.provinceId = payload.provinceId;
  event.value = payload.value;
}

void copyPayload(Event &event,
                 const EventPayload<EventType::UpdatePolityStrength> &payload) {
  event.polityId = payload.polityId;
  event.value = payload.value;
  event.secondaryValue = payload.secondaryValue;
  event.score = payload.score;
}

void copyPayload(Event &event,
                 const EventPayload<EventType::UpdateDevelopment> &payload) {
  event.provinceId = payload.provinceId;
  event.value = payload.value;
}

void copyPayload(
    Event &event,
    const EventPayload<EventType::UpdateCarryingCapacity> &payload) {
  event.provinceId = payload.provinceId;
  event.regionId = payload.regionId;
  event.value = payload.value;
  event.secondaryValue = payload.secondaryValue;
}

void copyPayload(Event &event,
                 const EventPayload<EventType::MigratePopulation> &payload) {
  event.provinceId = payload.provinceId;
  event.regionId = payload.regionId;
  event.value = payload.value;
}

void copyPayload(Event &event,
                 const EventPayload<EventType::SetRegionalPhase> &payload) {
  event.regionId = payload.regionId;
  event.value = payload.value;
}

void copyPayload(Event &event,
                 const EventPayload<EventType::SetSuperRegion> &payload) {
  event.regionId = payload.regionId;
  event.value = payload.value;
}

void copyPayload(Event &event,
                 const EventPayload<EventType::CreateSuperRegion> &payload) {
  event.regionId = payload.regionId;
  event.polityId = payload.polityId;
}

void copyPayload(Event &event,
                 const EventPayload<EventType::SetSuperRegionPhase> &payload) {
  event.regionId = payload.regionId;
  event.value = payload.value;
}

void copyPayload(
    Event &event,
    const EventPayload<EventType::MigrateCulturePopulation> &payload) {
  event.provinceId = payload.provinceId;
  event.regionId = payload.regionId;
  event.cultureId = payload.cultureId;
  event.value = payload.value;
}

void copyPayload(
    Event &event,
    const EventPayload<EventType::ConvertCulturePopulation> &payload) {
  event.provinceId = payload.provinceId;
  event.cultureId = payload.cultureId;
  event.parentId = payload.parentId;
  event.value = payload.value;
}

void copyPayload(Event &event,
                 const EventPayload<EventType::ColonizeProvince> &payload) {
  event.provinceId = payload.provinceId;
  event.regionId = payload.regionId;
  event.polityId = payload.polityId;
  event.previousPolityId = payload.previousPolityId;
  event.warId = payload.warId;
  event.overseas = payload.overseas;
  event.colony = payload.colony;
}

void copyPayload(Event &event,
                 const EventPayload<EventType::ConsolidateRegion> &payload) {
  event.provinceId = payload.provinceId;
  event.regionId = payload.regionId;
  event.polityId = payload.polityId;
  event.previousPolityId = payload.previousPolityId;
  event.warId = payload.warId;
  event.overseas = payload.overseas;
  event.colony = payload.colony;
}

void copyPayload(Event &event,
                 const EventPayload<EventType::SetCapital> &payload) {
  event.provinceId = payload.provinceId;
  event.polityId = payload.polityId;
}

void copyPayload(Event &event,
                 const EventPayload<EventType::PolityUpdate> &payload) {
  event.polityId = payload.polityId;
  event.armyTradition = payload.armyTradition;
  event.navyTradition = payload.navyTradition;
  event.stability = payload.stability;
  event.centralization = payload.centralization;
  event.governmentForm = payload.governmentForm;
  event.centralizationAffinity = payload.centralizationAffinity;
  event.lastGovernmentTransitionYear = payload.lastGovernmentTransitionYear;
  event.politicalPower = payload.politicalPower;
  event.desiredPoliticalPower = payload.desiredPoliticalPower;
  event.politicalTraits = payload.politicalTraits;
  event.politicalPressures = payload.politicalPressures;
  event.governmentPopularity = payload.governmentPopularity;
  event.lastTraitEra = payload.lastTraitEra;
  event.militaryInfluence = payload.militaryInfluence;
  event.desiredMilitaryInfluence = payload.desiredMilitaryInfluence;
  event.partyControl = payload.partyControl;
  event.desiredPartyControl = payload.desiredPartyControl;
  event.politicalMovements = payload.politicalMovements;
}
} // namespace

Event EventStore::materialize(size_t index) const {
  Event event;
  visit(index, [&](const EventHeader &header, const auto &payload) {
    event.year = header.year;
    event.type = header.type;
    event.description = description(header.descriptionId);
    copyPayload(event, payload);
  });
  return event;
}

size_t EventStore::bufferBytes() const {
  return chronology.capacity() * sizeof(EventHeader) +
         descriptions.capacity() * sizeof(std::string) +
         recordsInitializeProvince.capacity() *
             sizeof(EventPayload<EventType::InitializeProvince>) +
         recordsCreatePolity.capacity() *
             sizeof(EventPayload<EventType::CreatePolity>) +
         recordsCreateSuccessorPolity.capacity() *
             sizeof(EventPayload<EventType::CreateSuccessorPolity>) +
         recordsTransferProvince.capacity() *
             sizeof(EventPayload<EventType::TransferProvince>) +
         recordsDissolvePolity.capacity() *
             sizeof(EventPayload<EventType::DissolvePolity>) +
         recordsCreateCulture.capacity() *
             sizeof(EventPayload<EventType::CreateCulture>) +
         recordsSetCulture.capacity() *
             sizeof(EventPayload<EventType::SetCulture>) +
         recordsSetPrimaryCulture.capacity() *
             sizeof(EventPayload<EventType::SetPrimaryCulture>) +
         recordsCreateReligion.capacity() *
             sizeof(EventPayload<EventType::CreateReligion>) +
         recordsSetReligion.capacity() *
             sizeof(EventPayload<EventType::SetReligion>) +
         recordsUpdatePopulation.capacity() *
             sizeof(EventPayload<EventType::UpdatePopulation>) +
         recordsUpdatePolityStrength.capacity() *
             sizeof(EventPayload<EventType::UpdatePolityStrength>) +
         recordsUpdateDevelopment.capacity() *
             sizeof(EventPayload<EventType::UpdateDevelopment>) +
         recordsUpdateCarryingCapacity.capacity() *
             sizeof(EventPayload<EventType::UpdateCarryingCapacity>) +
         recordsMigratePopulation.capacity() *
             sizeof(EventPayload<EventType::MigratePopulation>) +
         recordsSetRegionalPhase.capacity() *
             sizeof(EventPayload<EventType::SetRegionalPhase>) +
         recordsSetSuperRegion.capacity() *
             sizeof(EventPayload<EventType::SetSuperRegion>) +
         recordsCreateSuperRegion.capacity() *
             sizeof(EventPayload<EventType::CreateSuperRegion>) +
         recordsSetSuperRegionPhase.capacity() *
             sizeof(EventPayload<EventType::SetSuperRegionPhase>) +
         recordsMigrateCulturePopulation.capacity() *
             sizeof(EventPayload<EventType::MigrateCulturePopulation>) +
         recordsConvertCulturePopulation.capacity() *
             sizeof(EventPayload<EventType::ConvertCulturePopulation>) +
         recordsColonizeProvince.capacity() *
             sizeof(EventPayload<EventType::ColonizeProvince>) +
         recordsConsolidateRegion.capacity() *
             sizeof(EventPayload<EventType::ConsolidateRegion>) +
         recordsSetCapital.capacity() *
             sizeof(EventPayload<EventType::SetCapital>) +
         recordsPolityUpdate.capacity() *
             sizeof(EventPayload<EventType::PolityUpdate>);
}

size_t EventStore::dynamicBytesEstimate() const {
  size_t bytes = descriptionIds.bucket_count() * sizeof(void *);
  for (const auto &text : descriptions)
    bytes += text.capacity() + 1;
  for (const auto &[text, id] : descriptionIds)
    bytes += sizeof(std::pair<const std::string, uint32_t>) + sizeof(void *) +
             text.capacity() + 1;
  // Approximate tree node size including the sentinel; allocator overhead
  // varies.
  for (const auto &event : recordsCreatePolity) {
    bytes +=
        event.politicalTraits.capacity() * sizeof(PoliticalTraitState) +
        event.politicalPressures.capacity() * sizeof(PoliticalPressureState) +
        (event.politicalMovements.size() + 1) *
            (sizeof(std::pair<const int, double>) + 4 * sizeof(void *));
  }
  for (const auto &event : recordsCreateSuccessorPolity) {
    bytes +=
        event.politicalTraits.capacity() * sizeof(PoliticalTraitState) +
        event.politicalPressures.capacity() * sizeof(PoliticalPressureState) +
        (event.politicalMovements.size() + 1) *
            (sizeof(std::pair<const int, double>) + 4 * sizeof(void *));
  }
  for (const auto &event : recordsPolityUpdate) {
    bytes +=
        event.politicalTraits.capacity() * sizeof(PoliticalTraitState) +
        event.politicalPressures.capacity() * sizeof(PoliticalPressureState) +
        (event.politicalMovements.size() + 1) *
            (sizeof(std::pair<const int, double>) + 4 * sizeof(void *));
  }
  return bytes;
}
} // namespace Arda::Simulation
