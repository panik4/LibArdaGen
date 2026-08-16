#pragma once

#include "simulation/SimulationInternal.h"

namespace Arda::Simulation::detail {

struct SimulationRun {
  SimulationRun();

  State state;
  Result result;
  PolityId nextPolity = 0;
  CultureId nextCulture = 0;
  ReligionId nextReligion = 0;
  int nextWarId = 0;
  std::map<ProvinceId, double> growthPotential;
  std::map<ProvinceId, double> baseCapacity;
  NormalizedInput normalized;

  AppendEvent append;

private:
  void appendEvent(Event event);
};

} // namespace Arda::Simulation::detail
