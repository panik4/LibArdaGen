#include "simulation/SimulationArtifacts.h"

#include <fstream>

namespace Arda::Simulation::artifacts {

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
  }
  return "Unknown";
}

bool writeEventLog(const std::filesystem::path &path,
                   const std::vector<Event> &events, Year errorYear,
                   std::vector<ValidationError> &errors) {
  std::ofstream eventLog(path);
  if (!eventLog) {
    errors.push_back(
        {errorYear, "Unable to open simulation event log", {}, {}});
    return false;
  }
  eventLog << "year\ttype\tprovince\tregion\tpolity\tprevious_"
              "polity\tculture\treligion\tparent\tvalue\tsecondary_"
              "value\tscore\tred\tgreen\tblue\tdescription\n";
  for (const auto &event : events) {
    eventLog << event.year << '\t' << eventTypeName(event.type) << '\t'
             << event.provinceId << '\t' << event.regionId << '\t'
             << event.polityId << '\t' << event.previousPolityId << '\t'
             << event.cultureId << '\t' << event.religionId << '\t'
             << event.parentId << '\t' << event.value << '\t'
             << event.secondaryValue << '\t' << event.score << '\t'
             << static_cast<int>(event.colour.getRed()) << '\t'
             << static_cast<int>(event.colour.getGreen()) << '\t'
             << static_cast<int>(event.colour.getBlue()) << '\t'
             << event.description << '\n';
  }
  return true;
}

} // namespace Arda::Simulation::artifacts
