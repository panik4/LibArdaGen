#pragma once

#include "simulation/Simulation.h"

namespace Arda::Simulation::artifacts {

const char *eventTypeName(EventType type);

bool writeEventLog(const std::filesystem::path &path,
				   const std::vector<Event> &events, Year errorYear,
				   std::vector<ValidationError> &errors);

} // namespace Arda::Simulation::artifacts
