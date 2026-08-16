#include "simulation/Simulation.h"
#include "simulation/SimulationArtifacts.h"
#include "simulation/SimulationCulture.h"
#include "simulation/SimulationDevelopment.h"
#include "simulation/SimulationEvents.h"
#include "simulation/SimulationExpansion.h"
#include "simulation/SimulationGeography.h"
#include "simulation/SimulationInternal.h"
#include "simulation/SimulationPolity.h"
#include "simulation/SimulationState.h"
#include "simulation/SimulationValidation.h"
#include "simulation/SimulationWarfare.h"

#include "RandNum.h"
#include "rendering/Png.h"
#include "utils/Cfg.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
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

bool chance(double probability) {
  return probability > 0.0 && RandNum::getRandom<double>(1.0) < probability;
}

PolityId strongestPolityOf(const State &state) {
  const auto strongest = std::max_element(
      state.polityStrengths.begin(), state.polityStrengths.end(),
      [](const auto &left, const auto &right) {
        return left.second.score < right.second.score;
      });
  return strongest == state.polityStrengths.end() ? NoPolity : strongest->first;
}

Fwg::Gfx::Colour randomPolityColour() {
  return {static_cast<unsigned char>(RandNum::getRandom<int>(256)),
          static_cast<unsigned char>(RandNum::getRandom<int>(256)),
          static_cast<unsigned char>(RandNum::getRandom<int>(256))};
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
  Result result;
  if (configuration.targetYear <= configuration.startYear) {
    result.errors.push_back({configuration.startYear,
                             "The target year must be after the start year",
                             {},
                             {}});
    return result;
  }
  auto normalized = normalize(input);
  result.errors = normalized.errors;
  if (!result.errors.empty())
    return result;

  provinceRegions = normalized.provinceRegions;
  provinceNeighbours = normalized.neighbours;
  if (normalized.provinces.empty()) {
    result.errors.push_back({configuration.startYear,
                             "No eligible land provinces were supplied",
                             {},
                             {}});
    return result;
  }
  if (configuration.targetEndPolityCount < 1 ||
      configuration.targetEndPolityCount >
          static_cast<int>(normalized.provinces.size())) {
    result.errors.push_back({configuration.startYear,
                             "The target end polity count must be between one "
                             "and the number of eligible land provinces",
                             {},
                             {}});
    return result;
  }

  State state;
  state.year = configuration.startYear;
  PolityId nextPolity = 0;
  CultureId nextCulture = 0;
  ReligionId nextReligion = 0;
  int nextWarId = 0;
  std::map<ProvinceId, double> growthPotential;
  std::map<ProvinceId, double> baseCapacity;
  auto append = [&](Event &&event) {
    result.events.emplace_back(std::move(event));
    if (result.events.back().type == EventType::InitializeProvince) {
      const auto provinceId = result.events.back().provinceId;
      result.events.back().coastal =
          normalized.environments.at(provinceId).coastal;
      result.events.back().island =
          normalized.environments.at(provinceId).island;
    }
    ++result.eventCounts[result.events.back().type];
    const auto &appliedEvent = result.events.back();
    events::apply(state, appliedEvent);
    if (appliedEvent.type == EventType::InitializeProvince ||
        appliedEvent.type == EventType::TransferProvince ||
        appliedEvent.type == EventType::ColonizeProvince ||
        appliedEvent.type == EventType::ConsolidateRegion) {
      if (appliedEvent.previousPolityId != NoPolity)
        result.polityHistory.currentProvinceIds[appliedEvent.previousPolityId]
            .erase(appliedEvent.provinceId);
      if (appliedEvent.polityId != NoPolity) {
        auto &current =
            result.polityHistory.currentProvinceIds[appliedEvent.polityId];
        current.insert(appliedEvent.provinceId);
        auto &peak =
            result.polityHistory.peakProvinceIds[appliedEvent.polityId];
        if (current.size() > peak.size())
          peak.assign(current.begin(), current.end());
        if (appliedEvent.regionId >= 0)
          result.polityHistory.historicalRegionOwners[appliedEvent.regionId]
              .insert(appliedEvent.polityId);
      }
    }
  };
  const auto totalRegions = std::max<size_t>(1, normalized.regions.size());
  SuperRegionId nextSuperRegion = 0;
  std::map<int, std::vector<RegionId>> continentRegions;
  for (const auto &[regionId, continentId] : normalized.regionContinents)
    if (normalized.regions.contains(regionId))
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
      append({configuration.startYear, EventType::CreateSuperRegion, -1,
              superRegionId, continentId, NoPolity, -1, NoReligion, 0.0, 0.0,
              "fixed development superregion"});
      std::vector<RegionId> frontier{*unassigned.begin()};
      size_t assigned = 0;
      while (!frontier.empty() && assigned < groupSize) {
        const auto regionId = frontier.back();
        frontier.pop_back();
        if (!unassigned.erase(regionId))
          continue;
        append({configuration.startYear, EventType::SetSuperRegion, -1,
                regionId, NoPolity, NoPolity, -1, NoReligion,
                static_cast<double>(superRegionId), 0.0,
                "fixed superregion assignment"});
        state.superRegions[superRegionId].regions.push_back(regionId);
        ++assigned;
        for (const auto neighbourId : normalized.regionNeighbours[regionId])
          if (unassigned.contains(neighbourId))
            frontier.push_back(neighbourId);
      }
    }
  }
  for (const auto &[regionId, neighbours] : normalized.regionNeighbours) {
    const auto source = state.regionSuperRegions.find(regionId);
    if (source == state.regionSuperRegions.end())
      continue;
    for (const auto neighbourId : neighbours) {
      const auto target = state.regionSuperRegions.find(neighbourId);
      if (target != state.regionSuperRegions.end() &&
          target->second != source->second)
        state.superRegions[source->second].neighbours.push_back(target->second);
    }
  }
  for (auto &[id, superRegion] : state.superRegions) {
    std::sort(superRegion.neighbours.begin(), superRegion.neighbours.end());
    superRegion.neighbours.erase(std::unique(superRegion.neighbours.begin(),
                                             superRegion.neighbours.end()),
                                 superRegion.neighbours.end());
  }

  for (const auto &[provinceId, province] : normalized.provinces) {
    const auto polityId = nextPolity++;
    const auto cultureId = nextCulture++;
    const auto habitability =
        province->habitability > 0.0f
            ? std::clamp(static_cast<double>(province->habitability), 0.0, 1.0)
            : 0.5;
    const auto connectivity =
        std::min(normalized.neighbours.at(provinceId).size(), size_t{6});
    const auto potential = 0.65 + habitability * 0.45 +
                           static_cast<double>(connectivity) * 0.04 +
                           RandNum::getRandom<double>(0.25);
    growthPotential.emplace(provinceId, potential);
    const auto &environment = normalized.environments.at(provinceId);
    const double environmentalCapacity =
        std::max(configuration.minimumPopulation,
                 configuration.defaultPopulation *
                     (0.5 + environment.habitability + environment.arableLand +
                      (environment.coastal ? 0.25 : 0.0)) *
                     std::sqrt(std::max(
                         1.0, static_cast<double>(environment.area) / 100.0)) /
                     (1.0 + environment.inclination));
    baseCapacity.emplace(provinceId, environmentalCapacity);
    const double population = 1.0;
    const double development = (province->averageDevelopment > 0.0
                                    ? province->averageDevelopment
                                    : configuration.defaultDevelopment) *
                               potential;
    append({configuration.startYear, EventType::CreatePolity, -1, -1, polityId,
            NoPolity, -1, NoReligion, 0.0, 0.0, "tribe", -1,
            randomPolityColour()});
    auto initializeProvince = Event{configuration.startYear,
                                    EventType::InitializeProvince,
                                    provinceId,
                                    normalized.provinceRegions.at(provinceId),
                                    polityId,
                                    NoPolity,
                                    cultureId,
                                    NoReligion,
                                    population,
                                    development,
                                    "initial tribe and culture",
                                    -1,
                                    randomPolityColour()};
    initializeProvince.coastal = environment.coastal;
    initializeProvince.island = environment.island;
    append(std::move(initializeProvince));
    append({configuration.startYear, EventType::UpdateCarryingCapacity,
            provinceId, normalized.provinceRegions.at(provinceId), NoPolity,
            NoPolity, -1, NoReligion, environmentalCapacity, 0.0,
            "initial carrying capacity"});
  }
  for (const auto &[regionId, provinces] : normalized.regions)
    append({configuration.startYear, EventType::SetRegionalPhase, -1, regionId,
            NoPolity, NoPolity, -1, NoReligion,
            static_cast<double>(static_cast<int>(RegionalPhase::Neutral)), 0.0,
            "initial regional phase"});

  for (Year year = configuration.startYear; year < configuration.targetYear;) {
    const Year nextYear =
        std::min(year + stepFor(configuration, year), configuration.targetYear);
    logYearProgress(year, configuration.targetYear, result.eventCounts);
    const double centuries = static_cast<double>(nextYear - year) / 100.0;
    if (!(configuration.superRegionCycleYears <= 0 ||
          (nextYear - configuration.startYear) /
                  configuration.superRegionCycleYears <=
              (year - configuration.startYear) /
                  configuration.superRegionCycleYears)) {
      development::updateSuperRegionPhases(year, nextYear, configuration, state,
                                           append);
    }
    if (!(configuration.regionPhaseDurationYears <= 0 ||
          (nextYear - configuration.startYear) /
                  configuration.regionPhaseDurationYears <=
              (year - configuration.startYear) /
                  configuration.regionPhaseDurationYears)) {
      development::updateRegionalPhases(year, nextYear, configuration,
                                        normalized, append);
    }
    auto territories = territoriesByPolity(state);
    development::updateProvinceGrowth(nextYear, centuries, configuration,
                                      normalized, state, growthPotential,
                                      baseCapacity, append, territories);
    development::updatePolityStrengths(nextYear, state, append);
    culture::integrate(nextYear, centuries, configuration, state, append);
    /*expansion::colonize(nextYear, centuries, configuration, normalized, state,
                          append, strongestPolityOf(state));*/
    warfare::resolveWars(nextYear, centuries, configuration, normalized, state,
                         append, result.wars, nextWarId, result.events,
                         normalized.provinces);
    // implodePolities(nextYear, centuries, configuration, normalized, state,
    // append,
    //                 nextPolity);
    culture::evolveAndReligions(nextYear, centuries, configuration, normalized,
                                state, append, nextCulture, nextReligion);
    if (nextYear >= configuration.regionOwnershipYear) {
      polity::consolidateRegions(nextYear, configuration, normalized, state,
                                 append);
    }
    polity::dissolveEmpty(nextYear, state, append);
    year = nextYear;
  }

  result.finalState = state;
  result.errors = validate(state, configuration.targetYear, true);
  State eventState;
  Year previousYear = configuration.startYear;
  for (const auto &event : result.events) {
    if (event.year < configuration.startYear ||
        event.year > configuration.targetYear)
      result.errors.push_back(
          {event.year, "Event year is outside the configured simulation range",
           event.provinceId >= 0 ? std::optional<ProvinceId>(event.provinceId)
                                 : std::nullopt,
           event.regionId >= 0 ? std::optional<RegionId>(event.regionId)
                               : std::nullopt});
    if (event.year < previousYear)
      result.errors.push_back(
          {event.year, "Events are not ordered chronologically",
           event.provinceId >= 0 ? std::optional<ProvinceId>(event.provinceId)
                                 : std::nullopt,
           event.regionId >= 0 ? std::optional<RegionId>(event.regionId)
                               : std::nullopt});
    previousYear = event.year;
    events::apply(eventState, event);
  }
  return result;
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
  // if (!result.succeeded()) {
  // errors.push_back({configuration.targetYear, "Cannot export artifacts from a
  // failed simulation", {}, {}}); return std::nullopt;
  // }
  if (!baseMap.initialised()) {
    errors.push_back(
        {configuration.startYear,
         "Cannot export ownership maps without an initialized world map",
         {},
         {}});
    return std::nullopt;
  }

  const auto normalized = normalize(input);
  if (!normalized.errors.empty()) {
    errors.insert(errors.end(), normalized.errors.begin(),
                  normalized.errors.end());
    return std::nullopt;
  }

  try {
    std::filesystem::create_directories(outputDirectory);
    const auto developmentDirectory = outputDirectory / "development";
    const auto populationDirectory = outputDirectory / "population";
    const auto cultureDirectory = outputDirectory / "culture";
    const auto religionDirectory = outputDirectory / "religion";
    std::filesystem::create_directories(developmentDirectory);
    std::filesystem::create_directories(populationDirectory);
    std::filesystem::create_directories(cultureDirectory);
    std::filesystem::create_directories(religionDirectory);
    ArtifactPaths paths;
    paths.eventLog = outputDirectory / "events.txt";
    std::ofstream eventLog(paths.eventLog);
    if (!eventLog) {
      errors.push_back({configuration.startYear,
                        "Unable to open simulation event log",
                        {},
                        {}});
      return std::nullopt;
    }
    // Keep TSV schema stable: successor lineage is encoded via "type" and
    // "parent" without adding new columns.
    eventLog << "year\ttype\tprovince\tregion\tpolity\tprevious_"
                "polity\tculture\treligion\tparent\tvalue\tsecondary_"
                "value\tscore\tred\tgreen\tblue\tdescription\n";
    for (const auto &event : result.events) {
      eventLog << event.year << '\t' << artifacts::eventTypeName(event.type)
               << '\t' << event.provinceId << '\t' << event.regionId << '\t'
               << event.polityId << '\t' << event.previousPolityId << '\t'
               << event.cultureId << '\t' << event.religionId << '\t'
               << event.parentId << '\t' << event.value << '\t'
               << event.secondaryValue << '\t' << event.score << '\t'
               << static_cast<int>(event.colour.getRed()) << '\t'
               << static_cast<int>(event.colour.getGreen()) << '\t'
               << static_cast<int>(event.colour.getBlue()) << '\t'
               << event.description << '\n';
    }
    paths.developmentLog = developmentDirectory / "development.tsv";
    paths.populationLog = populationDirectory / "population.tsv";
    paths.cultureLog = cultureDirectory / "culture.tsv";
    paths.religionLog = religionDirectory / "religion.tsv";
    paths.superRegionLog = outputDirectory / "superregions.tsv";
    std::ofstream developmentLog(paths.developmentLog);
    std::ofstream populationLog(paths.populationLog);
    std::ofstream cultureLog(paths.cultureLog);
    std::ofstream religionLog(paths.religionLog);
    std::ofstream superRegionLog(paths.superRegionLog);
    if (!developmentLog || !populationLog || !cultureLog || !religionLog ||
        !superRegionLog) {
      errors.push_back({configuration.startYear,
                        "Unable to open one or more category simulation logs",
                        {},
                        {}});
      return std::nullopt;
    }
    developmentLog << "year\tprovince\tdevelopment\n";
    populationLog
        << "year\tprovince\tpopulation\tcarrying_capacity\tregion_phase\n";
    cultureLog
        << "year\tprovince\tculture\tculture_population\tdominant\torigin_"
           "province\tfounded_year\tparent_culture\n";
    religionLog << "year\tprovince\treligion\torigin_province\tfounded_"
                   "year\tparent_religion\n";
    superRegionLog << "year\tsuperregion\tcontinent\tphase\tdevelopment\n";

    std::vector<Year> years;
    for (Year year = configuration.startYear; year <= configuration.targetYear;
         year += 100)
      years.push_back(year);
    if (years.empty() || years.back() != configuration.targetYear)
      years.push_back(configuration.targetYear);

    for (size_t index = 0; index < years.size(); ++index) {
      const auto year = years[index];
      const auto state = reconstruct(result.events, year);
      for (const auto &[provinceId, provinceState] : state.provinces) {
        // developmentLog << year << '\t' << provinceId << '\t'
        //                << provinceState.development << '\n';
        // const auto regionId = normalized.provinceRegions.at(provinceId);
        // populationLog << year << '\t' << provinceId << '\t'
        //               << provinceState.population << '\t'
        //               << provinceState.carryingCapacity << '\t'
        //               << static_cast<int>(
        //                      state.regionalPhases.contains(regionId)
        //                          ? state.regionalPhases.at(regionId)
        //                          : RegionalPhase::Neutral)
        //               << '\n';
        // for (const auto &[cultureId, culturePopulation] :
        //      provinceState.culturePopulations) {
        //   const auto &culture = state.cultures.at(cultureId);
        //   cultureLog << year << '\t' << provinceId << '\t' << cultureId <<
        //   '\t'
        //              << culturePopulation << '\t'
        //              << (cultureId == state.dominantCultureOf(provinceId) ? 1
        //                                                                   :
        //                                                                   0)
        //              << '\t' << culture.originProvinceId << '\t'
        //              << culture.foundedYear << '\t'
        //              << culture.parentId.value_or(-1) << '\n';
        // }
        // if (provinceState.religion != NoReligion) {
        //   const auto &religion = state.religions.at(provinceState.religion);
        //   religionLog << year << '\t' << provinceId << '\t'
        //               << provinceState.religion << '\t'
        //               << religion.originProvinceId << '\t'
        //               << religion.foundedYear << '\t'
        //               << religion.parentId.value_or(-1) << '\n';
        // } else {
        //   religionLog << year << '\t' << provinceId << "\t-1\t-1\t-1\t-1\n";
        // }
        // for (const auto &[superRegionId, superRegion] : state.superRegions)
        //   superRegionLog << year << '\t' << superRegionId << '\t'
        //                  << superRegion.continentId << '\t'
        //                  << static_cast<int>(superRegion.phase) << '\t'
        //                  << superRegion.development << '\n';
      }
      auto frame = baseMap;
      auto developmentFrame = baseMap;
      auto populationFrame = baseMap;
      auto cultureFrame = baseMap;
      auto religionFrame = baseMap;
      double maximumDevelopment = 0.0;
      double maximumPopulation = 0.0;
      for (const auto &[provinceId, provinceState] : state.provinces) {
        maximumDevelopment =
            std::max(maximumDevelopment, provinceState.development);
        maximumPopulation =
            std::max(maximumPopulation, provinceState.population);
      }
      for (const auto &[provinceId, province] : normalized.provinces) {
        const auto provinceState = state.findProvince(provinceId);
        if (!provinceState)
          continue;
        const auto colour = state.polities.at(provinceState->owner).colour;
        const auto development =
            maximumDevelopment > 0.0
                ? static_cast<unsigned char>(
                      255.0 * provinceState->development / maximumDevelopment)
                : static_cast<unsigned char>(0);
        const auto population =
            maximumPopulation > 0.0
                ? static_cast<unsigned char>(
                      255.0 * std::log1p(provinceState->population) /
                      std::log1p(maximumPopulation))
                : static_cast<unsigned char>(0);
        const auto cultureColour =
            state.cultures.at(provinceState->culture).colour;
        const auto religionColour =
            provinceState->religion == NoReligion
                ? Fwg::Gfx::Colour{0, 0, 0}
                : state.religions.at(provinceState->religion).colour;
        for (const auto pixel : province->pixels) {
          if (pixel >= 0 && static_cast<std::size_t>(pixel) < frame.size())
            frame.setColourAtIndex(pixel, colour);
          developmentFrame.setColourAtIndex(
              pixel, {development, development, development});
          populationFrame.setColourAtIndex(
              pixel, {population, population, population});
          cultureFrame.setColourAtIndex(pixel, cultureColour);
          religionFrame.setColourAtIndex(pixel, religionColour);
        }
      }
      std::ostringstream filename;
      filename << "ownership_" << std::setw(3) << std::setfill('0') << index
               << '_' << year << ".png";
      const auto mapPath = outputDirectory / filename.str();
      Fwg::Gfx::Png::save(frame, mapPath.string(), true);
      paths.ownershipMaps.push_back(mapPath);
      const auto developmentPath = developmentDirectory / filename.str();
      const auto populationPath = populationDirectory / filename.str();
      const auto culturePath = cultureDirectory / filename.str();
      const auto religionPath = religionDirectory / filename.str();
      Fwg::Gfx::Png::save(developmentFrame, developmentPath.string(), true);
      Fwg::Gfx::Png::save(populationFrame, populationPath.string(), true);
      // Fwg::Gfx::Png::save(cultureFrame, culturePath.string(), true);
      // Fwg::Gfx::Png::save(religionFrame, religionPath.string(), true);
      paths.developmentMaps.push_back(developmentPath);
      paths.populationMaps.push_back(populationPath);
      paths.cultureMaps.push_back(culturePath);
      paths.religionMaps.push_back(religionPath);
    }
    return paths;
  } catch (const std::exception &exception) {
    errors.push_back({configuration.startYear,
                      std::string("Unable to export simulation artifacts: ") +
                          exception.what(),
                      {},
                      {}});
    return std::nullopt;
  }
}

} // namespace Arda::Simulation