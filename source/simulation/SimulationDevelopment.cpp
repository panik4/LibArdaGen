#include "simulation/SimulationDevelopment.h"

#include "RandNum.h"
#include "utils/Cfg.h"

#include <algorithm>
#include <cmath>

namespace Arda::Simulation::development {

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
  if (year > configuration.renaissanceStartYear)
    multiplier =
        compound(multiplier, configuration.renaissanceCapacityGrowthPerCentury,
                 std::min(year, configuration.industrialRevolutionStartYear) -
                     configuration.renaissanceStartYear);
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

double polityCapacityAt(const Configuration &configuration, Year year) {
  const auto compound = [](double multiplier, double rate, Year years) {
    return multiplier *
           std::pow(1.0 + rate,
                    static_cast<double>(std::max(0, years)) / 100.0);
  };
  const auto renaissanceStart =
      std::max(configuration.renaissanceStartYear, configuration.startYear);
  const auto industrialStart =
      std::max(configuration.industrialRevolutionStartYear, renaissanceStart);
  auto multiplier =
      compound(1.0, configuration.ancientPolityCapacityGrowthPerCentury,
               std::min(year, renaissanceStart) - configuration.startYear);
  multiplier = compound(multiplier,
                        configuration.renaissancePolityCapacityGrowthPerCentury,
                        std::min(year, industrialStart) - renaissanceStart);
  return static_cast<double>(configuration.initialPolityCapacity) *
         compound(multiplier,
                  configuration.industrialPolityCapacityGrowthPerCentury,
                  year - industrialStart);
}

double capacityExpansionMultiplier(const Configuration &configuration,
                                   Year year, size_t territorySize) {
  const auto capacity = polityCapacityAt(configuration, year);
  if (capacity <= 0.0 || territorySize <= capacity)
    return 1.0;
  const auto overCapacity =
      (static_cast<double>(territorySize) - capacity) / capacity;
  return 1.0 /
         (1.0 + configuration.overCapacityExpansionPenalty * overCapacity);
}

using detail::AppendEvent;
using detail::NormalizedInput;

bool chance(double probability) {
  return probability > 0.0 && RandNum::getRandom<double>(1.0) < probability;
}

void updateSuperRegionPhases(Year year, Year nextYear,
                             const Configuration &configuration, State &state,
                             const AppendEvent &append) {
  std::vector<SuperRegionId> ordered;
  for (const auto &[id, superRegion] : state.superRegions)
    ordered.push_back(id);
  if (nextYear >= configuration.persistentDevelopmentStartYear) {
    std::sort(ordered.begin(), ordered.end(),
              [&](const auto left, const auto right) {
                return state.superRegions.at(left).development >
                       state.superRegions.at(right).development;
              });
  } else if (!ordered.empty()) {
    std::rotate(ordered.begin(),
                ordered.begin() +
                    RandNum::getRandom<int>(static_cast<int>(ordered.size())),
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

void updateRegionalPhases(Year year, Year nextYear,
                          const Configuration &configuration,
                          const NormalizedInput &normalized,
                          const AppendEvent &append) {
  for (const auto &[regionId, provinces] : normalized.regions) {
    const auto phase =
        static_cast<RegionalPhase>(RandNum::getRandom<int>(3) - 1);
    append({nextYear, EventType::SetRegionalPhase, -1, regionId, NoPolity,
            NoPolity, -1, NoReligion,
            static_cast<double>(static_cast<int>(phase)), 0.0,
            "regional boom or bust transition"});
  }
}

void updateProvinceGrowth(
    const Year nextYear, double centuries, const Configuration &configuration,
    const NormalizedInput &normalized, State &state,
    const std::map<ProvinceId, double> &growthPotential,
    const std::map<ProvinceId, double> &baseCapacity, const AppendEvent &append,
    std::map<PolityId, std::vector<ProvinceId>> &territories) {
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
    const double densityFactor = std::clamp(
        1.0 - province.population / std::max(capacity, 1.0), 0.0, 1.0);
    const auto immigrationHeadroom =
        std::any_of(normalized.neighbours.at(provinceId).begin(),
                    normalized.neighbours.at(provinceId).end(),
                    [&](const auto neighbourId) {
                      const auto &neighbour = state.provinces.at(neighbourId);
                      return neighbour.carryingCapacity > neighbour.population;
                    });
    const double phaseGrowth = phaseMultiplier(phase) * superRegionMultiplier;
    const double immigrationBonus = immigrationHeadroom ? 1.0 : 0.0;
    const double emigrationPenalty =
        normalized.neighbours.at(provinceId).empty() ? 0.0 : 0.25;
    const double gainChance = std::clamp(
        configuration.populationGrowthPerCentury * centuries * potential *
            densityFactor *
            (0.5 + phaseGrowth + immigrationBonus - emigrationPenalty),
        0.0, 1.0);
    auto population = province.population;
    if (chance(gainChance) && population < capacity)
      population = std::floor(population) + 1.0;
    population = std::max(configuration.minimumPopulation,
                          std::min(population, std::floor(capacity)));
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
    if (population != province.population) {
      append({nextYear, EventType::UpdatePopulation, provinceId, -1, NoPolity,
              NoPolity, -1, NoReligion, population, 0.0, "population growth"});
    }
    if (development != province.development) {
      append({nextYear, EventType::UpdateDevelopment, provinceId, -1, NoPolity,
              NoPolity, -1, NoReligion, development, 0.0,
              "development growth"});
    }
    territories[province.owner].push_back(provinceId);
  }
}

void updatePolityStrengths(const Year nextYear, State &state,
                           const AppendEvent &append) {
  std::map<PolityId, PolityStrength> strengths;
  for (const auto &[provinceId, province] : state.provinces) {
    auto &strength = strengths[province.owner];
    strength.population += province.population;
    strength.development += province.development;
  }
  for (auto &[polityId, strength] : strengths) {
    strength.score = strength.population * 0.01 + strength.development * 10.0;
    append({nextYear,
            EventType::UpdatePolityStrength,
            -1,
            -1,
            polityId,
            NoPolity,
            -1,
            NoReligion,
            strength.population,
            strength.development,
            "polity strength snapshot",
            -1,
            {},
            strength.score});
  }
}

} // namespace Arda::Simulation::development
