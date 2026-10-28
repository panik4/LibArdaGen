#pragma once

#include "areas/ArdaContinent.h"
#include "areas/Claim.h"
#include "climate/ClimateData.h"
#include "entities/Colour.h"
#include "terrain/TerrainData.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
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
inline constexpr Year TargetYear = 1936;
inline constexpr PolityId NoPolity = -1;
inline constexpr ReligionId NoReligion = -1;

inline constexpr std::size_t PoliticalBlocCount = 4;
using PoliticalPowerDistribution = std::array<double, PoliticalBlocCount>;

// The global RandNum sequence controls reproducibility. This primitive model is
// intentionally independent of Country, Culture, CultureGroup, and Religion;
// consumers materialize those domain objects from a reconstructed State.
struct Configuration {
  Year startYear = StartYear;
  Year regionOwnershipYear = 1700;
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
  Year ideologicalTransitionStartYear = 1850;
  int targetEndPolityCount = 100;
  int targetDevelopmentSuperRegionCount = 10;
  int superRegionCycleYears = 500;
  Year persistentDevelopmentStartYear = 1000;
  Year colonizationStartYear = 1450;
  Year polityAggressionStartYear = -3000;
  double superRegionNeighbourInfluence = 0.35;
  double culturalIntegrationRatePerCentury = 0.25;
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
  double aggressiveExpansionMultiplier = 2.0;
  double aggressiveFragmentationMultiplier = 2.5;
  double splitOffChance = 0.12;
  int maximumImplosionSuccessors = 4;
  size_t initialPolityCapacity = 4;
  double ancientPolityCapacityGrowthPerCentury = 0.04;
  double renaissancePolityCapacityGrowthPerCentury = 0.12;
  double industrialPolityCapacityGrowthPerCentury = 0.35;
  Year successorMaturationYears = 150;
  double successorFragmentationMultiplier = 0.2;
  size_t smallPolityProtectionSize = 4;
  double smallPolityFragmentationMultiplier = 0.05;
  double overCapacityExpansionPenalty = 0.25;
  Year polityStabilizationStartYear = 1500;
  double postStabilizationFragmentationMultiplier = 0.35;
  double decayFragmentationChance = 0.001;
  size_t lateSmallPolitySize = 2;
  double lateSmallPolityConquestMultiplier = 3.0;
  double religionEmergenceChance = 0.005;
  Year maritimeExpansionStartYear = -1000;
  double maritimeExpansionChance = 0.14;
  double maritimeRangeGrowthPerCentury = 0.12;
  double maritimeColonizationMultiplier = 1.5;
  double maritimeConquestMultiplier = 0.9;
  double islandPolityMaritimeMultiplier = 5.0;
  double landlockedPolityMaritimeMultiplier = 3.0;
  double warChancePerCentury = 0.25;
  double preIndustrialWarChanceMultiplier = 2.0;
  double initialLandWarRange = 100.0;
  double landWarRangeGrowthPerCentury = 0.12;
  double warMaximumDistance = 1200.0;
  double warAllianceDistanceMultiplier = 0.5;
  size_t maximumWarAllianceMembers = 3;
  size_t maximumWarMaritimeTransfers = 2;
  double religionSpreadChance = 0.35;
  double religionConversionChance = 0.40;
  double religionSplitChance = 0.0005;
  double cultureAssimilationChance = 0.35;
  double cultureSplitChance = 0.001;
  double primaryCultureChangeChance = 0.0001;
  double overseasCultureAdoptionChance = 0.01;
  double minimumPopulation = 1.0;
  double defaultPopulation = 100.0;
  double defaultDevelopment = 0.01;
  double initialPolityStability = 0.5;
  double stabilityMean = 0.7;
  double stabilityMeanReversionPerCentury = 0.5;
  double stabilityVolatilityPerCentury = 0.12;
  double stabilityWarVictoryImpact = 0.20;
  double stabilityWarDefeatImpact = -0.15;
  double stabilityRandomEventChancePerCentury = 0.45;
  double stabilityRandomEventMinimumImpact = 0.05;
  double stabilityRandomEventMaximumImpact = 0.35;
  double successorPoliticalInheritance = 0.7;
  double governmentTransitionChancePerCentury = 0.20;
  Year governmentTransitionCooldownYears = 150;
  Year modernGovernmentTransitionCooldownYears = 25;
  double governmentTransitionRandomness = 0.35;
  Year tribalGovernmentEndYear = 1000;
  Year tradeRepublicEndYear = 1800;
  double tribalGovernmentWeight = 0.15;
  double kingdomGovernmentWeight = 1.0;
  Year kingdomWeightReductionStartYear = 1600;
  Year kingdomWeightReductionEndYear = 1936;
  double lateKingdomGovernmentWeight = 0.25;
  double tradeRepublicGovernmentWeight = 0.8;
  double theocracyGovernmentWeight = 0.05;
  Year theocracyEndYear = 1800;
  double absolutismGovernmentWeight = 0.9;
  double constitutionalMonarchyGovernmentWeight = 1.0;
  double democracyGovernmentWeight = 1.0;
  double communismGovernmentWeight = 0.8;
  double fascismGovernmentWeight = 0.8;
  double centralizationMaximum = 0.9;
  double centralizationDevelopmentScale = 1.0;
  double centralizationConvergencePerCentury = 0.08;
  double centralizationVariation = 0.025;
  double centralizationAffinityMinimum = 0.75;
  double centralizationAffinityMaximum = 1.25;
  double tradeRepublicChancePerCentury = 0.20;
  double theocracyChancePerCentury = 0.12;
  double absolutismChancePerCentury = 0.18;
  double constitutionalMonarchyChancePerCentury = 0.16;
  double ideologicalTransitionChancePerCentury = 0.12;
  Year theocracyStartYear = -2000;
  Year absolutismStartYear = 1600;
  Year constitutionalMonarchyStartYear = 1800;
  Year democracyStartYear = 1800;
  Year communismStartYear = 1880;
  Year fascismStartYear = 1880;
  double democracyChancePerCentury = 0.14;
  double communismChancePerCentury = 0.12;
  double fascismChancePerCentury = 0.12;
  double modernGovernmentDevelopmentThreshold = 1.0;
  double modernGovernmentDevelopmentScale = 1.0;
  double modernGovernmentMinimumPressure = 0.25;
  double postIndustrialModernGovernmentMultiplier = 1.75;
  Year postIndustrialModernGovernmentStartYear = 1880;
  PoliticalPowerDistribution politicalPowerPersistence{0.55, 0.55, 0.55, 0.55};
  double politicalPowerConvergencePerCentury = 0.35;
  double politicalPowerVariation = 0.04;
  double militaryInfluencePersistence = 0.55;
  double militaryInfluenceConvergencePerCentury = 0.30;
  double militaryInfluenceVariation = 0.04;
  double partyControlPersistence = 0.55;
  double partyControlConvergencePerCentury = 0.30;
  double modernPartyControlConvergenceMultiplier = 4.0;
  double modernGovernmentPopularityDriftMultiplier = 8.0;
  double partyControlVariation = 0.04;
  double militaryInfluenceWarVictoryPressure = 0.12;
  double militaryInfluenceWarDefeatPressure = 0.08;
  double partyControlModernPressure = 0.10;
  double militaryGovernmentThreshold = 0.65;
  double partyStateThreshold = 0.65;
  double governmentPopularityDriftPerCentury = 0.18;
  double governmentPopularityVariation = 0.025;
  double governmentPopularityHysteresis = 0.04;
  double governmentTraitAffinityWeight = 2.0;
  double governmentExplicitMovementAffinity = 1.5;
  double democracyDevelopmentTarget = 0.20;
  double communismDevelopmentTarget = 0.20;
  double fascismDevelopmentTarget = 0.20;
  double governmentDirectionFeedbackStrength = 1.0;
  double governmentDirectionMinimumMultiplier = 0.5;
  double governmentDirectionMaximumMultiplier = 1.5;
  double preModernMonarchyTargetMultiplier = 1.0;
  double modernMonarchyTargetMultiplier = 0.75;
  double preModernTheocracyTargetMultiplier = 1.0;
  double modernTheocracyTargetMultiplier = 0.75;
  double modernDemocracyTargetMultiplier = 1.0;
  double modernCommunismTargetMultiplier = 1.0;
  double modernFascismTargetMultiplier = 1.0;
  Year coreOwnershipYears = 100;
  Year industrialCoreOwnershipYears = 50;
  Year modernCoreOwnershipYears = 30;
  Year coreClaimYears = 100;
  double directDemocracyPeopleThreshold = 0.72;
  double directDemocracyLeadThreshold = 0.20;
  double modernGovernmentMinimumPopularity = 0.0;
  double politicalGenericShockChancePerCentury = 0.12;
  double politicalGenericShockStrength = 0.25;
  Year politicalPressureDurationYears = 75;
  double politicalGovernmentHysteresis = 0.08;
  double eraTraitChance = 0.8;
  double randomTraitShockChancePerCentury = 0.015;
  double warLossTraitChancePerCentury = 0.04;
  double olderEraTraitMultiplier = 0.15;
  double traitInitialStrength = 0.5;
  double traitShockStrength = 0.45;
  double traitAcquisitionPressure = 1.0;
  double traitRemovalThreshold = 0.05;
  double traitStrengthConvergencePerCentury = 0.20;
  // Detailed route and settlement diagnostics; yearly progress stays enabled.
  bool enableWarfareDiagnostics = false;
};

struct SimulationClaimExport {
  PolityId polityId = NoPolity;
  RegionId regionId = -1;
  ClaimSource source = ClaimSource::HistoricalOwnership;
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
  std::filesystem::path polityStateLog;
};

enum class PoliticalEra {
  AncientClassical,
  Medieval,
  EarlyModern,
  EnlightenmentRevolutionary,
  Industrial,
  Modern
};

struct Input {
  std::vector<std::shared_ptr<ArdaContinent>> continents;
  const Fwg::Climate::ClimateData *climateData = nullptr;
  const Fwg::Terrain::TerrainData *terrainData = nullptr;
};

struct SeaRoute {
  ProvinceId provinceId = -1;
  double distance = 0.0;
};

using SeaRouteMap = std::map<ProvinceId, std::vector<SeaRoute>>;

enum class RegionalPhase { Bust = -1, Neutral = 0, Boom = 1 };

enum class SuperRegionPhase { Lagging = -1, Neutral = 0, Booming = 1 };

enum class GovernmentForm {
  Tribal,
  AbsoluteMonarchy,
  FeudalMonarchy,
  ConstitutionalMonarchy,
  ParliamentaryMonarchy,
  AristocraticMonarchy,
  ElectiveMonarchy,
  TheocraticMonarchy,
  Theocracy,
  AristocraticRepublic,
  OligarchicRepublic,
  ConstitutionalRepublic,
  ParliamentaryRepublic,
  DirectDemocracy,
  MilitaryGovernment,
  CommunistState,
  FascistState
};
inline constexpr std::size_t GovernmentFormCount =
    static_cast<std::size_t>(GovernmentForm::FascistState) + 1;

enum class PoliticalDirection { Other, Democracy, Communism, Fascism };

constexpr PoliticalDirection politicalDirection(GovernmentForm form) {
  switch (form) {
  case GovernmentForm::ConstitutionalMonarchy:
  case GovernmentForm::ParliamentaryMonarchy:
  case GovernmentForm::ConstitutionalRepublic:
  case GovernmentForm::ParliamentaryRepublic:
  case GovernmentForm::DirectDemocracy:
    return PoliticalDirection::Democracy;
  case GovernmentForm::CommunistState:
    return PoliticalDirection::Communism;
  case GovernmentForm::FascistState:
    return PoliticalDirection::Fascism;
  default:
    return PoliticalDirection::Other;
  }
}

enum class PoliticalBloc {
  CentralAuthority,
  Nobility,
  ReligiousInstitutions,
  People
};

enum class PoliticalTrait {
  DivineRulership,
  AristocraticRepublic,
  CivicTradition,
  PriestlyAuthority,
  MilitaryStrongman,
  LocalAutonomy,
  FeudalOrder,
  PowerfulCrown,
  EcclesiasticalAuthority,
  FragmentedRealm,
  MerchantPrivileges,
  CommunalTraditions,
  WarriorAristocracy,
  ImperialBureaucracy,
  RoyalAbsolutism,
  CentralizedAdministration,
  StrongEstates,
  ReligiousEstablishment,
  CommercialElite,
  ColonialAdministration,
  EnlightenedReform,
  MilitaryState,
  ConstitutionalTradition,
  EnlightenedAbsolutism,
  RepublicanTradition,
  AristocraticResistance,
  Secularization,
  RevolutionarySentiment,
  NationalAwakening,
  ReactionaryOrder,
  ConstitutionalMonarchy,
  LiberalReform,
  ParliamentaryTradition,
  ConservativeEstablishment,
  Nationalism,
  SocialReform,
  SocialistMovement,
  RevolutionaryMovement,
  AuthoritarianModernization,
  LiberalDemocracy,
  SocialDemocracy,
  CommunistMovement,
  FascistMovement,
  MilitaryAuthoritarianism,
  OnePartyState,
  ReligiousConservatism,
  PopulistMovement,
  TechnocraticGovernment
};

inline constexpr std::size_t PoliticalTraitCount =
    static_cast<std::size_t>(PoliticalTrait::TechnocraticGovernment) + 1;

struct PoliticalTraitDefinition {
  PoliticalTrait trait = PoliticalTrait::DivineRulership;
  PoliticalEra era = PoliticalEra::AncientClassical;
  PoliticalPowerDistribution powerModifier{};
  double militaryInfluenceModifier = 0.0;
  double partyControlModifier = 0.0;
};

struct PoliticalTraitState {
  PoliticalTrait trait = PoliticalTrait::DivineRulership;
  double strength = 1.0;
  Year acquiredYear = StartYear;
  std::optional<Year> expiryYear;
};

enum class PoliticalPressureType {
  WarVictory,
  WarDefeat,
  Conquest,
  Fragmentation,
  ReligiousChange,
  Development,
  GenericShock
};

struct PoliticalPressureState {
  PoliticalPressureType type = PoliticalPressureType::GenericShock;
  PoliticalPowerDistribution effect{};
  double strength = 0.0;
  Year acquiredYear = StartYear;
  std::optional<Year> expiryYear;
};

enum class EventType {
  InitializeProvince,
  CreatePolity,
  CreateSuccessorPolity,
  TransferProvince,
  DissolvePolity,
  CreateCulture,
  SetCulture,
  SetPrimaryCulture,
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
  ConsolidateRegion,
  SetCapital,
  PolityUpdate
};

// Transient producer input. Retained history uses compact EventStore payloads.
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
  bool coastal = false;
  bool island = false;
  int warId = -1;
  bool overseas = false;
  bool colony = false;
  int landMassID = -1;
  int continentId = -1;
  double armyTradition = 0.0;
  double navyTradition = 0.0;
  double stability = 0.5;
  double centralization = 0.0;
  GovernmentForm governmentForm = GovernmentForm::Tribal;
  double centralizationAffinity = 1.0;
  Year lastGovernmentTransitionYear = StartYear;
  PoliticalPowerDistribution politicalPower{0.25, 0.25, 0.25, 0.25};
  PoliticalPowerDistribution desiredPoliticalPower{0.25, 0.25, 0.25, 0.25};
  std::vector<PoliticalTraitState> politicalTraits;
  std::vector<PoliticalPressureState> politicalPressures;
  std::array<double, GovernmentFormCount> governmentPopularity{};
  PoliticalEra lastTraitEra = PoliticalEra::AncientClassical;
  double militaryInfluence = 0.0;
  double desiredMilitaryInfluence = 0.0;
  double partyControl = 0.0;
  double desiredPartyControl = 0.0;
  std::map<int, double> politicalMovements;
};

struct EventHeader {
  Year year;
  EventType type;
  size_t payloadIndex;
  uint32_t descriptionId;
};

template <EventType Type> struct EventPayload;

template <> struct EventPayload<EventType::InitializeProvince> {
  ProvinceId provinceId = -1;
  RegionId regionId = -1;
  PolityId polityId = NoPolity;
  CultureId cultureId = -1;
  ReligionId religionId = NoReligion;
  double value = 0.0;
  double secondaryValue = 0.0;
  Fwg::Gfx::Colour colour;
  bool coastal = false;
  bool island = false;
  bool overseas = false;
  bool colony = false;
  int landMassID = -1;
  int continentId = -1;
};

template <> struct EventPayload<EventType::CreatePolity> {
  PolityId polityId = NoPolity;
  int parentId = -1;
  Fwg::Gfx::Colour colour;
  double armyTradition = 0.0;
  double navyTradition = 0.0;
  double stability = 0.5;
  double centralization = 0.0;
  GovernmentForm governmentForm = GovernmentForm::Tribal;
  double centralizationAffinity = 1.0;
  Year lastGovernmentTransitionYear = StartYear;
  PoliticalPowerDistribution politicalPower{0.25, 0.25, 0.25, 0.25};
  PoliticalPowerDistribution desiredPoliticalPower{0.25, 0.25, 0.25, 0.25};
  std::vector<PoliticalTraitState> politicalTraits;
  std::vector<PoliticalPressureState> politicalPressures;
  std::array<double, GovernmentFormCount> governmentPopularity{};
  PoliticalEra lastTraitEra = PoliticalEra::AncientClassical;
  double militaryInfluence = 0.0;
  double desiredMilitaryInfluence = 0.0;
  double partyControl = 0.0;
  double desiredPartyControl = 0.0;
  std::map<int, double> politicalMovements;
};

template <> struct EventPayload<EventType::CreateSuccessorPolity> {
  PolityId polityId = NoPolity;
  int parentId = -1;
  Fwg::Gfx::Colour colour;
  double armyTradition = 0.0;
  double navyTradition = 0.0;
  double stability = 0.5;
  double centralization = 0.0;
  GovernmentForm governmentForm = GovernmentForm::Tribal;
  double centralizationAffinity = 1.0;
  Year lastGovernmentTransitionYear = StartYear;
  PoliticalPowerDistribution politicalPower{0.25, 0.25, 0.25, 0.25};
  PoliticalPowerDistribution desiredPoliticalPower{0.25, 0.25, 0.25, 0.25};
  std::vector<PoliticalTraitState> politicalTraits;
  std::vector<PoliticalPressureState> politicalPressures;
  std::array<double, GovernmentFormCount> governmentPopularity{};
  PoliticalEra lastTraitEra = PoliticalEra::AncientClassical;
  double militaryInfluence = 0.0;
  double desiredMilitaryInfluence = 0.0;
  double partyControl = 0.0;
  double desiredPartyControl = 0.0;
  std::map<int, double> politicalMovements;
};

template <> struct EventPayload<EventType::TransferProvince> {
  ProvinceId provinceId = -1;
  RegionId regionId = -1;
  PolityId polityId = NoPolity;
  PolityId previousPolityId = NoPolity;
  int warId = -1;
  bool overseas = false;
  bool colony = false;
};

template <> struct EventPayload<EventType::DissolvePolity> {
  PolityId polityId = NoPolity;
};

template <> struct EventPayload<EventType::CreateCulture> {
  ProvinceId provinceId = -1;
  CultureId cultureId = -1;
  int parentId = -1;
  Fwg::Gfx::Colour colour;
};

template <> struct EventPayload<EventType::SetCulture> {
  ProvinceId provinceId = -1;
  CultureId cultureId = -1;
};

template <> struct EventPayload<EventType::SetPrimaryCulture> {
  PolityId polityId = NoPolity;
  CultureId cultureId = -1;
};

template <> struct EventPayload<EventType::CreateReligion> {
  ProvinceId provinceId = -1;
  ReligionId religionId = NoReligion;
  int parentId = -1;
  Fwg::Gfx::Colour colour;
};

template <> struct EventPayload<EventType::SetReligion> {
  ProvinceId provinceId = -1;
  ReligionId religionId = NoReligion;
};

template <> struct EventPayload<EventType::UpdatePopulation> {
  ProvinceId provinceId = -1;
  double value = 0.0;
};

template <> struct EventPayload<EventType::UpdatePolityStrength> {
  PolityId polityId = NoPolity;
  double value = 0.0;
  double secondaryValue = 0.0;
  double score = 0.0;
};

template <> struct EventPayload<EventType::UpdateDevelopment> {
  ProvinceId provinceId = -1;
  double value = 0.0;
};

template <> struct EventPayload<EventType::UpdateCarryingCapacity> {
  ProvinceId provinceId = -1;
  RegionId regionId = -1;
  double value = 0.0;
  double secondaryValue = 0.0;
};

template <> struct EventPayload<EventType::MigratePopulation> {
  ProvinceId provinceId = -1;
  RegionId regionId = -1;
  double value = 0.0;
};

template <> struct EventPayload<EventType::SetRegionalPhase> {
  RegionId regionId = -1;
  double value = 0.0;
};

template <> struct EventPayload<EventType::SetSuperRegion> {
  RegionId regionId = -1;
  double value = 0.0;
};

template <> struct EventPayload<EventType::CreateSuperRegion> {
  RegionId regionId = -1;
  PolityId polityId = NoPolity;
};

template <> struct EventPayload<EventType::SetSuperRegionPhase> {
  RegionId regionId = -1;
  double value = 0.0;
};

template <> struct EventPayload<EventType::MigrateCulturePopulation> {
  ProvinceId provinceId = -1;
  RegionId regionId = -1;
  CultureId cultureId = -1;
  double value = 0.0;
};

template <> struct EventPayload<EventType::ConvertCulturePopulation> {
  ProvinceId provinceId = -1;
  CultureId cultureId = -1;
  int parentId = -1;
  double value = 0.0;
};

template <> struct EventPayload<EventType::ColonizeProvince> {
  ProvinceId provinceId = -1;
  RegionId regionId = -1;
  PolityId polityId = NoPolity;
  PolityId previousPolityId = NoPolity;
  int warId = -1;
  bool overseas = false;
  bool colony = false;
};

template <> struct EventPayload<EventType::ConsolidateRegion> {
  ProvinceId provinceId = -1;
  RegionId regionId = -1;
  PolityId polityId = NoPolity;
  PolityId previousPolityId = NoPolity;
  int warId = -1;
  bool overseas = false;
  bool colony = false;
};

template <> struct EventPayload<EventType::SetCapital> {
  ProvinceId provinceId = -1;
  PolityId polityId = NoPolity;
};

template <> struct EventPayload<EventType::PolityUpdate> {
  PolityId polityId = NoPolity;
  double armyTradition = 0.0;
  double navyTradition = 0.0;
  double stability = 0.5;
  double centralization = 0.0;
  GovernmentForm governmentForm = GovernmentForm::Tribal;
  double centralizationAffinity = 1.0;
  Year lastGovernmentTransitionYear = StartYear;
  PoliticalPowerDistribution politicalPower{0.25, 0.25, 0.25, 0.25};
  PoliticalPowerDistribution desiredPoliticalPower{0.25, 0.25, 0.25, 0.25};
  std::vector<PoliticalTraitState> politicalTraits;
  std::vector<PoliticalPressureState> politicalPressures;
  std::array<double, GovernmentFormCount> governmentPopularity{};
  PoliticalEra lastTraitEra = PoliticalEra::AncientClassical;
  double militaryInfluence = 0.0;
  double desiredMilitaryInfluence = 0.0;
  double partyControl = 0.0;
  double desiredPartyControl = 0.0;
  std::map<int, double> politicalMovements;
};

class EventStore {
public:
  size_t append(Event &&event);
  [[nodiscard]] size_t size() const { return chronology.size(); }
  [[nodiscard]] bool empty() const { return chronology.empty(); }
  [[nodiscard]] const EventHeader &header(size_t index) const {
    return chronology.at(index);
  }
  [[nodiscard]] std::string_view description(uint32_t id) const {
    return descriptions.at(id);
  }

  template <class Visitor>
  decltype(auto) visit(size_t index, Visitor &&visitor) const {
    const auto &entry = header(index);
    switch (entry.type) {
    case EventType::InitializeProvince:
      return std::forward<Visitor>(visitor)(
          entry, recordsInitializeProvince.at(entry.payloadIndex));
    case EventType::CreatePolity:
      return std::forward<Visitor>(visitor)(
          entry, recordsCreatePolity.at(entry.payloadIndex));
    case EventType::CreateSuccessorPolity:
      return std::forward<Visitor>(visitor)(
          entry, recordsCreateSuccessorPolity.at(entry.payloadIndex));
    case EventType::TransferProvince:
      return std::forward<Visitor>(visitor)(
          entry, recordsTransferProvince.at(entry.payloadIndex));
    case EventType::DissolvePolity:
      return std::forward<Visitor>(visitor)(
          entry, recordsDissolvePolity.at(entry.payloadIndex));
    case EventType::CreateCulture:
      return std::forward<Visitor>(visitor)(
          entry, recordsCreateCulture.at(entry.payloadIndex));
    case EventType::SetCulture:
      return std::forward<Visitor>(visitor)(
          entry, recordsSetCulture.at(entry.payloadIndex));
    case EventType::SetPrimaryCulture:
      return std::forward<Visitor>(visitor)(
          entry, recordsSetPrimaryCulture.at(entry.payloadIndex));
    case EventType::CreateReligion:
      return std::forward<Visitor>(visitor)(
          entry, recordsCreateReligion.at(entry.payloadIndex));
    case EventType::SetReligion:
      return std::forward<Visitor>(visitor)(
          entry, recordsSetReligion.at(entry.payloadIndex));
    case EventType::UpdatePopulation:
      return std::forward<Visitor>(visitor)(
          entry, recordsUpdatePopulation.at(entry.payloadIndex));
    case EventType::UpdatePolityStrength:
      return std::forward<Visitor>(visitor)(
          entry, recordsUpdatePolityStrength.at(entry.payloadIndex));
    case EventType::UpdateDevelopment:
      return std::forward<Visitor>(visitor)(
          entry, recordsUpdateDevelopment.at(entry.payloadIndex));
    case EventType::UpdateCarryingCapacity:
      return std::forward<Visitor>(visitor)(
          entry, recordsUpdateCarryingCapacity.at(entry.payloadIndex));
    case EventType::MigratePopulation:
      return std::forward<Visitor>(visitor)(
          entry, recordsMigratePopulation.at(entry.payloadIndex));
    case EventType::SetRegionalPhase:
      return std::forward<Visitor>(visitor)(
          entry, recordsSetRegionalPhase.at(entry.payloadIndex));
    case EventType::SetSuperRegion:
      return std::forward<Visitor>(visitor)(
          entry, recordsSetSuperRegion.at(entry.payloadIndex));
    case EventType::CreateSuperRegion:
      return std::forward<Visitor>(visitor)(
          entry, recordsCreateSuperRegion.at(entry.payloadIndex));
    case EventType::SetSuperRegionPhase:
      return std::forward<Visitor>(visitor)(
          entry, recordsSetSuperRegionPhase.at(entry.payloadIndex));
    case EventType::MigrateCulturePopulation:
      return std::forward<Visitor>(visitor)(
          entry, recordsMigrateCulturePopulation.at(entry.payloadIndex));
    case EventType::ConvertCulturePopulation:
      return std::forward<Visitor>(visitor)(
          entry, recordsConvertCulturePopulation.at(entry.payloadIndex));
    case EventType::ColonizeProvince:
      return std::forward<Visitor>(visitor)(
          entry, recordsColonizeProvince.at(entry.payloadIndex));
    case EventType::ConsolidateRegion:
      return std::forward<Visitor>(visitor)(
          entry, recordsConsolidateRegion.at(entry.payloadIndex));
    case EventType::SetCapital:
      return std::forward<Visitor>(visitor)(
          entry, recordsSetCapital.at(entry.payloadIndex));
    case EventType::PolityUpdate:
      return std::forward<Visitor>(visitor)(
          entry, recordsPolityUpdate.at(entry.payloadIndex));
    }
    throw std::invalid_argument("Unknown simulation event type");
  }
  template <class Visitor> void forEach(Visitor &&visitor) const {
    for (size_t index = 0; index < size(); ++index)
      visit(index, visitor);
  }
  [[nodiscard]] Event materialize(size_t index) const;
  [[nodiscard]] size_t bufferBytes() const;
  // Estimate includes nested allocations, but excludes allocator bookkeeping.
  [[nodiscard]] size_t dynamicBytesEstimate() const;

private:
  std::vector<EventHeader> chronology;
  std::vector<std::string> descriptions;
  std::unordered_map<std::string, uint32_t> descriptionIds;
  std::vector<EventPayload<EventType::InitializeProvince>>
      recordsInitializeProvince;
  std::vector<EventPayload<EventType::CreatePolity>> recordsCreatePolity;
  std::vector<EventPayload<EventType::CreateSuccessorPolity>>
      recordsCreateSuccessorPolity;
  std::vector<EventPayload<EventType::TransferProvince>>
      recordsTransferProvince;
  std::vector<EventPayload<EventType::DissolvePolity>> recordsDissolvePolity;
  std::vector<EventPayload<EventType::CreateCulture>> recordsCreateCulture;
  std::vector<EventPayload<EventType::SetCulture>> recordsSetCulture;
  std::vector<EventPayload<EventType::SetPrimaryCulture>>
      recordsSetPrimaryCulture;
  std::vector<EventPayload<EventType::CreateReligion>> recordsCreateReligion;
  std::vector<EventPayload<EventType::SetReligion>> recordsSetReligion;
  std::vector<EventPayload<EventType::UpdatePopulation>>
      recordsUpdatePopulation;
  std::vector<EventPayload<EventType::UpdatePolityStrength>>
      recordsUpdatePolityStrength;
  std::vector<EventPayload<EventType::UpdateDevelopment>>
      recordsUpdateDevelopment;
  std::vector<EventPayload<EventType::UpdateCarryingCapacity>>
      recordsUpdateCarryingCapacity;
  std::vector<EventPayload<EventType::MigratePopulation>>
      recordsMigratePopulation;
  std::vector<EventPayload<EventType::SetRegionalPhase>>
      recordsSetRegionalPhase;
  std::vector<EventPayload<EventType::SetSuperRegion>> recordsSetSuperRegion;
  std::vector<EventPayload<EventType::CreateSuperRegion>>
      recordsCreateSuperRegion;
  std::vector<EventPayload<EventType::SetSuperRegionPhase>>
      recordsSetSuperRegionPhase;
  std::vector<EventPayload<EventType::MigrateCulturePopulation>>
      recordsMigrateCulturePopulation;
  std::vector<EventPayload<EventType::ConvertCulturePopulation>>
      recordsConvertCulturePopulation;
  std::vector<EventPayload<EventType::ColonizeProvince>>
      recordsColonizeProvince;
  std::vector<EventPayload<EventType::ConsolidateRegion>>
      recordsConsolidateRegion;
  std::vector<EventPayload<EventType::SetCapital>> recordsSetCapital;
  std::vector<EventPayload<EventType::PolityUpdate>> recordsPolityUpdate;
};

struct Polity {
  bool initialized = false;
  PolityId id = NoPolity;
  Year foundedYear = StartYear;
  std::optional<Year> dissolvedYear;
  std::optional<PolityId> predecessorId;
  std::vector<PolityId> successorIds;
  bool isTribe = true;
  Fwg::Gfx::Colour colour;
  CultureId primaryCulture = -1;
  ProvinceId capitalProvince = -1;
  std::shared_ptr<ArdaProvince> capital;
  GovernmentForm governmentForm = GovernmentForm::Tribal;
  double centralization = 0.0;
  double centralizationAffinity = 1.0;
  Year lastGovernmentTransitionYear = StartYear;
  PoliticalPowerDistribution politicalPower{0.25, 0.25, 0.25, 0.25};
  PoliticalPowerDistribution desiredPoliticalPower{0.25, 0.25, 0.25, 0.25};
  std::vector<PoliticalTraitState> politicalTraits;
  std::vector<PoliticalPressureState> politicalPressures;
  std::array<double, GovernmentFormCount> governmentPopularity{};
  PoliticalEra lastTraitEra = PoliticalEra::AncientClassical;
  double militaryInfluence = 0.0;
  double desiredMilitaryInfluence = 0.0;
  double partyControl = 0.0;
  double desiredPartyControl = 0.0;
  double armyTradition = 0.0;
  double navyTradition = 0.0;
  double stability = 0.5;
  std::map<int, double> politicalMovements;
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
  bool initialized = false;
  PolityId owner = NoPolity;
  CultureId culture = -1;
  ReligionId religion = NoReligion;
  double population = 0.0;
  double development = 0.0; // Average development per province.
  double carryingCapacity = 0.0;
  std::map<CultureId, double> culturePopulations;
  int landMassID = -1;
  int continentId = -1;
  bool coastal = false;
  bool island = false;
  bool overseas = false;
  bool colony = false;
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

struct SimulationPoliticalDirectionStrength {
  double development = 0.0;
  double share = 0.0;
};

struct State {
  Year year = StartYear;
  std::vector<ProvinceState> provinces;
  std::vector<Polity> polities;
  std::map<CultureId, CultureLineage> cultures;
  std::map<ReligionId, ReligionLineage> religions;
  std::map<RegionId, RegionalPhase> regionalPhases;
  std::map<RegionId, SuperRegionId> regionSuperRegions;
  std::map<SuperRegionId, DevelopmentSuperRegion> superRegions;
  // Current owners have current totals; ownerless polities retain their last
  // recorded totals for historical export selection.
  std::map<PolityId, PolityStrength> polityStrengths;
  std::map<PolityId, std::set<ProvinceId>> coastalProvinceIds;
  std::map<ProvinceId, std::shared_ptr<ArdaProvince>> geography;

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

struct SimulationPolityHistory {
  std::map<PolityId, std::set<ProvinceId>> currentProvinceIds;
  std::map<PolityId, std::vector<ProvinceId>> peakProvinceIds;
  std::map<RegionId, std::set<PolityId>> historicalRegionOwners;
};

struct PolityTimelineSnapshot {
  Year year = StartYear;
  PolityId polityId = NoPolity;
  GovernmentForm governmentForm = GovernmentForm::Tribal;
  double centralization = 0.0;
  double armyTradition = 0.0;
  double navyTradition = 0.0;
  double stability = 0.5;
  std::map<int, double> politicalMovements;
  PoliticalPowerDistribution politicalPower{0.25, 0.25, 0.25, 0.25};
  PoliticalPowerDistribution desiredPoliticalPower{0.25, 0.25, 0.25, 0.25};
  std::vector<PoliticalTraitState> politicalTraits;
  std::vector<PoliticalPressureState> politicalPressures;
  std::array<double, GovernmentFormCount> governmentPopularity{};
  PoliticalEra lastTraitEra = PoliticalEra::AncientClassical;
  double militaryInfluence = 0.0;
  double desiredMilitaryInfluence = 0.0;
  double partyControl = 0.0;
  double desiredPartyControl = 0.0;
  double population = 0.0;
  double development = 0.0;
  double populationDensity = 0.0;
};

struct WarEvent {
  int id = -1;
  Year year = StartYear;
  std::vector<PolityId> attackers;
  std::vector<PolityId> defenders;
  std::vector<PolityId> winners;
  std::vector<PolityId> losers;
  std::vector<std::size_t> transferEventIndices;
};

struct GovernmentTransitionDiagnostics {
  std::size_t transitionChecks = 0;
  std::size_t transitionSuccesses = 0;
  std::size_t emptyCandidatePools = 0;
  std::size_t selectedTransitions = 0;
  std::size_t reversions = 0;
  std::array<std::size_t, GovernmentFormCount> checksBySource{};
  std::array<std::size_t, GovernmentFormCount> candidatePoolsBySource{};
  std::array<std::size_t, GovernmentFormCount> selectionsByDestination{};
};

struct Result {
  EventStore events;
  std::vector<WarEvent> wars;
  std::map<EventType, std::size_t> eventCounts;
  std::vector<ValidationError> errors;
  SimulationPolityHistory polityHistory;
  std::map<PolityId, std::vector<PolityTimelineSnapshot>> polityTimeline;
  GovernmentTransitionDiagnostics governmentTransitions;
  State finalState;

  [[nodiscard]] bool succeeded() const { return errors.empty(); }
};

// Events are an in-memory format; introduce an explicit version before
// persisting them across application versions.
class HistorySimulation {
public:
  explicit HistorySimulation(Configuration configuration = {});

  [[nodiscard]] Result runSimulation(const Input &input);
  [[nodiscard]] State reconstruct(const EventStore &events, Year year) const;
  [[nodiscard]] std::vector<ValidationError>
  validate(const State &state, Year year, bool requireWholeRegions) const;
  [[nodiscard]] std::optional<ArtifactPaths>
  writeArtifacts(const Input &input, const Result &result,
                 const Fwg::Gfx::Image &baseMap,
                 const std::filesystem::path &outputDirectory,
                 std::vector<ValidationError> &errors) const;

  State getFinalState() const { return finalState; }
  const Configuration &getConfiguration() const { return configuration; }
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
  bool overseas = false;
  bool colony = false;
};

struct SimulationPolityPeakExport {
  std::vector<ProvinceId> provinceIds;
  std::vector<RegionId> regionIds;
};

struct SimulationTerritorySetExport {
  std::vector<ProvinceId> provinceIds;
  std::vector<RegionId> regionIds;
};

struct SimulationPolityStatistics {
  size_t coreProvinceCount = 0;
  size_t nonCoreProvinceCount = 0;
  size_t claimProvinceCount = 0;
  size_t formableCount = 0;
  std::map<size_t, size_t> formableCountsByRequiredProvinceCount;
};

struct SimulationGovernmentFormStrength {
  size_t polityCount = 0;
  double population = 0.0;
  double development = 0.0;
  double score = 0.0;
};

struct SimulationPolityExport {
  Polity polity;
  PolityStrength strength;
  std::vector<ProvinceId> provinceIds;
  std::vector<RegionId> regionIds;
  SimulationPolityStatistics statistics;
};

struct SimulationFormableExport {
  PolityId historicalPolityId = NoPolity;
  std::vector<ProvinceId> requiredProvinceIds;
  std::vector<RegionId> requiredRegionIds;
  PolityId currentOwnerId = NoPolity;
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
  std::map<PolityId, SimulationTerritorySetExport> cores;
  std::map<PolityId, SimulationTerritorySetExport> claims;
  std::vector<SimulationClaimExport> claimRecords;
  std::map<PolityId, SimulationFormableExport> formables;
  std::map<GovernmentForm, SimulationGovernmentFormStrength>
      governmentFormStrength;
  std::map<PoliticalDirection, SimulationPoliticalDirectionStrength>
      politicalDirectionStrength;
  std::vector<WarEvent> wars;
};

} // namespace Arda::Simulation
