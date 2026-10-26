#include "simulation/Simulation.h"
#include "simulation/SimulationArtifactWriter.h"
#include "simulation/SimulationArtifacts.h"
// Kept for artifacts::eventTypeName used by yearly progress logging.
#include "simulation/SimulationCulture.h"
#include "simulation/SimulationDevelopment.h"
#include "simulation/SimulationEvents.h"
#include "simulation/SimulationExpansion.h"
#include "simulation/SimulationExecution.h"
#include "simulation/SimulationGeography.h"
#include "simulation/SimulationInternal.h"
#include "simulation/SimulationPolity.h"
#include "simulation/SimulationRun.h"
#include "simulation/SimulationRandom.h"
#include "simulation/SimulationSetup.h"
#include "simulation/SimulationState.h"
#include "simulation/SimulationValidation.h"
#include "simulation/SimulationWarfare.h"

#include "RandNum.h"
#include "utils/Cfg.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <functional>
#include <limits>
#include <numeric>
#include <set>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace Arda::Simulation {
namespace {

using detail::NormalizedInput;
using development::capacityEraMultiplier;
using development::capacityExpansionMultiplier;
using development::phaseMultiplier;
using development::polityCapacityAt;
using events::apply;
using geography::normalize;
using state::refreshDominantCulture;
using state::relocateCapital;
using state::territoriesByPolity;
using warfare::resolveWars;

std::vector<PolityId> contiguousSuccessorAssignments(
    const std::vector<ProvinceId> &provinceIds, size_t successorCount,
    const std::map<ProvinceId, std::vector<ProvinceId>> &neighbours) {
  std::vector<PolityId> assignments(provinceIds.size(), -1);
  std::map<ProvinceId, size_t> indices;
  for (size_t index = 0; index < provinceIds.size(); ++index)
    indices.emplace(provinceIds[index], index);

  std::vector<std::vector<size_t>> frontiers(successorCount);
  std::set<size_t> unassigned;
  for (size_t index = 0; index < provinceIds.size(); ++index)
    unassigned.insert(index);
  for (size_t successor = 0; successor < successorCount; ++successor) {
    const auto seed =
        provinceIds[(successor * provinceIds.size()) / successorCount];
    const auto seedIndex = indices.at(seed);
    assignments[seedIndex] = static_cast<PolityId>(successor);
    unassigned.erase(seedIndex);
    frontiers[successor].push_back(seedIndex);
  }

  while (!unassigned.empty()) {
    bool assigned = false;
    for (size_t successor = 0;
         successor < successorCount && !unassigned.empty(); ++successor) {
      auto &frontier = frontiers[successor];
      for (size_t frontierIndex = 0; frontierIndex < frontier.size();
           ++frontierIndex) {
        const auto provinceId = provinceIds[frontier[frontierIndex]];
        for (const auto neighbourId : neighbours.at(provinceId)) {
          const auto candidate = indices.find(neighbourId);
          if (candidate == indices.end() ||
              !unassigned.contains(candidate->second))
            continue;
          assignments[candidate->second] = static_cast<PolityId>(successor);
          unassigned.erase(candidate->second);
          frontier.push_back(candidate->second);
          assigned = true;
          break;
        }
        if (assigned)
          break;
      }
    }
    if (assigned)
      continue;
    const auto orphan = *unassigned.begin();
    size_t successor = 0;
    size_t smallestTerritory = provinceIds.size();
    for (size_t candidate = 0; candidate < successorCount; ++candidate) {
      const auto territorySize =
          std::count(assignments.begin(), assignments.end(),
                     static_cast<PolityId>(candidate));
      if (territorySize < smallestTerritory) {
        smallestTerritory = territorySize;
        successor = candidate;
      }
    }
    assignments[orphan] = static_cast<PolityId>(successor);
    unassigned.erase(orphan);
    frontiers[successor].push_back(orphan);
  }
  return assignments;
}

PolityId strongestPolityOf(const State &state) {
  const auto strongest = std::max_element(
      state.polityStrengths.begin(), state.polityStrengths.end(),
      [](const auto &left, const auto &right) {
        return left.second.score < right.second.score;
      });
  return strongest == state.polityStrengths.end() ? NoPolity : strongest->first;
}

int stepFor(const Configuration &configuration, Year year) {
  if (year < configuration.classicalStartYear)
    return configuration.ancientStepYears;
  if (year < configuration.medievalStartYear)
    return configuration.classicalStepYears;
  if (year < configuration.modernStartYear)
    return configuration.medievalStepYears;
  return configuration.modernStepYears;
}

using detail::AppendEvent;
using warfare::capitalsShareLandMass;
using warfare::expansionBorderScore;
using warfare::expansionTargetWeakness;
using warfare::hasMaritimeWarConnection;
using warfare::maritimeRangeForYear;
using warfare::polityDistance;
using warfare::remainsContiguousAfterConquest;

void logYearProgress(Year year, Year targetYear,
                     const std::map<EventType, std::size_t> &eventCounts) {
  std::ostringstream eventProgress;
  eventProgress << "Simulating year " << year << " of " << targetYear
                << " | cumulative events: ";
  bool firstEventCount = true;
  for (const auto &[eventType, count] : eventCounts) {
    if (!firstEventCount)
      eventProgress << ", ";
    eventProgress << artifacts::eventTypeName(eventType) << '=' << count;
    firstEventCount = false;
  }
  Fwg::Utils::Logging::logLine(eventProgress.str());
}

} // namespace

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
          std::chrono::steady_clock::now() - inputStart).count(), " ms");
  run.state.year = configuration.startYear;
  const auto worldStart = std::chrono::steady_clock::now();
  setup::initializeWorld(configuration, run);
  Fwg::Utils::Logging::logLine(
      "History setup timing: initialize world total = ",
      std::chrono::duration<double, std::milli>(
          std::chrono::steady_clock::now() - worldStart).count(), " ms");
  execution::simulateYears(configuration, run);
  execution::finalize(configuration, provinceRegions, run);
  return std::move(run.result);
}

State HistorySimulation::reconstruct(const std::vector<Event> &events,
                                     Year year) const {
  State state;
  for (const auto &event : events) {
    if (event.year > year)
      break;
    events::apply(state, event);
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
      [this](const std::vector<Event> &events, Year year) {
        return reconstruct(events, year);
      },
      errors);
}

} // namespace Arda::Simulation
