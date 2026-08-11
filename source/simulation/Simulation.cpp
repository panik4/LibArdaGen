#include "simulation/Simulation.h"

#include "RandNum.h"
#include "rendering/Png.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <set>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace Arda::Simulation {
namespace {

struct NormalizedInput {
  struct Environment {
    double habitability = 0.5;
    double arableLand = 0.5;
    double inclination = 0.0;
    bool coastal = false;
    size_t area = 0;
  };
  std::map<ProvinceId, std::shared_ptr<ArdaProvince>> provinces;
  std::map<ProvinceId, RegionId> provinceRegions;
  std::map<RegionId, std::vector<ProvinceId>> regions;
  std::map<ProvinceId, std::vector<ProvinceId>> neighbours;
  std::map<ProvinceId, int> provinceContinents;
  std::map<RegionId, int> regionContinents;
  std::map<RegionId, std::vector<RegionId>> regionNeighbours;
  std::map<ProvinceId, Environment> environments;
  std::vector<ValidationError> errors;
};

bool isEligible(const std::shared_ptr<ArdaProvince> &province) {
  return province && !province->isSea() && !province->isLake() &&
         !province->topographyTypes.contains(
             Civilization::TopographyType::WASTELAND);
}

void refreshDominantCulture(ProvinceState &province) {
  if (province.culturePopulations.empty()) {
    province.culture = -1;
    return;
  }
  province.culture = std::max_element(province.culturePopulations.begin(),
                                      province.culturePopulations.end(),
                                      [](const auto &left, const auto &right) {
                                        return left.second < right.second;
                                      })
                         ->first;
}

double capacityEraMultiplier(const Configuration &configuration, Year year) {
  const auto compound = [](double multiplier, double rate, Year years) {
    return multiplier *
           std::pow(1.0 + rate,
                    static_cast<double>(std::max(0, years)) / 100.0);
  };
  double multiplier =
      compound(1.0, configuration.ancientCapacityGrowthPerCentury,
               std::min(year, configuration.renaissanceStartYear) -
                   configuration.startYear);
  if (year > configuration.renaissanceStartYear) {
    multiplier =
        compound(multiplier, configuration.renaissanceCapacityGrowthPerCentury,
                 std::min(year, configuration.industrialRevolutionStartYear) -
                     configuration.renaissanceStartYear);
  }
  if (year > configuration.industrialRevolutionStartYear)
    multiplier =
        compound(multiplier, configuration.industrialCapacityGrowthPerCentury,
                 year - configuration.industrialRevolutionStartYear);
  return multiplier;
}

double phaseMultiplier(RegionalPhase phase) {
  switch (phase) {
  case RegionalPhase::Boom:
    return 1.20;
  case RegionalPhase::Bust:
    return 0.80;
  case RegionalPhase::Neutral:
    return 1.0;
  }
  return 1.0;
}

NormalizedInput normalize(const Input &input) {
  NormalizedInput normalized;
  std::map<ProvinceId, RegionId> allProvinceRegions;

  for (const auto &continent : input.continents) {
    if (!continent) {
      normalized.errors.push_back(
          {StartYear, "Input contains a null continent", {}, {}});
      continue;
    }
    for (const auto &ardaRegion : continent->ardaRegions) {
      if (!ardaRegion) {
        normalized.errors.push_back(
            {StartYear, "Input contains a null region", {}, {}});
        continue;
      }
      normalized.regionContinents[ardaRegion->ID] = continent->ID;
      for (const auto &neighbour : ardaRegion->neighbourRegions) {
        if (neighbour)
          normalized.regionNeighbours[ardaRegion->ID].push_back(neighbour->ID);
      }
      for (const auto &province : ardaRegion->ardaProvinces) {
        if (!province) {
          normalized.errors.push_back({StartYear,
                                       "Region contains a null province",
                                       {},
                                       ardaRegion->ID});
          continue;
        }
        const auto [it, inserted] =
            allProvinceRegions.emplace(province->ID, ardaRegion->ID);
        if (!inserted && it->second != ardaRegion->ID) {
          normalized.errors.push_back({StartYear,
                                       "Province belongs to multiple regions",
                                       province->ID, ardaRegion->ID});
          continue;
        }
        if (!isEligible(province))
          continue;
        const auto [provinceIt, provinceInserted] =
            normalized.provinces.emplace(province->ID, province);
        if (!provinceInserted && provinceIt->second != province) {
          normalized.errors.push_back({StartYear,
                                       "Input contains duplicate province IDs",
                                       province->ID, ardaRegion->ID});
          continue;
        }
        normalized.provinceRegions[province->ID] = ardaRegion->ID;
        normalized.provinceContinents[province->ID] = continent->ID;
        normalized.regions[ardaRegion->ID].push_back(province->ID);
      }
    }
  }

  for (auto &[regionId, provinceIds] : normalized.regions)
    std::sort(provinceIds.begin(), provinceIds.end());
  if (input.climateData && (input.climateData->habitabilities.empty() ||
                            input.climateData->arableLand.empty())) {
    normalized.errors.push_back(
        {StartYear,
         "Climate input must provide habitability and arable-land data",
         {},
         {}});
  }
  if (input.terrainData && input.terrainData->inclination.empty()) {
    normalized.errors.push_back(
        {StartYear, "Terrain input must provide inclination data", {}, {}});
  }

  for (const auto &[provinceId, province] : normalized.provinces) {
    auto &neighbours = normalized.neighbours[provinceId];
    for (const auto &baseNeighbour : province->provinceNeighbours) {
      auto neighbour = std::dynamic_pointer_cast<ArdaProvince>(baseNeighbour);
      if (neighbour && normalized.provinces.contains(neighbour->ID))
        neighbours.push_back(neighbour->ID);
    }
    std::sort(neighbours.begin(), neighbours.end());
    neighbours.erase(std::unique(neighbours.begin(), neighbours.end()),
                     neighbours.end());
    NormalizedInput::Environment environment;
    environment.coastal = province->isCoastalToOcean();
    environment.area = province->pixels.size();
    if (input.climateData || input.terrainData) {
      double habitability = 0.0;
      double arableLand = 0.0;
      double inclination = 0.0;
      size_t samples = 0;
      for (const auto pixel : province->pixels) {
        if (pixel < 0)
          continue;
        const auto index = static_cast<size_t>(pixel);
        if (input.climateData &&
            index < input.climateData->habitabilities.size())
          habitability += input.climateData->habitabilities[index];
        if (input.climateData && index < input.climateData->arableLand.size())
          arableLand += input.climateData->arableLand[index];
        if (input.terrainData && index < input.terrainData->inclination.size())
          inclination += input.terrainData->inclination[index];
        ++samples;
      }
      if (samples > 0) {
        environment.habitability = std::clamp(habitability / samples, 0.0, 1.0);
        environment.arableLand = std::clamp(arableLand / samples, 0.0, 1.0);
        environment.inclination = std::max(0.0, inclination / samples);
      }
    }
    if (!input.climateData && province->habitability > 0.0f)
      environment.habitability =
          std::clamp(static_cast<double>(province->habitability), 0.0, 1.0);
    normalized.environments[provinceId] = environment;
  }

  return normalized;
}

bool chance(double probability) {
  return probability > 0.0 && RandNum::getRandom<double>(1.0) < probability;
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

void applyEvent(State &state, const Event &event) {
  state.year = event.year;
  switch (event.type) {
  case EventType::CreatePolity:
    state.polities[event.polityId] = {event.polityId, event.year, std::nullopt,
                                      event.description == "tribe",
                                      event.colour};
    break;
  case EventType::ConvertCulturePopulation:
    if (auto province = state.provinces.find(event.provinceId);
        province != state.provinces.end()) {
      const auto sourceCulture = static_cast<CultureId>(event.parentId);
      province->second.culturePopulations[sourceCulture] =
          std::max(0.0, province->second.culturePopulations[sourceCulture] -
                            event.value);
      province->second.culturePopulations[event.cultureId] += event.value;
      refreshDominantCulture(province->second);
    }
    break;
  case EventType::InitializeProvince:
    state.provinces[event.provinceId] = {event.polityId, event.cultureId,
                                         event.religionId, event.value,
                                         event.secondaryValue};
    state.provinces[event.provinceId].culturePopulations[event.cultureId] =
        event.value;
    state.cultures.try_emplace(event.cultureId,
                               CultureLineage{event.cultureId, event.provinceId,
                                              event.year, std::nullopt,
                                              event.colour});
    break;
  case EventType::TransferProvince:
  case EventType::ConsolidateRegion:
    if (auto province = state.provinces.find(event.provinceId);
        province != state.provinces.end())
      province->second.owner = event.polityId;
    break;
  case EventType::DissolvePolity:
    if (auto polity = state.polities.find(event.polityId);
        polity != state.polities.end())
      polity->second.dissolvedYear = event.year;
    break;
  case EventType::CreateCulture:
    state.cultures.try_emplace(
        event.cultureId,
        CultureLineage{event.cultureId, event.provinceId, event.year,
                       event.parentId >= 0
                           ? std::optional<CultureId>(event.parentId)
                           : std::nullopt,
                       event.colour});
    break;
  case EventType::SetCulture:
    if (auto province = state.provinces.find(event.provinceId);
        province != state.provinces.end())
      province->second.culture = event.cultureId;
    break;
  case EventType::CreateReligion:
    state.religions.try_emplace(event.religionId,
                                ReligionLineage{event.religionId,
                                                event.provinceId, event.year,
                                                std::nullopt, event.colour});
    if (event.parentId >= 0)
      state.religions.at(event.religionId).parentId = event.parentId;
    break;
  case EventType::SetReligion:
    if (auto province = state.provinces.find(event.provinceId);
        province != state.provinces.end())
      province->second.religion = event.religionId;
    break;
  case EventType::UpdatePopulation:
    if (auto province = state.provinces.find(event.provinceId);
        province != state.provinces.end()) {
      const auto previousPopulation = province->second.population;
      province->second.population = event.value;
      if (previousPopulation > 0.0) {
        const auto factor = event.value / previousPopulation;
        for (auto &[cultureId, population] :
             province->second.culturePopulations)
          population *= factor;
      }
    }
    break;
  case EventType::UpdateDevelopment:
    if (auto province = state.provinces.find(event.provinceId);
        province != state.provinces.end())
      province->second.development = event.value;
    break;
  case EventType::UpdateCarryingCapacity:
    if (auto province = state.provinces.find(event.provinceId);
        province != state.provinces.end())
      province->second.carryingCapacity = event.value;
    break;
  case EventType::MigratePopulation:
    if (auto source = state.provinces.find(event.provinceId);
        source != state.provinces.end())
      source->second.population =
          std::max(0.0, source->second.population - event.value);
    if (auto target = state.provinces.find(event.regionId);
        target != state.provinces.end())
      target->second.population += event.value;
    break;
  case EventType::SetRegionalPhase:
    state.regionalPhases[event.regionId] =
        static_cast<RegionalPhase>(static_cast<int>(event.value));
    break;
  case EventType::SetSuperRegion:
    state.regionSuperRegions[event.regionId] =
        static_cast<SuperRegionId>(event.value);
    break;
  case EventType::CreateSuperRegion:
    state.superRegions.try_emplace(
        event.regionId, DevelopmentSuperRegion{event.regionId, event.polityId});
    break;
  case EventType::SetSuperRegionPhase:
    if (auto superRegion =
            state.superRegions.find(static_cast<SuperRegionId>(event.regionId));
        superRegion != state.superRegions.end())
      superRegion->second.phase =
          static_cast<SuperRegionPhase>(static_cast<int>(event.value));
    break;
  case EventType::MigrateCulturePopulation:
    if (auto source = state.provinces.find(event.provinceId);
        source != state.provinces.end())
      source->second.culturePopulations[event.cultureId] =
          std::max(0.0, source->second.culturePopulations[event.cultureId] -
                            event.value);
    if (auto target = state.provinces.find(event.regionId);
        target != state.provinces.end())
      target->second.culturePopulations[event.cultureId] += event.value;
    if (auto source = state.provinces.find(event.provinceId);
        source != state.provinces.end())
      source->second.culture = state.dominantCultureOf(event.provinceId);
    if (auto target = state.provinces.find(event.regionId);
        target != state.provinces.end())
      target->second.culture = state.dominantCultureOf(event.regionId);
    break;
  case EventType::ColonizeProvince:
    if (auto province = state.provinces.find(event.provinceId);
        province != state.provinces.end())
      province->second.owner = event.polityId;
    break;
  }
}

const char *eventTypeName(EventType type) {
  switch (type) {
  case EventType::InitializeProvince:
    return "InitializeProvince";
  case EventType::CreatePolity:
    return "CreatePolity";
  case EventType::TransferProvince:
    return "TransferProvince";
  case EventType::DissolvePolity:
    return "DissolvePolity";
  case EventType::CreateCulture:
    return "CreateCulture";
  case EventType::SetCulture:
    return "SetCulture";
  case EventType::CreateReligion:
    return "CreateReligion";
  case EventType::SetReligion:
    return "SetReligion";
  case EventType::UpdatePopulation:
    return "UpdatePopulation";
  case EventType::UpdateDevelopment:
    return "UpdateDevelopment";
  case EventType::UpdateCarryingCapacity:
    return "UpdateCarryingCapacity";
  case EventType::MigratePopulation:
    return "MigratePopulation";
  case EventType::SetRegionalPhase:
    return "SetRegionalPhase";
  case EventType::SetSuperRegion:
    return "SetSuperRegion";
  case EventType::CreateSuperRegion:
    return "CreateSuperRegion";
  case EventType::SetSuperRegionPhase:
    return "SetSuperRegionPhase";
  case EventType::MigrateCulturePopulation:
    return "MigrateCulturePopulation";
  case EventType::ConvertCulturePopulation:
    return "ConvertCulturePopulation";
  case EventType::ColonizeProvince:
    return "ColonizeProvince";
  case EventType::ConsolidateRegion:
    return "ConsolidateRegion";
  }
  return "Unknown";
}

} // namespace

const ProvinceState *State::findProvince(ProvinceId provinceId) const {
  const auto province = provinces.find(provinceId);
  return province == provinces.end() ? nullptr : &province->second;
}

std::vector<ProvinceId> State::territoryOf(PolityId polityId) const {
  std::vector<ProvinceId> territory;
  for (const auto &[provinceId, province] : provinces) {
    if (province.owner == polityId)
      territory.push_back(provinceId);
  }
  return territory;
}

CultureId State::dominantCultureOf(ProvinceId provinceId) const {
  const auto province = provinces.find(provinceId);
  if (province == provinces.end() ||
      province->second.culturePopulations.empty())
    return -1;
  return province->second.culture;
}

HistorySimulation::HistorySimulation(Configuration configuration)
    : configuration(std::move(configuration)) {}

Result HistorySimulation::run(const Input &input) {
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
  std::map<ProvinceId, double> growthPotential;
  std::map<ProvinceId, double> baseCapacity;
  auto append = [&](Event &&event) {
    result.events.emplace_back(std::move(event));
    applyEvent(state, result.events.back());
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
    const double population =
        std::min(environmentalCapacity,
                 (province->population > 0.0 ? province->population
                                             : environmentalCapacity * 0.25) *
                     potential);
    const double development = (province->averageDevelopment > 0.0
                                    ? province->averageDevelopment
                                    : configuration.defaultDevelopment) *
                               potential;
    append({configuration.startYear, EventType::CreatePolity, -1, -1, polityId,
            NoPolity, -1, NoReligion, 0.0, 0.0, "tribe", -1,
            randomPolityColour()});
    append({configuration.startYear, EventType::InitializeProvince, provinceId,
            normalized.provinceRegions.at(provinceId), polityId, NoPolity,
            cultureId, NoReligion, population, development,
            "initial tribe and culture", -1, randomPolityColour()});
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
    Fwg::Utils::Logging::logLine("Simulating year ", std::to_string(year),
                                 " of ",
                                 std::to_string(configuration.targetYear));
    const double centuries = static_cast<double>(nextYear - year) / 100.0;
    if (configuration.superRegionCycleYears > 0 &&
        (nextYear - configuration.startYear) /
                configuration.superRegionCycleYears >
            (year - configuration.startYear) /
                configuration.superRegionCycleYears) {
      std::vector<SuperRegionId> ordered;
      for (const auto &[id, superRegion] : state.superRegions)
        ordered.push_back(id);
      if (nextYear >= configuration.persistentDevelopmentStartYear) {
        std::sort(ordered.begin(), ordered.end(),
                  [&](const auto left, const auto right) {
                    return state.superRegions.at(left).development >
                           state.superRegions.at(right).development;
                  });
      } else {
        std::rotate(ordered.begin(),
                    ordered.begin() + RandNum::getRandom<int>(
                                          static_cast<int>(ordered.size())),
                    ordered.end());
      }
      for (size_t index = 0; index < ordered.size(); ++index) {
        const auto phase =
            index < 2 ? SuperRegionPhase::Booming : SuperRegionPhase::Lagging;
        append({nextYear, EventType::SetSuperRegionPhase, -1, ordered[index],
                NoPolity, NoPolity, -1, NoReligion,
                static_cast<double>(static_cast<int>(phase)), 0.0,
                "superregional development cycle"});
      }
    }
    if (configuration.regionPhaseDurationYears > 0 &&
        (nextYear - configuration.startYear) /
                configuration.regionPhaseDurationYears >
            (year - configuration.startYear) /
                configuration.regionPhaseDurationYears) {
      for (const auto &[regionId, provinces] : normalized.regions) {
        const auto phase =
            static_cast<RegionalPhase>(RandNum::getRandom<int>(3) - 1);
        append({nextYear, EventType::SetRegionalPhase, -1, regionId, NoPolity,
                NoPolity, -1, NoReligion,
                static_cast<double>(static_cast<int>(phase)), 0.0,
                "regional boom or bust transition"});
      }
    }
    std::map<PolityId, std::vector<ProvinceId>> territories;
    for (const auto &[provinceId, province] : state.provinces) {
      const auto potential = growthPotential.at(provinceId);
      const auto regionId = normalized.provinceRegions.at(provinceId);
      const auto phase = state.regionalPhases.contains(regionId)
                             ? state.regionalPhases.at(regionId)
                             : RegionalPhase::Neutral;
      const auto superRegionId = state.regionSuperRegions.at(regionId);
      const auto superPhase = state.superRegions.at(superRegionId).phase;
      const bool bordersBoomingSuperRegion =
          std::any_of(state.superRegions.at(superRegionId).neighbours.begin(),
                      state.superRegions.at(superRegionId).neighbours.end(),
                      [&](const auto neighbourId) {
                        return state.superRegions.at(neighbourId).phase ==
                               SuperRegionPhase::Booming;
                      });
      const double superRegionMultiplier =
          (superPhase == SuperRegionPhase::Booming   ? 1.8
           : superPhase == SuperRegionPhase::Lagging ? 0.55
                                                     : 1.0) +
          (bordersBoomingSuperRegion && superPhase != SuperRegionPhase::Booming
               ? configuration.superRegionNeighbourInfluence
               : 0.0);
      const double capacity = baseCapacity.at(provinceId) *
                              capacityEraMultiplier(configuration, nextYear) *
                              (1.0 + province.development * 0.05) *
                              phaseMultiplier(phase) * superRegionMultiplier;
      const double densityFactor =
          1.0 - province.population / std::max(capacity, 1.0);
      const double population =
          std::max(configuration.minimumPopulation,
                   province.population *
                       (1.0 + configuration.populationGrowthPerCentury *
                                  potential * densityFactor * centuries));
      const double developmentChange =
          configuration.developmentGrowthPerCentury * potential *
          (1.0 + std::log1p(province.population) / 10.0) * centuries *
          (phase == RegionalPhase::Bust ? -0.35 : phaseMultiplier(phase)) *
          superRegionMultiplier;
      const double development =
          std::max(0.0, province.development + developmentChange);
      state.superRegions[superRegionId].development += developmentChange;
      append({nextYear, EventType::UpdateCarryingCapacity, provinceId, regionId,
              NoPolity, NoPolity, -1, NoReligion, capacity,
              capacityEraMultiplier(configuration, nextYear),
              "era-adjusted carrying capacity"});
      append({nextYear, EventType::UpdatePopulation, provinceId, -1, NoPolity,
              NoPolity, -1, NoReligion, population, 0.0, "population growth"});
      append({nextYear, EventType::UpdateDevelopment, provinceId, -1, NoPolity,
              NoPolity, -1, NoReligion, development, 0.0,
              "development growth"});
      territories[province.owner].push_back(provinceId);
    }
    for (const auto &[provinceId, province] : state.provinces) {
      const auto sourceCapacity =
          state.provinces.at(provinceId).carryingCapacity;
      const auto sourceAttraction =
          sourceCapacity - province.population + province.development * 10.0;
      ProvinceId destinationId = -1;
      double bestAttraction = sourceAttraction;
      for (const auto neighbourId : normalized.neighbours.at(provinceId)) {
        const auto &destination = state.provinces.at(neighbourId);
        const auto attraction = destination.carryingCapacity -
                                destination.population +
                                destination.development * 10.0;
        if (attraction > bestAttraction) {
          bestAttraction = attraction;
          destinationId = neighbourId;
        }
      }
      if (destinationId >= 0) {
        const auto amount = std::min(
            province.population * configuration.migrationRatePerCentury *
                centuries,
            std::max(0.0, state.provinces.at(destinationId).carryingCapacity -
                              state.provinces.at(destinationId).population));
        if (amount > 0.0)
          append({nextYear, EventType::MigratePopulation, provinceId,
                  destinationId, NoPolity, NoPolity, -1, NoReligion, amount,
                  0.0, "adjacent migration"});
        for (const auto &[cultureId, culturePopulation] :
             state.provinces.at(provinceId).culturePopulations) {
          const auto culturalAmount =
              amount * culturePopulation /
              std::max(1.0, state.provinces.at(provinceId).population + amount);
          if (culturalAmount > 0.0)
            append({nextYear, EventType::MigrateCulturePopulation, provinceId,
                    destinationId, NoPolity, NoPolity, cultureId, NoReligion,
                    culturalAmount, 0.0, "adjacent cultural migration"});
        }
      }
    }
    if (nextYear >= configuration.longDistanceMigrationStartYear) {
      Fwg::Utils::Logging::logLine(
          "Year %d: %zu provinces, %zu polities, %zu cultures, %zu religions",
          nextYear, state.provinces.size(), state.polities.size(),
          state.cultures.size(), state.religions.size());
      for (const auto &[provinceId, province] : state.provinces) {
        ProvinceId destinationId = -1;
        double bestHeadroom = 0.0;
        for (const auto &[candidateId, candidate] : state.provinces) {
          if (candidateId == provinceId ||
              normalized.provinceContinents.at(candidateId) !=
                  normalized.provinceContinents.at(provinceId))
            continue;
          const auto headroom =
              candidate.carryingCapacity - candidate.population;
          if (headroom > bestHeadroom) {
            bestHeadroom = headroom;
            destinationId = candidateId;
          }
        }
        if (destinationId >= 0) {
          const auto amount = std::min(
              province.population *
                  configuration.longDistanceMigrationRatePerCentury * centuries,
              bestHeadroom);
          if (amount > 0.0)
            append({nextYear, EventType::MigratePopulation, provinceId,
                    destinationId, NoPolity, NoPolity, -1, NoReligion, amount,
                    0.0, "same-continent migration"});
          for (const auto &[cultureId, culturePopulation] :
               state.provinces.at(provinceId).culturePopulations) {
            const auto culturalAmount =
                amount * culturePopulation /
                std::max(1.0,
                         state.provinces.at(provinceId).population + amount);
            if (culturalAmount > 0.0)
              append({nextYear, EventType::MigrateCulturePopulation, provinceId,
                      destinationId, NoPolity, NoPolity, cultureId, NoReligion,
                      culturalAmount, 0.0,
                      "same-continent cultural migration"});
          }
        }
      }
    }
    state.polityStrengths.clear();
    for (const auto &[provinceId, province] : state.provinces) {
      auto &strength = state.polityStrengths[province.owner];
      strength.population += province.population;
      strength.development += province.development;
    }
    for (auto &[polityId, strength] : state.polityStrengths)
      strength.score = strength.population * 0.01 + strength.development * 10.0;
    const auto strongestPolity = std::max_element(
        state.polityStrengths.begin(), state.polityStrengths.end(),
        [](const auto &left, const auto &right) {
          return left.second.score < right.second.score;
        });
    const auto stabilityThreshold =
        strongestPolity == state.polityStrengths.end()
            ? 0.0
            : strongestPolity->second.score * 0.5;
    for (const auto &[provinceId, province] : state.provinces) {
      if (state.polityStrengths.at(province.owner).score < stabilityThreshold ||
          province.culturePopulations.size() < 2)
        continue;
      const auto dominantCulture = state.dominantCultureOf(provinceId);
      for (const auto &[cultureId, culturalPopulation] :
           province.culturePopulations) {
        if (cultureId == dominantCulture)
          continue;
        const auto conversion =
            culturalPopulation *
            configuration.culturalIntegrationRatePerCentury * centuries;
        if (conversion > 0.0)
          append({nextYear, EventType::ConvertCulturePopulation, provinceId, -1,
                  NoPolity, NoPolity, dominantCulture, NoReligion, conversion,
                  0.0, "stable polity cultural integration", cultureId});
      }
    }
    if (nextYear >= configuration.colonizationStartYear &&
        nextYear < configuration.regionOwnershipYear &&
        strongestPolity != state.polityStrengths.end()) {
      ProvinceId sourceProvince = -1;
      for (const auto &[provinceId, province] : state.provinces) {
        if (province.owner == strongestPolity->first &&
            normalized.provinces.at(provinceId)->isCoastalToOcean() &&
            (sourceProvince < 0 ||
             province.development >
                 state.provinces.at(sourceProvince).development))
          sourceProvince = provinceId;
      }
      ProvinceId targetProvince = -1;
      if (sourceProvince >= 0) {
        for (const auto &[provinceId, province] : state.provinces) {
          const auto targetRegion = normalized.provinceRegions.at(provinceId);
          const auto targetSuperRegion =
              state.regionSuperRegions.at(targetRegion);
          const bool lagging = state.superRegions.at(targetSuperRegion).phase ==
                               SuperRegionPhase::Lagging;
          const bool reachableByLand =
              normalized.provinceContinents.at(provinceId) ==
              normalized.provinceContinents.at(sourceProvince);
          const bool reachableBySea =
              normalized.provinces.at(provinceId)->isCoastalToOcean();
          if (province.owner != strongestPolity->first && lagging &&
              (reachableByLand || reachableBySea) &&
              (targetProvince < 0 ||
               province.development <
                   state.provinces.at(targetProvince).development))
            targetProvince = provinceId;
        }
      }
      if (targetProvince >= 0 &&
          chance(configuration.colonialSettlementRatePerCentury * centuries)) {
        const auto settlers = std::min(
            state.provinces.at(sourceProvince).population *
                configuration.colonialSettlementRatePerCentury * centuries,
            std::max(0.0, state.provinces.at(targetProvince).carryingCapacity -
                              state.provinces.at(targetProvince).population));
        if (settlers > 0.0) {
          append({nextYear, EventType::ColonizeProvince, targetProvince,
                  normalized.provinceRegions.at(targetProvince),
                  strongestPolity->first,
                  state.provinces.at(targetProvince).owner, -1, NoReligion,
                  settlers, 0.0, "colonial settlement"});
          append({nextYear, EventType::MigratePopulation, sourceProvince,
                  targetProvince, NoPolity, NoPolity, -1, NoReligion, settlers,
                  0.0, "colonial migration"});
          for (const auto &[cultureId, culturalPopulation] :
               state.provinces.at(sourceProvince).culturePopulations) {
            const auto culturalSettlers =
                settlers * culturalPopulation /
                std::max(1.0, state.provinces.at(sourceProvince).population +
                                  settlers);
            if (culturalSettlers > 0.0)
              append({nextYear, EventType::MigrateCulturePopulation,
                      sourceProvince, targetProvince, NoPolity, NoPolity,
                      cultureId, NoReligion, culturalSettlers, 0.0,
                      "colonial cultural migration"});
          }
        }
      }
    }

    const auto currentPolityCount = static_cast<double>(territories.size());
    const auto initialPolityCount =
        static_cast<double>(normalized.provinces.size());
    const double simulationProgress =
        std::clamp(static_cast<double>(nextYear - configuration.startYear) /
                       static_cast<double>(configuration.targetYear -
                                           configuration.startYear),
                   0.0, 1.0);
    const double desiredPolityCount =
        initialPolityCount +
        (static_cast<double>(configuration.targetEndPolityCount) -
         initialPolityCount) *
            simulationProgress;
    const double consolidationPressure =
        initialPolityCount == configuration.targetEndPolityCount
            ? 0.0
            : std::clamp(
                  (currentPolityCount - desiredPolityCount) /
                      (initialPolityCount - configuration.targetEndPolityCount),
                  0.0, 1.0);

    if (nextYear < configuration.regionOwnershipYear) {
      for (const auto &[provinceId, province] : state.provinces) {
        if (!chance(configuration.expansionChance * centuries *
                    consolidationPressure))
          continue;
        const auto &neighbours = normalized.neighbours.at(provinceId);
        if (neighbours.empty())
          continue;
        const auto firstNeighbour =
            RandNum::getRandom<int>(static_cast<int>(neighbours.size()));
        for (size_t offset = 0; offset < neighbours.size(); ++offset) {
          const auto neighbourId =
              neighbours[(firstNeighbour + offset) % neighbours.size()];
          const auto &neighbour = state.provinces.at(neighbourId);
          if (neighbour.owner != province.owner) {
            const auto attacker =
                state.polityStrengths.at(province.owner).score;
            const auto defender =
                state.polityStrengths.at(neighbour.owner).score;
            if (!chance(attacker / std::max(1.0, attacker + defender)))
              continue;
            append({nextYear, EventType::TransferProvince, neighbourId,
                    normalized.provinceRegions.at(neighbourId), province.owner,
                    neighbour.owner, -1, NoReligion, 0.0, 0.0,
                    "provincial expansion"});
            break;
          }
        }
      }

      territories.clear();
      for (const auto &[provinceId, province] : state.provinces)
        territories[province.owner].push_back(provinceId);
      for (const auto &[polityId, provinceIds] : territories) {
        if (provinceIds.size() < 2 ||
            !chance(configuration.fragmentationChance * centuries *
                    (1.0 - consolidationPressure)))
          continue;
        const auto splitProvince = provinceIds.back();
        const auto splitPolity = nextPolity++;
        append({nextYear, EventType::CreatePolity, -1, -1, splitPolity,
                NoPolity, -1, NoReligion, 0.0, 0.0, "tribe", -1,
                randomPolityColour()});
        append({nextYear, EventType::TransferProvince, splitProvince,
                normalized.provinceRegions.at(splitProvince), splitPolity,
                polityId, -1, NoReligion, 0.0, 0.0, "fragmentation"});
      }
    }

    if (nextYear >= configuration.classicalStartYear) {
      for (const auto &[provinceId, province] : state.provinces) {
        if (province.religion == NoReligion &&
            chance(configuration.religionEmergenceChance * centuries)) {
          const auto religionId = nextReligion++;
          append({nextYear, EventType::CreateReligion, provinceId, -1, NoPolity,
                  NoPolity, -1, religionId, 0.0, 0.0, "religion emergence", -1,
                  randomPolityColour()});
          append({nextYear, EventType::SetReligion, provinceId, -1, NoPolity,
                  NoPolity, -1, religionId, 0.0, 0.0, "religion adoption"});
        }
        if (province.religion != NoReligion &&
            chance(configuration.religionSplitChance * centuries)) {
          const auto religionId = nextReligion++;
          append({nextYear, EventType::CreateReligion, provinceId, -1, NoPolity,
                  NoPolity, -1, religionId, 0.0, 0.0, "religion split",
                  province.religion, randomPolityColour()});
          append({nextYear, EventType::SetReligion, provinceId, -1, NoPolity,
                  NoPolity, -1, religionId, 0.0, 0.0, "religion schism"});
        }
        for (const auto neighbourId : normalized.neighbours.at(provinceId)) {
          const auto &neighbour = state.provinces.at(neighbourId);
          if (province.religion == NoReligion &&
              neighbour.religion != NoReligion &&
              chance(configuration.religionSpreadChance * centuries)) {
            append({nextYear, EventType::SetReligion, provinceId, -1, NoPolity,
                    NoPolity, -1, neighbour.religion, 0.0, 0.0,
                    "religion spread"});
            break;
          }
          if (province.culture != neighbour.culture &&
              province.owner == neighbour.owner &&
              chance(configuration.cultureAssimilationChance * centuries)) {
            append({nextYear, EventType::SetCulture, provinceId, -1, NoPolity,
                    NoPolity, neighbour.culture, NoReligion, 0.0, 0.0,
                    "cultural assimilation"});
            break;
          }
          if (province.culture == neighbour.culture &&
              chance(configuration.cultureSplitChance * centuries)) {
            const auto cultureId = nextCulture++;
            append({nextYear, EventType::CreateCulture, provinceId, -1,
                    NoPolity, NoPolity, cultureId, NoReligion, 0.0, 0.0,
                    "culture split", province.culture, randomPolityColour()});
            append({nextYear, EventType::SetCulture, provinceId, -1, NoPolity,
                    NoPolity, cultureId, NoReligion, 0.0, 0.0,
                    "cultural differentiation"});
            break;
          }
        }
      }
    }

    if (nextYear >= configuration.regionOwnershipYear) {
      for (const auto &[regionId, provinceIds] : normalized.regions) {
        std::map<PolityId, int> ownership;
        for (const auto provinceId : provinceIds)
          ++ownership[state.provinces.at(provinceId).owner];
        const auto dominant =
            std::max_element(ownership.begin(), ownership.end(),
                             [](const auto &left, const auto &right) {
                               return left.second != right.second
                                          ? left.second < right.second
                                          : left.first > right.first;
                             })
                ->first;
        for (const auto provinceId : provinceIds) {
          const auto previous = state.provinces.at(provinceId).owner;
          if (previous != dominant)
            append({nextYear, EventType::ConsolidateRegion, provinceId,
                    regionId, dominant, previous, -1, NoReligion, 0.0, 0.0,
                    "regional consolidation"});
        }
      }
    }

    territories.clear();
    for (const auto &[provinceId, province] : state.provinces)
      territories[province.owner].push_back(provinceId);
    for (const auto &[polityId, polity] : state.polities) {
      if (!polity.dissolvedYear && !territories.contains(polityId))
        append({nextYear, EventType::DissolvePolity, -1, -1, polityId, NoPolity,
                -1, NoReligion, 0.0, 0.0, "annexed or dissolved"});
    }
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
           event.provinceId, event.regionId});
    if (event.year < previousYear)
      result.errors.push_back({event.year,
                               "Event log is not chronologically ordered",
                               event.provinceId, event.regionId});
    if (event.type == EventType::TransferProvince &&
        event.description == "provincial expansion") {
      const auto neighbours = provinceNeighbours.find(event.provinceId);
      const bool touchesOwner =
          neighbours != provinceNeighbours.end() &&
          std::any_of(neighbours->second.begin(), neighbours->second.end(),
                      [&](ProvinceId neighbourId) {
                        const auto neighbour =
                            eventState.provinces.find(neighbourId);
                        return neighbour != eventState.provinces.end() &&
                               neighbour->second.owner == event.polityId;
                      });
      if (!touchesOwner)
        result.errors.push_back(
            {event.year,
             "Provincial expansion does not cross a valid land border",
             event.provinceId, event.regionId});
    }
    applyEvent(eventState, event);
    previousYear = event.year;
  }
  return result;
}

State HistorySimulation::reconstruct(const std::vector<Event> &events,
                                     Year year) const {
  State state;
  state.year = configuration.startYear;
  for (const auto &event : events) {
    if (event.year > year)
      break;
    applyEvent(state, event);
  }
  state.year = year;
  return state;
}

std::vector<ValidationError>
HistorySimulation::validate(const State &state, Year year,
                            bool requireWholeRegions) const {
  std::vector<ValidationError> errors;
  for (const auto &[provinceId, province] : state.provinces) {
    if (province.owner == NoPolity || !state.polities.contains(province.owner))
      errors.push_back({year, "Province has no valid owner", provinceId, {}});
    if (province.culture < 0 || !state.cultures.contains(province.culture))
      errors.push_back({year, "Province has no valid culture", provinceId, {}});
    if (province.religion != NoReligion &&
        !state.religions.contains(province.religion))
      errors.push_back(
          {year, "Province has no valid religion", provinceId, {}});
    if (province.population < configuration.minimumPopulation)
      errors.push_back({year,
                        "Province population is below the configured minimum",
                        provinceId,
                        {}});
    if (province.development < 0.0)
      errors.push_back(
          {year, "Province development is negative", provinceId, {}});
    if (province.carryingCapacity < configuration.minimumPopulation)
      errors.push_back(
          {year,
           "Province carrying capacity is below the configured minimum",
           provinceId,
           {}});
    const auto culturalPopulation = std::accumulate(
        province.culturePopulations.begin(), province.culturePopulations.end(),
        0.0,
        [](double total, const auto &entry) { return total + entry.second; });
    if (std::abs(culturalPopulation - province.population) >
        std::max(0.001, province.population * 0.001))
      errors.push_back(
          {year,
           "Cultural populations do not sum to province population",
           provinceId,
           {}});
  }
  const auto boomingSuperRegions =
      std::count_if(state.superRegions.begin(), state.superRegions.end(),
                    [](const auto &entry) {
                      return entry.second.phase == SuperRegionPhase::Booming;
                    });
  if (boomingSuperRegions > 2)
    errors.push_back({year, "More than two superregions are booming", {}, {}});
  if (requireWholeRegions) {
    std::map<RegionId, PolityId> owners;
    for (const auto &[provinceId, province] : state.provinces) {
      const auto region = provinceRegions.find(provinceId);
      if (region == provinceRegions.end())
        continue;
      const auto [owner, inserted] =
          owners.emplace(region->second, province.owner);
      if (!inserted && owner->second != province.owner)
        errors.push_back(
            {year, "Region has split ownership", {}, region->second});
    }
  }
  return errors;
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
    eventLog << "year\ttype\tprovince\tregion\tpolity\tprevious_"
                "polity\tculture\treligion\tparent\tvalue\tsecondary_"
                "value\tred\tgreen\tblue\tdescription\n";
    for (const auto &event : result.events) {
      eventLog << event.year << '\t' << eventTypeName(event.type) << '\t'
               << event.provinceId << '\t' << event.regionId << '\t'
               << event.polityId << '\t' << event.previousPolityId << '\t'
               << event.cultureId << '\t' << event.religionId << '\t'
               << event.parentId << '\t' << event.value << '\t'
               << event.secondaryValue << '\t'
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
        developmentLog << year << '\t' << provinceId << '\t'
                       << provinceState.development << '\n';
        const auto regionId = normalized.provinceRegions.at(provinceId);
        populationLog << year << '\t' << provinceId << '\t'
                      << provinceState.population << '\t'
                      << provinceState.carryingCapacity << '\t'
                      << static_cast<int>(
                             state.regionalPhases.contains(regionId)
                                 ? state.regionalPhases.at(regionId)
                                 : RegionalPhase::Neutral)
                      << '\n';
        for (const auto &[cultureId, culturePopulation] :
             provinceState.culturePopulations) {
          const auto &culture = state.cultures.at(cultureId);
          cultureLog << year << '\t' << provinceId << '\t' << cultureId << '\t'
                     << culturePopulation << '\t'
                     << (cultureId == state.dominantCultureOf(provinceId) ? 1
                                                                          : 0)
                     << '\t' << culture.originProvinceId << '\t'
                     << culture.foundedYear << '\t'
                     << culture.parentId.value_or(-1) << '\n';
        }
        if (provinceState.religion != NoReligion) {
          const auto &religion = state.religions.at(provinceState.religion);
          religionLog << year << '\t' << provinceId << '\t'
                      << provinceState.religion << '\t'
                      << religion.originProvinceId << '\t'
                      << religion.foundedYear << '\t'
                      << religion.parentId.value_or(-1) << '\n';
        } else {
          religionLog << year << '\t' << provinceId << "\t-1\t-1\t-1\t-1\n";
        }
        for (const auto &[superRegionId, superRegion] : state.superRegions)
          superRegionLog << year << '\t' << superRegionId << '\t'
                         << superRegion.continentId << '\t'
                         << static_cast<int>(superRegion.phase) << '\t'
                         << superRegion.development << '\n';
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
      Fwg::Gfx::Png::save(frame, mapPath.string(), false);
      paths.ownershipMaps.push_back(mapPath);
      const auto developmentPath = developmentDirectory / filename.str();
      const auto populationPath = populationDirectory / filename.str();
      const auto culturePath = cultureDirectory / filename.str();
      const auto religionPath = religionDirectory / filename.str();
      Fwg::Gfx::Png::save(developmentFrame, developmentPath.string(), false);
      Fwg::Gfx::Png::save(populationFrame, populationPath.string(), false);
      Fwg::Gfx::Png::save(cultureFrame, culturePath.string(), false);
      Fwg::Gfx::Png::save(religionFrame, religionPath.string(), false);
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