#pragma once

#include "simulation/Simulation.h"

#include <functional>

namespace Arda::Simulation::artifacts {

using StateReconstructor = std::function<State(const std::vector<Event> &, Year)>;

bool writePolityStateCsv(const Result &result,
						 const std::filesystem::path &path, Year errorYear,
						 std::vector<ValidationError> &errors);

std::optional<ArtifactPaths>
writeSimulationArtifacts(const Configuration &configuration,
					   const Input &input, const Result &result,
					   const Fwg::Gfx::Image &baseMap,
					   const std::filesystem::path &outputDirectory,
					   const StateReconstructor &reconstruct,
					   std::vector<ValidationError> &errors);

} // namespace Arda::Simulation::artifacts
