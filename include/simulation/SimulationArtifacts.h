#pragma once

#include "simulation/Simulation.h"

namespace Arda::Simulation::artifacts {

// Scalar values are copied; description is valid until the store is mutated.
struct EventLogRow {
  Year year = StartYear;
  EventType type = EventType::InitializeProvince;
  ProvinceId provinceId = -1;
  RegionId regionId = -1;
  PolityId polityId = NoPolity;
  PolityId previousPolityId = NoPolity;
  CultureId cultureId = -1;
  ReligionId religionId = NoReligion;
  double value = 0.0;
  double secondaryValue = 0.0;
  int parentId = -1;
  Fwg::Gfx::Colour colour;
  double score = 0.0;
  double armyTradition = 0.0;
  double navyTradition = 0.0;
  double stability = 0.5;
  double centralization = 0.0;
  GovernmentForm governmentForm = GovernmentForm::Tribal;
  std::string_view description;
};
EventLogRow makeEventLogRow(const EventStore &events, size_t index);

const char *eventTypeName(EventType type);

bool writeEventLog(const std::filesystem::path &path, const EventStore &events,
                   Year errorYear, std::vector<ValidationError> &errors);

} // namespace Arda::Simulation::artifacts
