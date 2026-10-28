#include "simulation/SimulationArtifacts.h"

#include <fstream>

namespace Arda::Simulation::artifacts {

namespace {
void fillLogRow(EventLogRow &row,
                const EventPayload<EventType::InitializeProvince> &payload) {
  row.provinceId = payload.provinceId;
  row.regionId = payload.regionId;
  row.polityId = payload.polityId;
  row.cultureId = payload.cultureId;
  row.religionId = payload.religionId;
  row.value = payload.value;
  row.secondaryValue = payload.secondaryValue;
  row.colour = payload.colour;
}

void fillLogRow(EventLogRow &row,
                const EventPayload<EventType::CreatePolity> &payload) {
  row.polityId = payload.polityId;
  row.parentId = payload.parentId;
  row.colour = payload.colour;
  row.armyTradition = payload.armyTradition;
  row.navyTradition = payload.navyTradition;
  row.stability = payload.stability;
  row.centralization = payload.centralization;
  row.governmentForm = payload.governmentForm;
}

void fillLogRow(EventLogRow &row,
                const EventPayload<EventType::CreateSuccessorPolity> &payload) {
  row.polityId = payload.polityId;
  row.parentId = payload.parentId;
  row.colour = payload.colour;
  row.armyTradition = payload.armyTradition;
  row.navyTradition = payload.navyTradition;
  row.stability = payload.stability;
  row.centralization = payload.centralization;
  row.governmentForm = payload.governmentForm;
}

void fillLogRow(EventLogRow &row,
                const EventPayload<EventType::TransferProvince> &payload) {
  row.provinceId = payload.provinceId;
  row.regionId = payload.regionId;
  row.polityId = payload.polityId;
  row.previousPolityId = payload.previousPolityId;
}

void fillLogRow(EventLogRow &row,
                const EventPayload<EventType::DissolvePolity> &payload) {
  row.polityId = payload.polityId;
}

void fillLogRow(EventLogRow &row,
                const EventPayload<EventType::CreateCulture> &payload) {
  row.provinceId = payload.provinceId;
  row.cultureId = payload.cultureId;
  row.parentId = payload.parentId;
  row.colour = payload.colour;
}

void fillLogRow(EventLogRow &row,
                const EventPayload<EventType::SetCulture> &payload) {
  row.provinceId = payload.provinceId;
  row.cultureId = payload.cultureId;
}

void fillLogRow(EventLogRow &row,
                const EventPayload<EventType::SetPrimaryCulture> &payload) {
  row.polityId = payload.polityId;
  row.cultureId = payload.cultureId;
}

void fillLogRow(EventLogRow &row,
                const EventPayload<EventType::CreateReligion> &payload) {
  row.provinceId = payload.provinceId;
  row.religionId = payload.religionId;
  row.parentId = payload.parentId;
  row.colour = payload.colour;
}

void fillLogRow(EventLogRow &row,
                const EventPayload<EventType::SetReligion> &payload) {
  row.provinceId = payload.provinceId;
  row.religionId = payload.religionId;
}

void fillLogRow(EventLogRow &row,
                const EventPayload<EventType::UpdatePopulation> &payload) {
  row.provinceId = payload.provinceId;
  row.value = payload.value;
}

void fillLogRow(EventLogRow &row,
                const EventPayload<EventType::UpdatePolityStrength> &payload) {
  row.polityId = payload.polityId;
  row.value = payload.value;
  row.secondaryValue = payload.secondaryValue;
  row.score = payload.score;
}

void fillLogRow(EventLogRow &row,
                const EventPayload<EventType::UpdateDevelopment> &payload) {
  row.provinceId = payload.provinceId;
  row.value = payload.value;
}

void fillLogRow(
    EventLogRow &row,
    const EventPayload<EventType::UpdateCarryingCapacity> &payload) {
  row.provinceId = payload.provinceId;
  row.regionId = payload.regionId;
  row.value = payload.value;
  row.secondaryValue = payload.secondaryValue;
}

void fillLogRow(EventLogRow &row,
                const EventPayload<EventType::MigratePopulation> &payload) {
  row.provinceId = payload.provinceId;
  row.regionId = payload.regionId;
  row.value = payload.value;
}

void fillLogRow(EventLogRow &row,
                const EventPayload<EventType::SetRegionalPhase> &payload) {
  row.regionId = payload.regionId;
  row.value = payload.value;
}

void fillLogRow(EventLogRow &row,
                const EventPayload<EventType::SetSuperRegion> &payload) {
  row.regionId = payload.regionId;
  row.value = payload.value;
}

void fillLogRow(EventLogRow &row,
                const EventPayload<EventType::CreateSuperRegion> &payload) {
  row.regionId = payload.regionId;
  row.polityId = payload.polityId;
}

void fillLogRow(EventLogRow &row,
                const EventPayload<EventType::SetSuperRegionPhase> &payload) {
  row.regionId = payload.regionId;
  row.value = payload.value;
}

void fillLogRow(
    EventLogRow &row,
    const EventPayload<EventType::MigrateCulturePopulation> &payload) {
  row.provinceId = payload.provinceId;
  row.regionId = payload.regionId;
  row.cultureId = payload.cultureId;
  row.value = payload.value;
}

void fillLogRow(
    EventLogRow &row,
    const EventPayload<EventType::ConvertCulturePopulation> &payload) {
  row.provinceId = payload.provinceId;
  row.cultureId = payload.cultureId;
  row.parentId = payload.parentId;
  row.value = payload.value;
}

void fillLogRow(EventLogRow &row,
                const EventPayload<EventType::ColonizeProvince> &payload) {
  row.provinceId = payload.provinceId;
  row.regionId = payload.regionId;
  row.polityId = payload.polityId;
  row.previousPolityId = payload.previousPolityId;
}

void fillLogRow(EventLogRow &row,
                const EventPayload<EventType::ConsolidateRegion> &payload) {
  row.provinceId = payload.provinceId;
  row.regionId = payload.regionId;
  row.polityId = payload.polityId;
  row.previousPolityId = payload.previousPolityId;
}

void fillLogRow(EventLogRow &row,
                const EventPayload<EventType::SetCapital> &payload) {
  row.provinceId = payload.provinceId;
  row.polityId = payload.polityId;
}

void fillLogRow(EventLogRow &row,
                const EventPayload<EventType::PolityUpdate> &payload) {
  row.polityId = payload.polityId;
  row.armyTradition = payload.armyTradition;
  row.navyTradition = payload.navyTradition;
  row.stability = payload.stability;
  row.centralization = payload.centralization;
  row.governmentForm = payload.governmentForm;
}
} // namespace

EventLogRow makeEventLogRow(const EventStore &events, size_t index) {
  EventLogRow row;
  events.visit(index, [&](const EventHeader &header, const auto &payload) {
    row.year = header.year;
    row.type = header.type;
    row.description = events.description(header.descriptionId);
    fillLogRow(row, payload);
  });
  return row;
}

const char *eventTypeName(EventType type) {
  switch (type) {
  case EventType::InitializeProvince:
    return "InitializeProvince";
  case EventType::CreatePolity:
    return "CreatePolity";
  case EventType::CreateSuccessorPolity:
    return "CreateSuccessorPolity";
  case EventType::TransferProvince:
    return "TransferProvince";
  case EventType::DissolvePolity:
    return "DissolvePolity";
  case EventType::CreateCulture:
    return "CreateCulture";
  case EventType::SetCulture:
    return "SetCulture";
  case EventType::SetPrimaryCulture:
    return "SetPrimaryCulture";
  case EventType::CreateReligion:
    return "CreateReligion";
  case EventType::SetReligion:
    return "SetReligion";
  case EventType::UpdatePopulation:
    return "UpdatePopulation";
  case EventType::UpdatePolityStrength:
    return "UpdatePolityStrength";
  case EventType::UpdateDevelopment:
    return "UpdateDevelopment";
  case EventType::UpdateCarryingCapacity:
    return "UpdateCarryingCapacity";
  case EventType::MigratePopulation:
    return "MigratePopulation";
  case EventType::SetRegionalPhase:
    return "SetRegionalPhase";
  case EventType::SetSuperRegion:
    return "SetSuperRegion";
  case EventType::CreateSuperRegion:
    return "CreateSuperRegion";
  case EventType::SetSuperRegionPhase:
    return "SetSuperRegionPhase";
  case EventType::MigrateCulturePopulation:
    return "MigrateCulturePopulation";
  case EventType::ConvertCulturePopulation:
    return "ConvertCulturePopulation";
  case EventType::ColonizeProvince:
    return "ColonizeProvince";
  case EventType::ConsolidateRegion:
    return "ConsolidateRegion";
  case EventType::SetCapital:
    return "SetCapital";
  case EventType::PolityUpdate:
    return "PolityUpdate";
  }
  return "Unknown";
}

bool writeEventLog(const std::filesystem::path &path, const EventStore &events,
                   Year errorYear, std::vector<ValidationError> &errors) {
  std::ofstream eventLog(path);
  if (!eventLog) {
    errors.push_back(
        {errorYear, "Unable to open simulation event log", {}, {}});
    return false;
  }
  eventLog << "year\ttype\tprovince\tregion\tpolity\tprevious_"
              "polity\tculture\treligion\tparent\tvalue\tsecondary_"
              "value\tscore\tred\tgreen\tblue\tarmy_tradition\t"
              "navy_tradition\tstability\tcentralization\tgovernment_form\t"
              "description\n";
  for (size_t index = 0; index < events.size(); ++index) {
    const auto event = makeEventLogRow(events, index);
    eventLog << event.year << '\t' << eventTypeName(event.type) << '\t'
             << event.provinceId << '\t' << event.regionId << '\t'
             << event.polityId << '\t' << event.previousPolityId << '\t'
             << event.cultureId << '\t' << event.religionId << '\t'
             << event.parentId << '\t' << event.value << '\t'
             << event.secondaryValue << '\t' << event.score << '\t'
             << static_cast<int>(event.colour.getRed()) << '\t'
             << static_cast<int>(event.colour.getGreen()) << '\t'
             << static_cast<int>(event.colour.getBlue()) << '\t'
             << event.armyTradition << '\t' << event.navyTradition << '\t'
             << event.stability << '\t' << event.centralization << '\t'
             << static_cast<int>(event.governmentForm) << '\t'
             << event.description << '\n';
  }
  return true;
}

} // namespace Arda::Simulation::artifacts
