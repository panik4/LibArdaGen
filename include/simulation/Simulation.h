#pragma once

#include "areas/ArdaContinent.h"
#include "climate/ClimateData.h"
#include "entities/Colour.h"
#include "terrain/TerrainData.h"
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Arda::Simulation {

using Year = int;
using ProvinceId = int;
using RegionId = int;
using PolityId = int;
using CultureId = int;
using ReligionId = int;
using SuperRegionId = int;

inline constexpr Year StartYear = -4000;
inline constexpr Year RegionOwnershipYear = 1836;
inline constexpr Year TargetYear = 1936;
inline constexpr PolityId NoPolity = -1;
inline constexpr ReligionId NoReligion = -1;

// The global RandNum sequence controls reproducibility. This primitive model is
// intentionally independent of Country, Culture, CultureGroup, and Religion;
// consumers materialize those domain objects from a reconstructed State.
struct Configuration {
  Year startYear = StartYear;
  Year regionOwnershipYear = RegionOwnershipYear;
  Year targetYear = TargetYear;
  int ancientStepYears = 100;
  int classicalStepYears = 50;
  int medievalStepYears = 20;
  int modernStepYears = 5;
  Year classicalStartYear = -500;
  Year medievalStartYear = 500;
  Year modernStartYear = 1700;
  Year renaissanceStartYear = 1450;
  Year industrialRevolutionStartYear = 1760;
  int targetEndPolityCount = 100;
  int targetDevelopmentSuperRegionCount = 10;
  int superRegionCycleYears = 500;
  Year persistentDevelopmentStartYear = 1000;
  Year colonizationStartYear = 1500;
  Year polityAggressionStartYear = -3000;
  double superRegionNeighbourInfluence = 0.35;
  double culturalIntegrationRatePerCentury = 0.01;
  double colonialSettlementRatePerCentury = 0.03;
  double populationGrowthPerCentury = 0.06;
  double developmentGrowthPerCentury = 0.01;
  int regionPhaseDurationYears = 300;
  Year longDistanceMigrationStartYear = 1500;
  double ancientCapacityGrowthPerCentury = 0.005;
  double renaissanceCapacityGrowthPerCentury = 0.015;
  double industrialCapacityGrowthPerCentury = 0.08;
  double migrationRatePerCentury = 0.04;
  double longDistanceMigrationRatePerCentury = 0.01;
  double expansionChance = 0.30;
  double fragmentationChance = 0.04;
  double aggressiveExpansionMultiplier = 2.0;
  double aggressiveFragmentationMultiplier = 2.5;
  double splitOffChance = 0.12;
  int maximumImplosionSuccessors = 10;
  size_t initialPolityCapacity = 4;
  double ancientPolityCapacityGrowthPerCentury = 0.04;
  double renaissancePolityCapacityGrowthPerCentury = 0.12;
  double industrialPolityCapacityGrowthPerCentury = 0.35;
  Year successorMaturationYears = 150;
  double successorFragmentationMultiplier = 0.2;
  size_t smallPolityProtectionSize = 4;
  double smallPolityFragmentationMultiplier = 0.05;
  double overCapacityExpansionPenalty = 0.75;
  Year polityStabilizationStartYear = 1500;
  double postStabilizationFragmentationMultiplier = 0.35;
  double decayStrengthRatio = 0.35;
  double decayFragmentationChance = 0.20;
  double religionEmergenceChance = 0.03;
  Year maritimeExpansionStartYear = -1000;
  double maritimeExpansionChance = 0.06;
  double initialMaritimeRange = 150.0;
  double maritimeRangeGrowthPerCentury = 0.08;
  double maritimeColonizationMultiplier = 0.7;
  double maritimeConquestMultiplier = 0.45;
  double religionSpreadChance = 0.10;
  double religionSplitChance = 0.01;
  double cultureAssimilationChance = 0.03;
  double cultureSplitChance = 0.01;
  double minimumPopulation = 1.0;
  double defaultPopulation = 100.0;
  double defaultDevelopment = 0.01;
};

struct ArtifactPaths {
  std::filesystem::path eventLog;
  std::vector<std::filesystem::path> ownershipMaps;
  std::filesystem::path developmentLog;
  std::vector<std::filesystem::path> developmentMaps;
  std::filesystem::path populationLog;
  std::vector<std::filesystem::path> populationMaps;
  std::filesystem::path cultureLog;
  std::vector<std::filesystem::path> cultureMaps;
  std::filesystem::path religionLog;
  std::vector<std::filesystem::path> religionMaps;
  std::filesystem::path superRegionLog;
};

struct Input {
  std::vector<std::shared_ptr<ArdaContinent>> continents;
  const Fwg::Climate::ClimateData *climateData = nullptr;
  const Fwg::Terrain::TerrainData *terrainData = nullptr;
};

enum class RegionalPhase { Bust = -1, Neutral = 0, Boom = 1 };

enum class SuperRegionPhase { Lagging = -1, Neutral = 0, Booming = 1 };

enum class EventType {
  InitializeProvince,
  CreatePolity,
  CreateSuccessorPolity,
  TransferProvince,
  DissolvePolity,
  CreateCulture,
  SetCulture,
  CreateReligion,
  SetReligion,
  UpdatePopulation,
  UpdatePolityStrength,
  UpdateDevelopment,
  UpdateCarryingCapacity,
  MigratePopulation,
  SetRegionalPhase,
  SetSuperRegion,
  CreateSuperRegion,
  SetSuperRegionPhase,
  MigrateCulturePopulation,
  ConvertCulturePopulation,
  ColonizeProvince,
  ConsolidateRegion
};

struct Event {
  Year year = StartYear;
  EventType type = EventType::InitializeProvince;
  ProvinceId provinceId = -1;
  RegionId regionId = -1;
  PolityId polityId = NoPolity;
  PolityId previousPolityId = NoPolity;
  CultureId cultureId = -1;
  ReligionId religionId = NoReligion;
  double value = 0.0;
  double secondaryValue = 0.0;
  std::string description;
  int parentId = -1;
  Fwg::Gfx::Colour colour;
  double score = 0.0;
};

struct Polity {
  PolityId id = NoPolity;
  Year foundedYear = StartYear;
  std::optional<Year> dissolvedYear;
  std::optional<PolityId> predecessorId;
  std::vector<PolityId> successorIds;
  bool isTribe = true;
  Fwg::Gfx::Colour colour;
};

struct CultureLineage {
  CultureId id = -1;
  ProvinceId originProvinceId = -1;
  Year foundedYear = StartYear;
  std::optional<CultureId> parentId;
  Fwg::Gfx::Colour colour;
};

struct ReligionLineage {
  ReligionId id = NoReligion;
  ProvinceId originProvinceId = -1;
  Year foundedYear = StartYear;
  std::optional<ReligionId> parentId;
  Fwg::Gfx::Colour colour;
};

struct ProvinceState {
  PolityId owner = NoPolity;
  CultureId culture = -1;
  ReligionId religion = NoReligion;
  double population = 0.0;
  double development = 0.0;
  double carryingCapacity = 0.0;
  std::map<CultureId, double> culturePopulations;
};

struct DevelopmentSuperRegion {
  SuperRegionId id = -1;
  int continentId = -1;
  std::vector<RegionId> regions;
  SuperRegionPhase phase = SuperRegionPhase::Neutral;
  double development = 0.0;
  std::vector<SuperRegionId> neighbours;
};

struct PolityStrength {
  double population = 0.0;
  double development = 0.0;
  double score = 0.0;
};

struct State {
  Year year = StartYear;
  std::map<ProvinceId, ProvinceState> provinces;
  std::map<PolityId, Polity> polities;
  std::map<CultureId, CultureLineage> cultures;
  std::map<ReligionId, ReligionLineage> religions;
  std::map<RegionId, RegionalPhase> regionalPhases;
  std::map<RegionId, SuperRegionId> regionSuperRegions;
  std::map<SuperRegionId, DevelopmentSuperRegion> superRegions;
  std::map<PolityId, PolityStrength> polityStrengths;

  [[nodiscard]] const ProvinceState *findProvince(ProvinceId provinceId) const;
  [[nodiscard]] std::vector<ProvinceId> territoryOf(PolityId polityId) const;
  [[nodiscard]] CultureId dominantCultureOf(ProvinceId provinceId) const;
};

struct ValidationError {
  Year year = StartYear;
  std::string message;
  std::optional<ProvinceId> provinceId;
  std::optional<RegionId> regionId;
};

struct Result {
  std::vector<Event> events;
  std::map<EventType, std::size_t> eventCounts;
  std::vector<ValidationError> errors;
  State finalState;

  [[nodiscard]] bool succeeded() const { return errors.empty(); }
};

// Events are an in-memory format; introduce an explicit version before
// persisting them across application versions.
class HistorySimulation {
public:
  explicit HistorySimulation(Configuration configuration = {});

  [[nodiscard]] Result run(const Input &input);
  [[nodiscard]] State reconstruct(const std::vector<Event> &events,
                                  Year year) const;
  [[nodiscard]] std::vector<ValidationError>
  validate(const State &state, Year year, bool requireWholeRegions) const;
  [[nodiscard]] std::optional<ArtifactPaths>
  writeArtifacts(const Input &input, const Result &result,
                 const Fwg::Gfx::Image &baseMap,
                 const std::filesystem::path &outputDirectory,
                 std::vector<ValidationError> &errors) const;

  State getFinalState() const { return finalState; }
  std::map<ProvinceId, RegionId> getProvinceRegions() const {
    return provinceRegions;
  }
  std::map<ProvinceId, std::vector<ProvinceId>> getProvinceNeighbours() const {
    return provinceNeighbours;
  }

private:
  Configuration configuration;
  std::map<ProvinceId, RegionId> provinceRegions;
  std::map<ProvinceId, std::vector<ProvinceId>> provinceNeighbours;
  State finalState;
};

struct SimulationProvinceExport {
  ProvinceId id = -1;
  RegionId regionId = -1;
  PolityId polityId = NoPolity;
  CultureId cultureId = -1;
  ReligionId religionId = NoReligion;
  double population = 0.0;
  double normalizedPopulation = 0.0;
  double development = 0.0;
  double normalizedDevelopment = 0.0;
};

struct SimulationPolityExport {
  Polity polity;
  PolityStrength strength;
  std::vector<ProvinceId> provinceIds;
  std::vector<RegionId> regionIds;
};

struct SimulationPolityPeakExport {
  std::vector<ProvinceId> provinceIds;
  std::vector<RegionId> regionIds;
};

struct SimulationExport {
  Year year = StartYear;
  std::map<ProvinceId, SimulationProvinceExport> provinces;
  std::map<PolityId, SimulationPolityExport> polities;
  std::map<CultureId, CultureLineage> cultures;
  std::map<ReligionId, ReligionLineage> religions;
  std::map<RegionId, std::vector<ProvinceId>> regions;
  std::map<RegionId, std::vector<PolityId>> historicalRegionOwners;
  std::map<PolityId, SimulationPolityPeakExport> historicalPolityPeaks;
};

} // namespace Arda::Simulation