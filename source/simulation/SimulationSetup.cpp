#include "simulation/SimulationSetup.h"

#include "simulation/SimulationGeography.h"
#include "simulation/SimulationRandom.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace Arda::Simulation::setup {

bool prepareInput(
    const Configuration &configuration, const Input &input,
    std::map<ProvinceId, RegionId> &provinceRegions,
    std::map<ProvinceId, std::vector<ProvinceId>> &provinceNeighbours,
    detail::SimulationRun &run) {
  if (configuration.targetYear <= configuration.startYear) {
    run.result.errors.push_back({configuration.startYear,
                                 "The target year must be after the start year",
                                 {},
                                 {}});
    return false;
  }

  run.normalized = geography::normalize(input);
  run.result.errors = run.normalized.errors;
  if (!run.result.errors.empty())
    return false;

  provinceRegions = run.normalized.provinceRegions;
  provinceNeighbours = run.normalized.neighbours;
  if (run.normalized.provinces.empty()) {
    run.result.errors.push_back({configuration.startYear,
                                 "No eligible land provinces were supplied",
                                 {},
                                 {}});
    return false;
  }
  if (configuration.targetEndPolityCount < 1 ||
      configuration.targetEndPolityCount >
          static_cast<int>(run.normalized.provinces.size())) {
    run.result.errors.push_back(
        {configuration.startYear,
         "The target end polity count must be between one and the number of "
         "eligible land provinces",
         {},
         {}});
    return false;
  }
  return true;
}

void initializeWorld(const Configuration &configuration,
                     detail::SimulationRun &run) {
  run.state.geography = run.normalized.provinces;
  const auto totalRegions = std::max<size_t>(1, run.normalized.regions.size());
  SuperRegionId nextSuperRegion = 0;
  std::map<int, std::vector<RegionId>> continentRegions;
  for (const auto &[regionId, continentId] : run.normalized.regionContinents)
    if (run.normalized.regions.contains(regionId))
      continentRegions[continentId].push_back(regionId);
  for (auto &[continentId, regionIds] : continentRegions) {
    std::set<RegionId> unassigned(regionIds.begin(), regionIds.end());
    const auto groupCount = std::max<size_t>(
        1, std::min(regionIds.size(),
                    static_cast<size_t>(std::round(
                        static_cast<double>(
                            configuration.targetDevelopmentSuperRegionCount) *
                        regionIds.size() / totalRegions))));
    const auto groupSize =
        std::max<size_t>(1, (regionIds.size() + groupCount - 1) / groupCount);
    while (!unassigned.empty()) {
      const auto superRegionId = nextSuperRegion++;
      run.append({configuration.startYear, EventType::CreateSuperRegion, -1,
                  superRegionId, continentId, NoPolity, -1, NoReligion, 0.0,
                  0.0, "fixed development superregion"});
      std::vector<RegionId> frontier{*unassigned.begin()};
      size_t assigned = 0;
      while (!frontier.empty() && assigned < groupSize) {
        const auto regionId = frontier.back();
        frontier.pop_back();
        if (!unassigned.erase(regionId))
          continue;
        run.append({configuration.startYear, EventType::SetSuperRegion, -1,
                    regionId, NoPolity, NoPolity, -1, NoReligion,
                    static_cast<double>(superRegionId), 0.0,
                    "fixed superregion assignment"});
        run.state.superRegions[superRegionId].regions.push_back(regionId);
        ++assigned;
        for (const auto neighbourId : run.normalized.regionNeighbours[regionId])
          if (unassigned.contains(neighbourId))
            frontier.push_back(neighbourId);
      }
    }
  }
  for (const auto &[regionId, neighbours] : run.normalized.regionNeighbours) {
    const auto source = run.state.regionSuperRegions.find(regionId);
    if (source == run.state.regionSuperRegions.end())
      continue;
    for (const auto neighbourId : neighbours) {
      const auto target = run.state.regionSuperRegions.find(neighbourId);
      if (target != run.state.regionSuperRegions.end() &&
          target->second != source->second)
        run.state.superRegions[source->second].neighbours.push_back(
            target->second);
    }
  }
  for (auto &[id, superRegion] : run.state.superRegions) {
    std::sort(superRegion.neighbours.begin(), superRegion.neighbours.end());
    superRegion.neighbours.erase(std::unique(superRegion.neighbours.begin(),
                                             superRegion.neighbours.end()),
                                 superRegion.neighbours.end());
  }

  for (const auto &[provinceId, province] : run.normalized.provinces) {
    const auto polityId = run.nextPolity++;
    const auto cultureId = run.nextCulture++;
    const auto habitability =
        province->habitability > 0.0f
            ? std::clamp(static_cast<double>(province->habitability), 0.0, 1.0)
            : 0.5;
    const auto connectivity =
        std::min(run.normalized.neighbours.at(provinceId).size(), size_t{6});
    const auto potential = 0.65 + habitability * 0.45 +
                           static_cast<double>(connectivity) * 0.04 +
                           RandNum::getRandom<double>(0.25);
    run.growthPotential.emplace(provinceId, potential);
    const auto &environment = run.normalized.environments.at(provinceId);
    const double environmentalCapacity =
        std::max(configuration.minimumPopulation,
                 configuration.defaultPopulation *
                     (0.5 + environment.habitability + environment.arableLand +
                      (environment.coastal ? 0.25 : 0.0)) *
                     std::sqrt(std::max(
                         1.0, static_cast<double>(environment.area) / 100.0)) /
                     (1.0 + environment.inclination));
    run.baseCapacity.emplace(provinceId, environmentalCapacity);
    const double population = 1.0;
    const double development = (province->averageDevelopment > 0.0
                                    ? province->averageDevelopment
                                    : configuration.defaultDevelopment) *
                               potential;
    run.append({configuration.startYear, EventType::CreatePolity, -1, -1,
                polityId, NoPolity, -1, NoReligion, 0.0, 0.0, "tribe", -1,
                random::colour()});
    auto initializeProvince =
        Event{configuration.startYear,
              EventType::InitializeProvince,
              provinceId,
              run.normalized.provinceRegions.at(provinceId),
              polityId,
              NoPolity,
              cultureId,
              NoReligion,
              population,
              development,
              "initial tribe and culture",
              -1,
              random::colour()};
    initializeProvince.coastal = environment.coastal;
    initializeProvince.island = environment.island;
    run.append(std::move(initializeProvince));
    run.append({configuration.startYear, EventType::UpdateCarryingCapacity,
                provinceId, run.normalized.provinceRegions.at(provinceId),
                NoPolity, NoPolity, -1, NoReligion, environmentalCapacity, 0.0,
                "initial carrying capacity"});
  }
  for (const auto &[regionId, provinces] : run.normalized.regions)
    run.append({configuration.startYear, EventType::SetRegionalPhase, -1,
                regionId, NoPolity, NoPolity, -1, NoReligion,
                static_cast<double>(static_cast<int>(RegionalPhase::Neutral)),
                0.0, "initial regional phase"});
}

} // namespace Arda::Simulation::setup
