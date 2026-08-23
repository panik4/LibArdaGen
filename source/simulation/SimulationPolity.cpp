#include "simulation/SimulationPolity.h"

#include "RandNum.h"
#include "simulation/SimulationState.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace Arda::Simulation::polity {
namespace {

using detail::AppendEvent;
using detail::NormalizedInput;

bool chance(double probability) {
  return probability > 0.0 && RandNum::getRandom<double>(1.0) < probability;
}

struct PolityDevelopmentTotal {
  std::size_t provinceCount = 0;
  double development = 0.0;
};

std::vector<double>
calculateCentralizationTargets(const State &state,
                               const Configuration &configuration) {
  std::vector<PolityDevelopmentTotal> polityTotals(state.polities.size());
  for (const auto &province : state.provinces) {
    if (!province.initialized || province.owner < 0 ||
        province.owner >= static_cast<PolityId>(polityTotals.size()))
      continue;
    auto &totals = polityTotals[static_cast<size_t>(province.owner)];
    ++totals.provinceCount;
    totals.development += province.development;
  }

  std::vector<double> targets(polityTotals.size(), 0.0);
  const auto maximum =
      std::clamp(configuration.centralizationMaximum, 0.0, 1.0);
  for (size_t polityId = 0; polityId < polityTotals.size(); ++polityId) {
    const auto &totals = polityTotals[polityId];
    if (totals.provinceCount == 0)
      continue;
    const auto averageDevelopment =
        totals.development / static_cast<double>(totals.provinceCount);
    const auto *polity =
        state::findPolity(state, static_cast<PolityId>(polityId));
    const auto affinity =
        polity == nullptr ? 1.0 : polity->centralizationAffinity;
    targets[polityId] = std::clamp(
        3.0 * averageDevelopment * configuration.centralizationDevelopmentScale *
            maximum * affinity,
        0.0, maximum);
  }
  return targets;
}

double nextCentralization(double current, double target, double centuries,
                          const Configuration &configuration) {
  const auto maximum =
      std::clamp(configuration.centralizationMaximum, 0.0, 1.0);
  const auto convergence = std::clamp(
      1.0 - std::exp(-configuration.centralizationConvergencePerCentury *
                     std::max(centuries, 0.0)),
      0.0, 1.0);
  const auto variation = std::max(0.0, configuration.centralizationVariation);
  const auto noise = (RandNum::getRandom<double>(2.0) - 1.0) * variation *
                     std::abs(target - current);
  const auto deterministic = current + (target - current) * convergence;
  return std::clamp(deterministic + noise, 0.0, maximum);
}

GovernmentForm governmentFor(const Polity &polity, Year year,
                             const Configuration &configuration,
                             bool hasCoastalProvince,
                             double stability, double navyTradition,
                             double armyTradition, double development,
                             double warPressure, double centuries,
                             GovernmentTransitionDiagnostics &diagnostics) {
  if (polity.governmentForm == GovernmentForm::Tribal) {
    const auto form = year >= configuration.classicalStartYear
                          ? GovernmentForm::Kingdom
                          : GovernmentForm::Tribal;
    if (form != polity.governmentForm) {
      ++diagnostics.transitionSuccesses;
      ++diagnostics.selectedTransitions;
      ++diagnostics.selectionsByDestination[static_cast<std::size_t>(form)];
    }
    return form;
  }

  ++diagnostics.transitionChecks;
  const auto sourceIndex = static_cast<std::size_t>(polity.governmentForm);
  if (sourceIndex < diagnostics.checksBySource.size())
    ++diagnostics.checksBySource[sourceIndex];

  const auto elapsedYears = std::max(0, year - polity.lastGovernmentTransitionYear);
  const auto normalCooldown = std::max<Year>(
      1, configuration.governmentTransitionCooldownYears);
  const auto modernCooldown = std::max<Year>(
      1, configuration.modernGovernmentTransitionCooldownYears);
  const auto modernProgress = std::clamp(
      static_cast<double>(year - configuration.modernStartYear) / 100.0, 0.0,
      1.0);
  const auto cooldown = static_cast<double>(normalCooldown) +
                        (static_cast<double>(modernCooldown) - normalCooldown) *
                            modernProgress;
  const auto recencyPressure = std::clamp(
      static_cast<double>(elapsedYears) / cooldown, 0.0, 1.0);
  const auto stabilityVolatility =
      1.0 + 0.8 * std::abs(std::clamp(stability, 0.0, 1.0) - 0.5) * 2.0;
  const auto randomness = std::max(0.0, configuration.governmentTransitionRandomness);
  const auto randomFactor =
      1.0 + (RandNum::getRandom<double>(2.0) - 1.0) * randomness;
  const auto transitionHazard =
      std::max(0.0, configuration.governmentTransitionChancePerCentury) *
      recencyPressure * stabilityVolatility * randomFactor;
  if (!chance(1.0 - std::exp(-transitionHazard * std::max(centuries, 0.0))))
    return polity.governmentForm;
  ++diagnostics.transitionSuccesses;

  std::vector<std::pair<GovernmentForm, double>> candidates;
  const auto addCandidate = [&candidates](GovernmentForm form, double weight) {
    if (weight > 0.0)
      candidates.emplace_back(form, weight);
  };
  const auto developmentPressure = std::clamp(
      (development - configuration.modernGovernmentDevelopmentThreshold) *
          configuration.modernGovernmentDevelopmentScale,
      0.0, 1.0);
  const auto modernPressure = development <= 0.0
                                  ? 0.0
                                  : std::max(
                                        developmentPressure,
                                        std::clamp(configuration.modernGovernmentMinimumPressure,
                                                   0.0, 1.0));

  if (year < configuration.tribalGovernmentEndYear)
    addCandidate(GovernmentForm::Tribal,
                 configuration.tribalGovernmentWeight *
                     std::clamp((configuration.tribalGovernmentEndYear - year) /
                                    1000.0,
                                0.0, 1.0));
  if (polity.governmentForm != GovernmentForm::Kingdom)
    addCandidate(
        GovernmentForm::Kingdom,
        configuration.kingdomGovernmentWeight +
            (configuration.lateKingdomGovernmentWeight -
             configuration.kingdomGovernmentWeight) *
                std::clamp(static_cast<double>(
                               year - configuration.kingdomWeightReductionStartYear) /
                               static_cast<double>(std::max<Year>(
                                   1, configuration.kingdomWeightReductionEndYear -
                                          configuration.kingdomWeightReductionStartYear)),
                           0.0, 1.0));
  if (year < configuration.tradeRepublicEndYear && hasCoastalProvince &&
      navyTradition >= 0.2)
    addCandidate(GovernmentForm::TradeRepublic,
                 configuration.tradeRepublicGovernmentWeight *
                     std::clamp((navyTradition - 0.2) * 2.0, 0.0, 1.0));
  const auto postIndustrialMultiplier =
      year >= configuration.postIndustrialModernGovernmentStartYear
          ? std::max(1.0, configuration.postIndustrialModernGovernmentMultiplier)
          : 1.0;
  if (year >= configuration.theocracyStartYear &&
      year <= configuration.theocracyEndYear &&
      (polity.governmentForm == GovernmentForm::Kingdom ||
       polity.governmentForm == GovernmentForm::TradeRepublic))
    addCandidate(GovernmentForm::Theocracy,
                 configuration.theocracyGovernmentWeight *
                     std::clamp(static_cast<double>(configuration.theocracyEndYear - year) /
                                    static_cast<double>(std::max<Year>(
                                        1, configuration.theocracyEndYear -
                                            configuration.theocracyStartYear)),
                                0.0, 1.0));
  if (year >= configuration.absolutismStartYear &&
      (polity.governmentForm == GovernmentForm::Kingdom ||
       polity.governmentForm == GovernmentForm::TradeRepublic))
    addCandidate(GovernmentForm::Absolutism,
                 configuration.absolutismGovernmentWeight);
  if (year >= configuration.constitutionalMonarchyStartYear &&
      (polity.governmentForm == GovernmentForm::Kingdom ||
       polity.governmentForm == GovernmentForm::Absolutism ||
       polity.governmentForm == GovernmentForm::Theocracy))
    addCandidate(GovernmentForm::ConstitutionalMonarchy,
                 configuration.constitutionalMonarchyGovernmentWeight);
  if (year >= configuration.democracyStartYear &&
      (polity.governmentForm == GovernmentForm::Kingdom ||
       polity.governmentForm == GovernmentForm::Absolutism ||
       polity.governmentForm == GovernmentForm::TradeRepublic ||
       polity.governmentForm == GovernmentForm::ConstitutionalMonarchy ||
       polity.governmentForm == GovernmentForm::Theocracy))
    addCandidate(GovernmentForm::Democracy,
                 configuration.democracyGovernmentWeight * modernPressure *
                     postIndustrialMultiplier);
  if (year >= configuration.communismStartYear &&
      (polity.governmentForm == GovernmentForm::Kingdom ||
       polity.governmentForm == GovernmentForm::Absolutism ||
       polity.governmentForm == GovernmentForm::ConstitutionalMonarchy ||
       polity.governmentForm == GovernmentForm::Democracy ||
       polity.governmentForm == GovernmentForm::Theocracy))
    addCandidate(GovernmentForm::Communism,
                 configuration.communismGovernmentWeight * modernPressure *
                     postIndustrialMultiplier);
  if (year >= configuration.fascismStartYear &&
      (polity.governmentForm == GovernmentForm::Kingdom ||
       polity.governmentForm == GovernmentForm::Absolutism ||
       polity.governmentForm == GovernmentForm::ConstitutionalMonarchy ||
       polity.governmentForm == GovernmentForm::Democracy ||
       polity.governmentForm == GovernmentForm::Theocracy))
    addCandidate(GovernmentForm::Fascism,
                 configuration.fascismGovernmentWeight * modernPressure *
                     postIndustrialMultiplier);

  candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
                                  [&polity](const auto &candidate) {
                                    return candidate.first == polity.governmentForm;
                                  }),
                   candidates.end());
  if (sourceIndex < diagnostics.candidatePoolsBySource.size())
    ++diagnostics.candidatePoolsBySource[sourceIndex];
  double totalWeight = 0.0;
  for (const auto &[form, weight] : candidates)
    totalWeight += weight;
  if (totalWeight <= 0.0)
    {
      ++diagnostics.emptyCandidatePools;
    return polity.governmentForm;
    }
  auto selection = RandNum::getRandom<double>(totalWeight);
  for (const auto &[form, weight] : candidates) {
    if (selection < weight)
      ++diagnostics.selectedTransitions;
      ++diagnostics.selectionsByDestination[static_cast<std::size_t>(form)];
      if (static_cast<int>(form) < static_cast<int>(polity.governmentForm))
        ++diagnostics.reversions;
      return form;
    selection -= weight;
  }
  const auto form = candidates.back().first;
  ++diagnostics.selectedTransitions;
  ++diagnostics.selectionsByDestination[static_cast<std::size_t>(form)];
  if (static_cast<int>(form) < static_cast<int>(polity.governmentForm))
    ++diagnostics.reversions;
  return form;
}

Fwg::Gfx::Colour randomColour() {
  return {static_cast<unsigned char>(RandNum::getRandom<int>(256)),
          static_cast<unsigned char>(RandNum::getRandom<int>(256)),
          static_cast<unsigned char>(RandNum::getRandom<int>(256))};
}

double randomCentralizationAffinity(const Configuration &configuration) {
  const auto minimum = configuration.centralizationAffinityMinimum;
  const auto maximum = configuration.centralizationAffinityMaximum;
  return std::clamp(minimum +
                        RandNum::getRandom<double>(1.0) * (maximum - minimum),
                    0.0, 2.0);
}

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
} // namespace

void captureTimelineSnapshot(
    Year year, const NormalizedInput &normalized, const State &state,
    std::map<PolityId, std::vector<PolityTimelineSnapshot>> &timeline) {
  const auto territories = state::territoriesByPolity(state);
  for (PolityId polityId = 0;
       polityId < static_cast<PolityId>(state.polities.size()); ++polityId) {
    const auto *polity = state::findPolity(state, polityId);
    if (!polity || territories[static_cast<size_t>(polityId)].empty())
      continue;

    PolityTimelineSnapshot snapshot;
    snapshot.year = year;
    snapshot.polityId = polityId;
    snapshot.governmentForm = polity->governmentForm;
    snapshot.centralization = polity->centralization;
    snapshot.armyTradition = polity->armyTradition;
    snapshot.navyTradition = polity->navyTradition;
    snapshot.stability = polity->stability;
    snapshot.politicalMovements = polity->politicalMovements;

    double area = 0.0;
    for (const auto provinceId : territories[static_cast<size_t>(polityId)]) {
      const auto *province = state::findProvince(state, provinceId);
      if (!province)
        continue;
      snapshot.population += province->population;
      snapshot.development += province->development;
      const auto environment = normalized.environments.find(provinceId);
      if (environment != normalized.environments.end())
        area += static_cast<double>(environment->second.area);
    }
    snapshot.development /=
        static_cast<double>(territories[static_cast<size_t>(polityId)].size());
    snapshot.populationDensity =
        area > 0.0 ? snapshot.population / area
                   : snapshot.population /
                         static_cast<double>(
                             territories[static_cast<size_t>(polityId)].size());
    timeline[polityId].push_back(std::move(snapshot));
  }
}

void updatePoliticalStates(Year nextYear, double centuries,
                           const Configuration &configuration, State &state,
                           const AppendEvent &append,
                           const std::vector<WarEvent> &wars,
                           GovernmentTransitionDiagnostics &diagnostics) {
  const auto territories = state::territoriesByPolity(state);
  const auto centralizationTargets =
      calculateCentralizationTargets(state, configuration);
  std::vector<double> armyGains(state.polities.size());
  std::vector<double> navyGains(state.polities.size());
  std::vector<double> developmentTotals(state.polities.size());
  std::vector<std::size_t> developmentProvinceCounts(state.polities.size());
  std::vector<double> warPressure(state.polities.size());
  std::vector<double> stabilityWarEffects(state.polities.size());
  for (const auto &province : state.provinces) {
    if (!province.initialized || province.owner < 0 ||
        province.owner >= static_cast<PolityId>(state.polities.size()))
      continue;
    const auto index = static_cast<size_t>(province.owner);
    developmentTotals[index] += province.development;
    ++developmentProvinceCounts[index];
  }
  for (const auto &war : wars) {
    if (war.year != nextYear)
      continue;
    for (const auto polityId : war.winners) {
      if (polityId < 0 ||
          polityId >= static_cast<PolityId>(state.polities.size()))
        continue;
      armyGains[static_cast<size_t>(polityId)] += 0.08;
      navyGains[static_cast<size_t>(polityId)] += 0.03;
      warPressure[static_cast<size_t>(polityId)] += 1.0;
      stabilityWarEffects[static_cast<size_t>(polityId)] +=
          configuration.stabilityWarVictoryImpact;
    }
    for (const auto polityId : war.losers) {
      if (polityId < 0 ||
          polityId >= static_cast<PolityId>(state.polities.size()))
        continue;
      armyGains[static_cast<size_t>(polityId)] += 0.02;
      navyGains[static_cast<size_t>(polityId)] += 0.01;
      warPressure[static_cast<size_t>(polityId)] += 1.0;
      stabilityWarEffects[static_cast<size_t>(polityId)] +=
          configuration.stabilityWarDefeatImpact;
    }
  }
  for (PolityId polityId = 0;
       polityId < static_cast<PolityId>(state.polities.size()); ++polityId) {
    auto *polity = state::findPolity(state, polityId);
    if (!polity || !polity->initialized || polity->dissolvedYear ||
        territories[static_cast<size_t>(polityId)].empty())
      continue;
    const auto index = static_cast<size_t>(polityId);
    const auto coastal = !state::coastalProvincesOf(state, polityId).empty();
    const auto centralization =
        nextCentralization(polity->centralization, centralizationTargets[index],
                           centuries, configuration);
    const auto armyTradition = std::clamp(
        polity->armyTradition + armyGains[index] - 0.04 * centuries, 0.0, 1.0);
    const auto navyTradition = std::clamp(
        polity->navyTradition + navyGains[index] - 0.03 * centuries, 0.0, 1.0);
    const auto stabilityReversion =
        (configuration.stabilityMean - polity->stability) *
        std::max(0.0, configuration.stabilityMeanReversionPerCentury) *
        centuries;
    const auto eventMinimum =
        std::max(0.0, configuration.stabilityRandomEventMinimumImpact);
    const auto eventMaximum = std::max(eventMinimum,
                                       configuration.stabilityRandomEventMaximumImpact);
    const auto randomEvent =
        chance(std::max(0.0, configuration.stabilityRandomEventChancePerCentury) *
               centuries)
            ? (eventMinimum +
               RandNum::getRandom<double>(1.0) *
                   (eventMaximum - eventMinimum)) *
                  (RandNum::getRandom<double>(2.0) - 1.0)
            : 0.0;
    const auto randomVariation =
        (RandNum::getRandom<double>(2.0) - 1.0) *
        std::max(0.0, configuration.stabilityVolatilityPerCentury) *
        std::sqrt(std::max(centuries, 0.0));
    const auto stability = std::clamp(
        polity->stability + stabilityReversion +
            stabilityWarEffects[index] + randomEvent + randomVariation,
        0.0, 1.0);
    Event update{nextYear, EventType::PolityUpdate};
    update.polityId = polityId;
    update.centralizationAffinity = polity->centralizationAffinity;
    update.governmentForm = governmentFor(
        *polity, nextYear, configuration, coastal, stability,
        navyTradition, armyTradition,
        developmentProvinceCounts[index] == 0
            ? 0.0
            : developmentTotals[index] /
                  static_cast<double>(developmentProvinceCounts[index]),
        std::clamp(warPressure[index], 0.0, 1.0), centuries, diagnostics);
    update.lastGovernmentTransitionYear = polity->lastGovernmentTransitionYear;
    if (update.governmentForm != polity->governmentForm)
      update.lastGovernmentTransitionYear = nextYear;
    update.centralization = centralization;
    update.armyTradition = armyTradition;
    update.navyTradition = navyTradition;
    update.stability = stability;
    update.politicalMovements = polity->politicalMovements;
    update.politicalMovements[static_cast<int>(update.governmentForm)] =
        std::clamp(stability * (1.0 - centralization), 0.0, 1.0);
    append(std::move(update));
  }
}

void implode(const Year nextYear, double centuries,
             const Configuration &configuration,
             const NormalizedInput &normalized, State &state,
             const AppendEvent &append, PolityId &nextPolity) {
  const auto territories = state::territoriesByPolity(state);
  const auto currentPolityCount = static_cast<double>(std::count_if(
      territories.begin(), territories.end(),
      [](const auto &provinceIds) { return !provinceIds.empty(); }));
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
  const bool aggressive = nextYear >= configuration.polityAggressionStartYear;
  const double expansionMultiplier =
      aggressive ? configuration.aggressiveExpansionMultiplier : 1.0;
  const double fragmentationMultiplier =
      aggressive ? configuration.aggressiveFragmentationMultiplier : 1.0;
  const double postStabilizationProgress =
      configuration.polityStabilizationStartYear >= configuration.targetYear
          ? 0.0
          : std::clamp(
                static_cast<double>(
                    nextYear - configuration.polityStabilizationStartYear) /
                    static_cast<double>(
                        configuration.targetYear -
                        configuration.polityStabilizationStartYear),
                0.0, 1.0);
  const double stabilizationMultiplier =
      1.0 - postStabilizationProgress *
                (1.0 - configuration.postStabilizationFragmentationMultiplier);
  PolityId strongestPolity = NoPolity;
  double strongestScore = 0.0;
  for (const auto &[polityId, strength] : state.polityStrengths) {
    if (strength.score > strongestScore) {
      strongestPolity = polityId;
      strongestScore = strength.score;
    }
  }
  for (PolityId polityId = 0;
       polityId < static_cast<PolityId>(territories.size()); ++polityId) {
    const auto &provinceIds = territories[static_cast<std::size_t>(polityId)];
    if (provinceIds.empty())
      continue;
    if (provinceIds.size() < 2 || !state::findPolity(state, polityId) ||
        !state.polityStrengths.contains(polityId))
      continue;
    const auto polityScore = state.polityStrengths.at(polityId).score;
    const auto foundedYear = state::findPolity(state, polityId)->foundedYear;
    const auto maturationProgress =
        configuration.successorMaturationYears <= 0
            ? 1.0
            : std::clamp(static_cast<double>(nextYear - foundedYear) /
                             static_cast<double>(
                                 configuration.successorMaturationYears),
                         0.0, 1.0);
    const auto maturationMultiplier =
        configuration.successorFragmentationMultiplier +
        maturationProgress *
            (1.0 - configuration.successorFragmentationMultiplier);
    const auto smallPolityProgress =
        configuration.smallPolityProtectionSize == 0 ||
                provinceIds.size() <= configuration.smallPolityProtectionSize
            ? 0.0
            : std::clamp(
                  static_cast<double>(provinceIds.size() -
                                      configuration.smallPolityProtectionSize) /
                      static_cast<double>(
                          configuration.smallPolityProtectionSize),
                  0.0, 1.0);
    const auto smallPolityMultiplier =
        configuration.smallPolityFragmentationMultiplier +
        smallPolityProgress *
            (1.0 - configuration.smallPolityFragmentationMultiplier);
    const bool decaying =
        strongestScore > 0.0 &&
        polityScore <= strongestScore * configuration.decayStrengthRatio;
    const bool decaySplit =
        decaying && chance(configuration.decayFragmentationChance * centuries *
                           stabilizationMultiplier * maturationMultiplier *
                           smallPolityMultiplier);
    const bool ordinarySplit = chance(
        configuration.splitOffChance * fragmentationMultiplier * centuries *
        std::max(0.25, 1.0 - consolidationPressure) * stabilizationMultiplier *
        maturationMultiplier * smallPolityMultiplier);
    if (!decaySplit && !ordinarySplit)
      continue;
    if (decaySplit) {
      const auto configuredMaximum = static_cast<size_t>(
          std::clamp(configuration.maximumImplosionSuccessors, 1, 10));
      const auto desiredSuccessorCount = provinceIds.size() >= 8 ? 4u : 3u;
      const auto successorCount =
          std::min(provinceIds.size(),
                   std::min(configuredMaximum,
                            static_cast<size_t>(desiredSuccessorCount)));
      std::vector<PolityId> successors;
      successors.reserve(successorCount);
      for (size_t index = 0; index < successorCount; ++index) {
        const auto successorId = nextPolity++;
        successors.push_back(successorId);
        Event successor{nextYear, EventType::CreateSuccessorPolity};
        successor.polityId = successorId;
        successor.parentId = polityId;
        successor.description = "successor state";
        successor.colour = randomColour();
        if (const auto *parent = state::findPolity(state, polityId)) {
          successor.governmentForm = parent->governmentForm;
          successor.centralizationAffinity = parent->centralizationAffinity;
          successor.lastGovernmentTransitionYear =
              parent->lastGovernmentTransitionYear;
          const auto inheritance =
              std::clamp(configuration.successorPoliticalInheritance, 0.0, 1.0);
          successor.centralization = parent->centralization * inheritance;
          successor.armyTradition = parent->armyTradition * inheritance;
          successor.navyTradition = parent->navyTradition * inheritance;
          successor.stability = parent->stability * inheritance;
          for (const auto &[movement, strength] : parent->politicalMovements)
            successor.politicalMovements[movement] = strength * inheritance;
        }
        append(std::move(successor));
      }
      const auto assignments = contiguousSuccessorAssignments(
          provinceIds, successors.size(), normalized.neighbours);
      for (size_t index = 0; index < provinceIds.size(); ++index)
        append({nextYear, EventType::TransferProvince, provinceIds[index],
                normalized.provinceRegions.at(provinceIds[index]),
                successors[static_cast<size_t>(assignments[index])], polityId,
                -1, NoReligion, 0.0, 0.0, "successor state partition"});
      append({nextYear, EventType::DissolvePolity, -1, -1, polityId, NoPolity,
              -1, NoReligion, 0.0, 0.0, "polity implosion"});
      continue;
    }
    const auto splitPolity = nextPolity++;
    Event split{nextYear, EventType::CreatePolity};
    split.polityId = splitPolity;
    split.description = "tribe";
    split.colour = randomColour();
    split.centralizationAffinity = randomCentralizationAffinity(configuration);
    append(std::move(split));
    const auto splitProvince = provinceIds.back();
    append({nextYear, EventType::TransferProvince, splitProvince,
            normalized.provinceRegions.at(splitProvince), splitPolity, polityId,
            -1, NoReligion, 0.0, 0.0, "fragmentation"});
  }
}

void consolidateRegions(const Year nextYear, const Configuration &configuration,
                        const NormalizedInput &normalized, const State &state,
                        const AppendEvent &append) {
  (void)configuration;
  for (const auto &[regionId, provinceIds] : normalized.regions) {
    std::map<PolityId, int> ownership;
    for (const auto provinceId : provinceIds) {
      const auto *provinceState = state::findProvince(state, provinceId);
      if (provinceState)
        ++ownership[provinceState->owner];
    }
    const auto dominant =
        std::max_element(ownership.begin(), ownership.end(),
                         [](const auto &left, const auto &right) {
                           return left.second != right.second
                                      ? left.second < right.second
                                      : left.first > right.first;
                         })
            ->first;
    for (const auto provinceId : provinceIds) {
      const auto *province = state::findProvince(state, provinceId);
      if (!province)
        continue;
      const auto previous = province->owner;
      if (previous != dominant)
        append({nextYear, EventType::ConsolidateRegion, provinceId, regionId,
                dominant, previous, -1, NoReligion, 0.0, 0.0,
                "regional consolidation"});
    }
  }
}

void dissolveEmpty(const Year nextYear, State &state,
                   const AppendEvent &append) {
  const auto territories = state::territoriesByPolity(state);
  for (PolityId polityId = 0;
       polityId < static_cast<PolityId>(state.polities.size()); ++polityId) {
    const auto *polity = state::findPolity(state, polityId);
    if (polity && !polity->dissolvedYear &&
        territories[static_cast<std::size_t>(polityId)].empty())
      append({nextYear, EventType::DissolvePolity, -1, -1, polityId, NoPolity,
              -1, NoReligion, 0.0, 0.0, "annexed or dissolved"});
  }
}

} // namespace Arda::Simulation::polity
