#include "simulation/SimulationPolity.h"

#include "RandNum.h"
#include "simulation/SimulationState.h"

#include <algorithm>
#include <set>

namespace Arda::Simulation::polity {
namespace {

using detail::AppendEvent;
using detail::NormalizedInput;

bool chance(double probability) {
  return probability > 0.0 && RandNum::getRandom<double>(1.0) < probability;
}

Fwg::Gfx::Colour randomColour() {
  return {static_cast<unsigned char>(RandNum::getRandom<int>(256)),
          static_cast<unsigned char>(RandNum::getRandom<int>(256)),
          static_cast<unsigned char>(RandNum::getRandom<int>(256))};
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
        append({nextYear, EventType::CreateSuccessorPolity, -1, -1, successorId,
                NoPolity, -1, NoReligion, 0.0, 0.0, "successor state", polityId,
                randomColour()});
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
    append({nextYear, EventType::CreatePolity, -1, -1, splitPolity, NoPolity,
            -1, NoReligion, 0.0, 0.0, "tribe", -1, randomColour()});
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
