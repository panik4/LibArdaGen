#include "simulation/SimulationExecution.h"

#include "simulation/SimulationArtifacts.h"
#include "simulation/SimulationCulture.h"
#include "simulation/SimulationDevelopment.h"
#include "simulation/SimulationPolity.h"
#include "simulation/SimulationValidation.h"
#include "simulation/SimulationWarfare.h"

#include "utils/Cfg.h"
#include <algorithm>
#include <chrono>
#include <functional>
#include <map>
#include <span>
#include <sstream>

namespace Arda::Simulation::execution {
namespace {

int stepFor(const Configuration &configuration, Year year) {
  if (year < configuration.classicalStartYear)
    return configuration.ancientStepYears;
  if (year < configuration.medievalStartYear)
    return configuration.classicalStepYears;
  if (year < configuration.modernStartYear)
    return configuration.medievalStepYears;
  return configuration.modernStepYears;
}

void logYearProgress(Year year, Year targetYear,
                     const std::map<EventType, std::size_t> &eventCounts) {
  std::ostringstream progress;
  progress << "Simulating year " << year << " of " << targetYear
           << " | cumulative events: ";
  bool first = true;
  for (const auto &[eventType, count] : eventCounts) {
    if (!first)
      progress << ", ";
    progress << artifacts::eventTypeName(eventType) << '=' << count;
    first = false;
  }
  Fwg::Utils::Logging::logLine(progress.str());
}

} // namespace

void simulateYears(const Configuration &configuration,
                   detail::SimulationRun &run) {
  const auto snapshotStart = std::chrono::steady_clock::now();
  polity::captureTimelineSnapshot(configuration.startYear, run.normalized,
                                  run.state, run.result.polityTimeline);
  Fwg::Utils::Logging::logLine(
      "History setup timing: initial timeline snapshot = ",
      std::chrono::duration<double, std::milli>(
          std::chrono::steady_clock::now() - snapshotStart).count(), " ms");
  for (Year year = configuration.startYear; year < configuration.targetYear;) {
    const Year nextYear =
        std::min(year + stepFor(configuration, year), configuration.targetYear);
    logYearProgress(year, configuration.targetYear, run.result.eventCounts);
    const double centuries = static_cast<double>(nextYear - year) / 100.0;
    if (!(configuration.superRegionCycleYears <= 0 ||
          (nextYear - configuration.startYear) /
                  configuration.superRegionCycleYears <=
              (year - configuration.startYear) /
                  configuration.superRegionCycleYears))
      development::updateSuperRegionPhases(year, nextYear, configuration,
                                           run.state, run.append);
    if (!(configuration.regionPhaseDurationYears <= 0 ||
          (nextYear - configuration.startYear) /
                  configuration.regionPhaseDurationYears <=
              (year - configuration.startYear) /
                  configuration.regionPhaseDurationYears))
      development::updateRegionalPhases(year, nextYear, configuration,
                                        run.normalized, run.append);
    development::updateProvinceGrowth(
        nextYear, centuries, configuration, run.normalized, run.state,
        run.growthPotential, run.baseCapacity, run.append);
    development::updatePolityStrengths(nextYear, run.state, run.append);
    culture::integrate(nextYear, centuries, configuration, run.state,
                       run.append);
    const auto firstCurrentWar = run.result.wars.size();
    warfare::resolveWars(nextYear, centuries, configuration, run.normalized,
                         run.state, run.append, run.result.wars, run.nextWarId,
                         run.result.events);
    polity::updatePoliticalStates(nextYear, centuries, configuration, run.state,
                                  run.append,
                                  std::span<const WarEvent>(run.result.wars)
                                      .subspan(firstCurrentWar),
                                  run.result.governmentTransitions);
    culture::evolveAndReligions(nextYear, centuries, configuration,
                                run.normalized, run.state, run.append,
                                run.nextCulture, run.nextReligion);
    if (nextYear >= configuration.regionOwnershipYear)
      polity::consolidateRegions(nextYear, configuration, run.normalized,
                                 run.state, run.append);
    polity::dissolveEmpty(nextYear, run.state, run.append);
    polity::captureTimelineSnapshot(nextYear, run.normalized, run.state,
                                    run.result.polityTimeline);
    polity::implode(nextYear, centuries, configuration, run.normalized,
                    run.state, run.append, run.nextPolity);
    year = nextYear;
  }
}

void finalize(const Configuration &configuration,
              const std::map<ProvinceId, RegionId> &provinceRegions,
              detail::SimulationRun &run) {
  run.result.finalState = run.state;
  for (const auto &error :
       validation::validateState(run.state, configuration, provinceRegions,
                                 configuration.targetYear, true))
    run.result.errors.push_back(error);
  Year previousYear = configuration.startYear;
  for (const auto &event : run.result.events) {
    if (event.year < configuration.startYear ||
        event.year > configuration.targetYear)
      run.result.errors.push_back(
          {event.year, "Event year is outside the configured simulation range",
           event.provinceId >= 0 ? std::optional<ProvinceId>(event.provinceId)
                                 : std::nullopt,
           event.regionId >= 0 ? std::optional<RegionId>(event.regionId)
                               : std::nullopt});
    if (event.year < previousYear)
      run.result.errors.push_back(
          {event.year, "Events are not ordered chronologically",
           event.provinceId >= 0 ? std::optional<ProvinceId>(event.provinceId)
                                 : std::nullopt,
           event.regionId >= 0 ? std::optional<RegionId>(event.regionId)
                               : std::nullopt});
    previousYear = event.year;
  }
}

} // namespace Arda::Simulation::execution
