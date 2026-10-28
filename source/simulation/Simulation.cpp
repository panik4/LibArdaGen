#include "simulation/Simulation.h"
#include "simulation/SimulationArtifactWriter.h"
#include "simulation/SimulationEvents.h"
#include "simulation/SimulationExecution.h"
#include "simulation/SimulationRun.h"
#include "simulation/SimulationSetup.h"
#include "simulation/SimulationValidation.h"
#include "utils/Cfg.h"
#include <chrono>
#include <utility>

namespace Arda::Simulation {

HistorySimulation::HistorySimulation(Configuration configuration)
    : configuration(std::move(configuration)) {}

Result HistorySimulation::runSimulation(const Input &input) {
  detail::SimulationRun run;
  Fwg::Utils::Logging::logLine("History setup: preparing simulation input");
  const auto inputStart = std::chrono::steady_clock::now();
  if (!setup::prepareInput(configuration, input, provinceRegions,
                           provinceNeighbours, run))
    return std::move(run.result);
  Fwg::Utils::Logging::logLine(
      "History setup timing: prepare input total = ",
      std::chrono::duration<double, std::milli>(
          std::chrono::steady_clock::now() - inputStart)
          .count(),
      " ms");
  run.state.year = configuration.startYear;
  const auto worldStart = std::chrono::steady_clock::now();
  setup::initializeWorld(configuration, run);
  Fwg::Utils::Logging::logLine(
      "History setup timing: initialize world total = ",
      std::chrono::duration<double, std::milli>(
          std::chrono::steady_clock::now() - worldStart)
          .count(),
      " ms");
  execution::simulateYears(configuration, run);
  execution::finalize(configuration, provinceRegions, run);
  return std::move(run.result);
}

State HistorySimulation::reconstruct(const EventStore &events,
                                     Year year) const {
  State state;
  for (size_t index = 0; index < events.size(); ++index) {
    const auto &event = events.header(index);
    if (event.year > year)
      break;
    events::apply(state, events, index);
    state.year = event.year;
  }
  return state;
}

std::vector<ValidationError>
HistorySimulation::validate(const State &state, Year year,
                            bool requireWholeRegions) const {
  return validation::validateState(state, configuration, provinceRegions, year,
                                   requireWholeRegions);
}

std::optional<ArtifactPaths>
HistorySimulation::writeArtifacts(const Input &input, const Result &result,
                                  const Fwg::Gfx::Image &baseMap,
                                  const std::filesystem::path &outputDirectory,
                                  std::vector<ValidationError> &errors) const {
  return artifacts::writeSimulationArtifacts(
      configuration, input, result, baseMap, outputDirectory,
      [this](const EventStore &events, Year year) {
        return reconstruct(events, year);
      },
      errors);
}

} // namespace Arda::Simulation
