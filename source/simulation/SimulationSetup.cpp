#include "simulation/SimulationSetup.h"

#include "simulation/SimulationGeography.h"
#include "simulation/SimulationRandom.h"
#include "utils/Cfg.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <numeric>
#include <set>

namespace Arda::Simulation::setup {
namespace {

// Counts eligible sources in province-ID order, preserving random rank selection
// without rebuilding or walking the entire expansion frontier.
class ExpansionFrontier {
public:
  explicit ExpansionFrontier(size_t size) : counts(size + 1) {}

  void add(size_t index, int delta) {
    total += delta;
    for (++index; index < counts.size(); index += index & (~index + 1))
      counts[index] += delta;
  }

  int size() const { return total; }

  size_t select(size_t rank) const {
    size_t index = 0;
    for (size_t step = std::bit_floor(counts.size() - 1); step; step >>= 1) {
      const auto next = index + step;
      if (next < counts.size() && static_cast<size_t>(counts[next]) <= rank) {
        index = next;
        rank -= static_cast<size_t>(counts[next]);
      }
    }
    return index;
  }

private:
  std::vector<int> counts;
  int total = 0;
};

PoliticalPowerDistribution initialPoliticalPower() {
  PoliticalPowerDistribution power{};
  double total = 0.0;
  for (auto &value : power) {
    value = 0.5 + RandNum::getRandom<double>(1.0);
    total += value;
  }
  for (auto &value : power)
    value /= total;
  return power;
}

} // namespace

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
  const auto superRegionStart = std::chrono::steady_clock::now();
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

  Fwg::Utils::Logging::logLine(
      "History setup timing: superregion initialization = ",
      std::chrono::duration<double, std::milli>(
          std::chrono::steady_clock::now() - superRegionStart).count(), " ms");
  const auto territoryStart = std::chrono::steady_clock::now();
  std::vector<ProvinceId> provinceIds;
  provinceIds.reserve(run.normalized.provinces.size());
  for (const auto &[provinceId, province] : run.normalized.provinces)
    provinceIds.push_back(provinceId);
  std::vector<ProvinceId> shuffledProvinceIds = provinceIds;
  for (size_t index = shuffledProvinceIds.size(); index > 1; --index) {
    const auto swapIndex =
        static_cast<size_t>(RandNum::getRandom<int>(static_cast<int>(index)));
    std::swap(shuffledProvinceIds[index - 1], shuffledProvinceIds[swapIndex]);
  }
  const auto seedCount = std::min<size_t>(1000, shuffledProvinceIds.size());
  Fwg::Utils::Logging::logLine(
      "History setup: assigning initial territory (provinces=",
      provinceIds.size(), ", seeds=", seedCount, ")");
  std::map<ProvinceId, PolityId> provinceOwners;
  std::vector<std::vector<ProvinceId>> polityProvinces(seedCount);
  for (size_t index = 0; index < seedCount; ++index) {
    const auto provinceId = shuffledProvinceIds[index];
    const auto polityId = static_cast<PolityId>(index);
    provinceOwners[provinceId] = polityId;
    polityProvinces[index].push_back(provinceId);
  }
  std::set<ProvinceId> unassigned(shuffledProvinceIds.begin() + seedCount,
                                  shuffledProvinceIds.end());
  if (!unassigned.empty()) {
    std::map<ProvinceId, size_t> provinceIndices;
    for (size_t index = 0; index < provinceIds.size(); ++index)
      provinceIndices.emplace(provinceIds[index], index);
    std::vector<size_t> unassignedNeighbourCounts(provinceIds.size());
    std::vector<std::vector<size_t>> incomingNeighbours(provinceIds.size());
    ExpansionFrontier frontier(provinceIds.size());
    for (size_t index = 0; index < provinceIds.size(); ++index) {
      const auto provinceId = provinceIds[index];
      for (const auto neighbourId : run.normalized.neighbours.at(provinceId)) {
        incomingNeighbours[provinceIndices.at(neighbourId)].push_back(index);
        if (unassigned.contains(neighbourId))
          ++unassignedNeighbourCounts[index];
      }
      if (provinceOwners.contains(provinceId) && unassignedNeighbourCounts[index])
        frontier.add(index, 1);
    }
    while (!unassigned.empty() && frontier.size() > 0) {
      const auto source = provinceIds[frontier.select(
          static_cast<size_t>(RandNum::getRandom<int>(frontier.size())))];
      std::vector<ProvinceId> candidates;
      const auto owner = provinceOwners.at(source);
      for (const auto neighbourId : run.normalized.neighbours.at(source))
        if (unassigned.contains(neighbourId))
          candidates.push_back(neighbourId);
      const auto assigned = candidates[static_cast<size_t>(RandNum::getRandom<int>(
          static_cast<int>(candidates.size())))];
      const auto assignedIndex = provinceIndices.at(assigned);
      unassigned.erase(assigned);
      // Reverse edges also handle directed adjacency. Update before marking the
      // new province owned, so a self-edge cannot remove an inactive source.
      for (const auto index : incomingNeighbours[assignedIndex]) {
        if (--unassignedNeighbourCounts[index] == 0 &&
            provinceOwners.contains(provinceIds[index]))
          frontier.add(index, -1);
      }
      provinceOwners[assigned] = owner;
      polityProvinces[static_cast<size_t>(owner)].push_back(assigned);
      if (unassignedNeighbourCounts[assignedIndex])
        frontier.add(assignedIndex, 1);
    }
  }
  for (const auto provinceId : unassigned) {
    const auto polityId = static_cast<PolityId>(polityProvinces.size());
    provinceOwners[provinceId] = polityId;
    polityProvinces.push_back({provinceId});
  }
  run.nextPolity = static_cast<PolityId>(polityProvinces.size());

  Fwg::Utils::Logging::logLine(
      "History setup timing: initial territory allocation = ",
      std::chrono::duration<double, std::milli>(
          std::chrono::steady_clock::now() - territoryStart).count(),
      " ms | polities=", polityProvinces.size());
  const auto polityStart = std::chrono::steady_clock::now();
  for (size_t index = 0; index < polityProvinces.size(); ++index) {
    const auto polityId = static_cast<PolityId>(index);
    const auto provinceId = polityProvinces[index].front();
    const auto &province = run.normalized.provinces.at(provinceId);
    auto createPolity = Event{configuration.startYear,
                              EventType::CreatePolity,
                              -1,
                              -1,
                              polityId,
                              NoPolity,
                              -1,
                              NoReligion,
                              0.0,
                              0.0,
                              "tribe",
                              -1,
                              random::colour()};
    createPolity.governmentForm = GovernmentForm::Tribal;
    createPolity.stability = configuration.initialPolityStability;
    createPolity.lastGovernmentTransitionYear = configuration.startYear;
    createPolity.centralizationAffinity = std::clamp(
        configuration.centralizationAffinityMinimum +
            RandNum::getRandom<double>(1.0) *
                (configuration.centralizationAffinityMaximum -
                 configuration.centralizationAffinityMinimum),
        0.0, 2.0);
    createPolity.politicalPower = initialPoliticalPower();
    createPolity.desiredPoliticalPower = createPolity.politicalPower;
    createPolity.governmentPopularity.fill(0.0);
    createPolity.governmentPopularity[static_cast<std::size_t>(GovernmentForm::Tribal)] = 1.0;
    createPolity.militaryInfluence = RandNum::getRandom<double>(0.25);
    createPolity.desiredMilitaryInfluence = createPolity.militaryInfluence;
    createPolity.partyControl = RandNum::getRandom<double>(0.15);
    createPolity.desiredPartyControl = createPolity.partyControl;
    createPolity.politicalTraits.push_back(
        {PoliticalTrait::DivineRulership, configuration.traitInitialStrength,
         configuration.startYear, std::nullopt});
    createPolity.lastTraitEra = PoliticalEra::AncientClassical;
    run.append(std::move(createPolity));
  }
  Fwg::Utils::Logging::logLine(
      "History setup timing: polity initialization = ",
      std::chrono::duration<double, std::milli>(
          std::chrono::steady_clock::now() - polityStart).count(), " ms");
  const auto provinceStart = std::chrono::steady_clock::now();
  for (const auto &[provinceId, province] : run.normalized.provinces) {
    const auto polityId = provinceOwners.at(provinceId);
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
    initializeProvince.landMassID = province->landMassID;
    initializeProvince.continentId = province->continent->ID;
    run.append(std::move(initializeProvince));
    run.append({configuration.startYear, EventType::UpdateCarryingCapacity,
                provinceId, run.normalized.provinceRegions.at(provinceId),
                NoPolity, NoPolity, -1, NoReligion, environmentalCapacity, 0.0,
                "initial carrying capacity"});
  }
  Fwg::Utils::Logging::logLine(
      "History setup timing: province initialization = ",
      std::chrono::duration<double, std::milli>(
          std::chrono::steady_clock::now() - provinceStart).count(), " ms");
  const auto regionalPhaseStart = std::chrono::steady_clock::now();
  for (const auto &[regionId, provinces] : run.normalized.regions)
    run.append({configuration.startYear, EventType::SetRegionalPhase, -1,
                regionId, NoPolity, NoPolity, -1, NoReligion,
                static_cast<double>(static_cast<int>(RegionalPhase::Neutral)),
                0.0, "initial regional phase"});
  Fwg::Utils::Logging::logLine(
      "History setup timing: regional phase initialization = ",
      std::chrono::duration<double, std::milli>(
          std::chrono::steady_clock::now() - regionalPhaseStart).count(), " ms");
}

} // namespace Arda::Simulation::setup
