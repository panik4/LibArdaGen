#include "simulation/SimulationWarfare.h"
#include "simulation/SimulationState.h"

#include "RandNum.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <span>

namespace Arda::Simulation::warfare {
namespace {

using MaritimeReachability = std::vector<std::vector<bool>>;
using PolityReachability = std::vector<std::vector<PolityId>>;

bool chance(double probability) {
  return probability > 0.0 && RandNum::getRandom<double>(1.0) < probability;
}

double
landWarRangeForYear(const Arda::Simulation::Year simulationYear,
                    const Arda::Simulation::Configuration &configuration) {
  const auto centuriesSinceStart = std::max(
      0.0,
      static_cast<double>(simulationYear - configuration.startYear) / 100.0);
  return std::min(configuration.warMaximumDistance,
                  configuration.initialLandWarRange *
                      std::pow(1.0 + configuration.landWarRangeGrowthPerCentury,
                               centuriesSinceStart));
}

PolityReachability buildPolityReachability(
    const detail::NormalizedInput &normalized, const State &state,
    std::span<const PolityId> activePolities, double maritimeRange) {
  // Rows follow activePolities; target values retain their original polity IDs.
  PolityReachability reachability(activePolities.size());
  std::vector<unsigned char> seen(state.polities.size(), 0);
  for (std::size_t index = 0; index < activePolities.size(); ++index) {
    const auto polityId = activePolities[index];
    auto &targets = reachability[index];
    for (const auto sourceId : state::coastalProvincesOf(state, polityId)) {
      if (sourceId < 0)
        continue;
      const auto routes = normalized.seaRoutesFrom.find(sourceId);
      if (routes == normalized.seaRoutesFrom.end())
        continue;
      for (const auto &route : routes->second) {
        if (route.distance > maritimeRange)
          break;
        const auto *target = state::findProvince(state, route.provinceId);
        if (!target || target->owner < 0 ||
            static_cast<std::size_t>(target->owner) >= state.polities.size() ||
            seen[static_cast<std::size_t>(target->owner)] ||
            !state::coastalProvincesOf(state, target->owner).contains(route.provinceId))
          continue;
        seen[static_cast<std::size_t>(target->owner)] = 1;
        targets.push_back(target->owner);
      }
    }
    std::sort(targets.begin(), targets.end());
    for (const auto target : targets)
      seen[static_cast<std::size_t>(target)] = 0;
  }
  return reachability;
}

bool hasMaritimeWarConnectionOneWay(PolityId left, PolityId right,
                                    const State &state,
                                    const MaritimeReachability &reachability) {
  const auto &coastalSources =
      Arda::Simulation::state::coastalProvincesOf(state, left);
  const auto &coastalTargets =
      Arda::Simulation::state::coastalProvincesOf(state, right);
  for (const auto sourceId : coastalSources) {
    if (sourceId < 0 ||
        static_cast<std::size_t>(sourceId) >= reachability.size())
      continue;
    const auto &reachableTargets =
        reachability[static_cast<std::size_t>(sourceId)];
    for (const auto targetId : coastalTargets)
      if (targetId >= 0 &&
          static_cast<std::size_t>(targetId) < reachableTargets.size() &&
          reachableTargets[static_cast<std::size_t>(targetId)])
        return true;
  }
  return false;
}

struct BorderOwners {
  std::vector<PolityId> discoveryOrder;
  std::vector<PolityId> sorted;

  bool contains(PolityId polityId) const {
    return std::binary_search(sorted.begin(), sorted.end(), polityId);
  }
};

BorderOwners gatherBorders(PolityId attacker,
                           std::span<const ProvinceId> territory,
                           const detail::NormalizedInput &normalized,
                           const State &state) {
  BorderOwners borders;
  std::set<PolityId> seen;
  for (const auto provinceId : territory) {
    const auto neighbours = normalized.neighbours.find(provinceId);
    if (neighbours == normalized.neighbours.end())
      continue;
    for (const auto neighbourId : neighbours->second) {
      const auto *neighbour = state::findProvince(state, neighbourId);
      if (neighbour && neighbour->owner >= 0 && neighbour->owner != attacker &&
          static_cast<std::size_t>(neighbour->owner) < state.polities.size() &&
          seen.insert(neighbour->owner).second)
        borders.discoveryOrder.push_back(neighbour->owner);
    }
  }
  borders.sorted.assign(seen.begin(), seen.end());
  return borders;
}

struct ConnectivityScratch {
  std::vector<unsigned char> remaining;
  std::vector<ProvinceId> touched;
  std::vector<ProvinceId> frontier;
};

bool territoryRemainsContiguous(
    PolityId owner, ProvinceId removedProvince,
    std::span<const ProvinceId> territory,
    const std::map<ProvinceId, std::vector<ProvinceId>> &neighbours,
    const std::vector<ProvinceState> &provinces, ConnectivityScratch &scratch) {
  for (const auto provinceId : scratch.touched)
    scratch.remaining[static_cast<std::size_t>(provinceId)] = 0;
  scratch.remaining.resize(provinces.size(), 0);
  scratch.touched.clear();
  scratch.frontier.clear();

  // The phase-start territory can contain provinces transferred earlier in
  // this settlement. Only current ownership participates in connectivity.
  for (const auto provinceId : territory) {
    const auto &province = provinces.at(static_cast<std::size_t>(provinceId));
    if (province.initialized && province.owner == owner &&
        provinceId != removedProvince) {
      scratch.remaining[static_cast<std::size_t>(provinceId)] = 1;
      scratch.touched.push_back(provinceId);
    }
  }
  if (scratch.touched.size() < 2)
    return true;

  const auto seed = scratch.touched.front();
  scratch.remaining[static_cast<std::size_t>(seed)] = 0;
  scratch.frontier.push_back(seed);
  std::size_t visited = 1;
  while (!scratch.frontier.empty()) {
    const auto provinceId = scratch.frontier.back();
    scratch.frontier.pop_back();
    for (const auto neighbourId : neighbours.at(provinceId)) {
      if (neighbourId < 0 ||
          static_cast<std::size_t>(neighbourId) >= scratch.remaining.size() ||
          !scratch.remaining[static_cast<std::size_t>(neighbourId)])
        continue;
      scratch.remaining[static_cast<std::size_t>(neighbourId)] = 0;
      scratch.frontier.push_back(neighbourId);
      ++visited;
    }
  }
  return visited == scratch.touched.size();
}

} // namespace

double expansionBorderScore(
    ProvinceId targetProvince, PolityId attacker,
    const std::map<ProvinceId, std::vector<ProvinceId>> &neighbours,
    const std::vector<ProvinceState> &provinces) {
  const auto &targetNeighbours = neighbours.at(targetProvince);
  double score = static_cast<double>(targetNeighbours.size());
  for (const auto neighbourId : targetNeighbours) {
    const auto &neighbour = provinces.at(static_cast<std::size_t>(neighbourId));
    if (neighbour.initialized && neighbour.owner == attacker)
      score += 3.0;

    size_t attackerNeighbours = 0;
    size_t foreignNeighbours = 0;
    for (const auto surroundingId : neighbours.at(neighbourId)) {
      const auto &surrounding =
          provinces.at(static_cast<std::size_t>(surroundingId));
      if (surrounding.initialized && surrounding.owner == attacker)
        ++attackerNeighbours;
      else if (surrounding.initialized && surrounding.owner == neighbour.owner)
        ++foreignNeighbours;
    }
    if (foreignNeighbours > 0 && attackerNeighbours + 1 >= foreignNeighbours)
      score -= 8.0;
    if (foreignNeighbours == 1 && attackerNeighbours > 0)
      score -= 12.0;
  }
  return score;
}

double
expansionTargetWeakness(PolityId attacker, PolityId defender,
                        const std::map<PolityId, PolityStrength> &strengths) {
  if (defender == NoPolity)
    return 1.0;
  const auto attackerIt = strengths.find(attacker);
  const auto defenderIt = strengths.find(defender);
  if (attackerIt == strengths.end() || defenderIt == strengths.end())
    return 0.0;
  const auto attackerScore = std::max(1.0, attackerIt->second.score);
  const auto defenderScore = std::max(0.0, defenderIt->second.score);
  return std::clamp(
      attackerScore / std::max(1.0, attackerScore + defenderScore), 0.0, 1.0);
}

bool remainsContiguousAfterConquest(
    PolityId owner, ProvinceId removedProvince,
    const std::map<ProvinceId, std::vector<ProvinceId>> &neighbours,
    const std::vector<ProvinceState> &provinces) {
  std::vector<ProvinceId> territory;
  territory.reserve(provinces.size());
  for (ProvinceId provinceId = 0;
       provinceId < static_cast<ProvinceId>(provinces.size()); ++provinceId)
    if (provinces[static_cast<std::size_t>(provinceId)].initialized &&
        provinces[static_cast<std::size_t>(provinceId)].owner == owner)
      territory.push_back(provinceId);
  ConnectivityScratch scratch;
  return territoryRemainsContiguous(owner, removedProvince, territory,
                                    neighbours, provinces, scratch);
}

double provinceCenterDistance(const ArdaProvince &left,
                              const ArdaProvince &right) {
  const auto dx = static_cast<double>(left.position.widthCenter -
                                      right.position.widthCenter);
  const auto dy = static_cast<double>(left.position.heightCenter -
                                      right.position.heightCenter);
  return std::sqrt(dx * dx + dy * dy);
}

struct CapitalProvinces {
  const ArdaProvince *left = nullptr;
  const ArdaProvince *right = nullptr;
};

CapitalProvinces findCapitalProvinces(PolityId left, PolityId right,
                                      const State &state) {
  const auto *leftPolity = state::findPolity(state, left);
  const auto *rightPolity = state::findPolity(state, right);
  if (!leftPolity || !rightPolity || !leftPolity->capital ||
      !rightPolity->capital)
    return {};

  return {leftPolity->capital.get(), rightPolity->capital.get()};
}

double polityDistance(PolityId left, PolityId right, const State &state) {
  const auto capitals = findCapitalProvinces(left, right, state);
  if (!capitals.left || !capitals.right)
    return std::numeric_limits<double>::max();

  return provinceCenterDistance(*capitals.left, *capitals.right);
}

double maritimeRangeForYear(const Year year,
                            const Configuration &configuration) {
  const auto initialMaritimeRange = Fwg::Cfg::Values().width * 0.01;
  if (year < configuration.maritimeExpansionStartYear)
    return 0.0;
  const auto centuriesSinceStart = std::max(
      0.0,
      static_cast<double>(year - configuration.maritimeExpansionStartYear) /
          100.0);
  return std::max(
      1.0, initialMaritimeRange *
               std::pow(1.0 + configuration.maritimeRangeGrowthPerCentury,
                        centuriesSinceStart));
}

bool hasMaritimeWarConnection(PolityId left, PolityId right, const State &state,
                              const MaritimeReachability &reachability) {
  return hasMaritimeWarConnectionOneWay(left, right, state, reachability);
}

bool capitalsShareLandMass(PolityId left, PolityId right, const State &state) {
  const auto *leftPolity = state::findPolity(state, left);
  const auto *rightPolity = state::findPolity(state, right);
  const auto *leftCapital =
      leftPolity ? state::findProvince(state, leftPolity->capitalProvince)
                 : nullptr;
  const auto *rightCapital =
      rightPolity ? state::findProvince(state, rightPolity->capitalProvince)
                  : nullptr;
  if (!leftCapital || !rightCapital || leftCapital->landMassID < 0 ||
      rightCapital->landMassID < 0)
    return false;

  return leftCapital->landMassID == rightCapital->landMassID;
}

struct NavalDiagnostics {
  size_t differentLandmassPairs = 0;
  size_t navalConnections = 0;
  size_t navalRangeFailures = 0;
  size_t missingLandMassData = 0;
  size_t navalDefenderSelections = 0;
};

struct CapitalCacheEntry {
  const ArdaProvince *province = nullptr;
  ProvinceId provinceId = -1;
  int landMassID = -1;
};

std::vector<CapitalCacheEntry>
buildCapitalCache(const State &state, std::span<const PolityId> activePolities) {
  std::vector<CapitalCacheEntry> cache(state.polities.size());
  for (const auto polityId : activePolities) {
    const auto *polity = state::findPolity(state, polityId);
    if (!polity)
      continue;
    const auto *province = state::findProvince(state, polity->capitalProvince);
    if (!province || !polity->capital)
      continue;
    cache[static_cast<std::size_t>(polityId)] = {
        polity->capital.get(), polity->capitalProvince, province->landMassID};
  }
  return cache;
}

PolityId selectDefender(PolityId attackerId,
                        const std::vector<std::vector<ProvinceId>> &territories,
                        std::span<const PolityId> activePolities,
                        const BorderOwners &borders,
                        const std::vector<char> &committed,
                        const Configuration &configuration,
                        const detail::NormalizedInput &normalized,
                        const State &state, Year nextYear, double landWarRange,
                        double maritimeRange,
                        std::span<const PolityId> maritimeTargets,
                        bool navalChecksEnabled, bool &representativePairLogged,
                        NavalDiagnostics &diagnostics,
                        const std::vector<CapitalCacheEntry> &capitalCache) {
  PolityId defenderId = NoPolity;
  std::vector<std::pair<PolityId, double>> candidates;
  const std::span<const PolityId> candidateIds =
      nextYear < configuration.maritimeExpansionStartYear
          ? std::span<const PolityId>(borders.discoveryOrder)
          : activePolities;

  for (const auto candidateId : candidateIds) {
    const auto &candidateTerritory =
        territories[static_cast<std::size_t>(candidateId)];
    if (candidateId == attackerId ||
        committed[static_cast<std::size_t>(candidateId)] ||
        candidateTerritory.empty())
      continue;
    if (attackerId < 0 || candidateId < 0 ||
        static_cast<std::size_t>(attackerId) >= capitalCache.size() ||
        static_cast<std::size_t>(candidateId) >= capitalCache.size())
      continue;
    const auto &attackerCapital =
        capitalCache[static_cast<std::size_t>(attackerId)];
    const auto &candidateCapital =
        capitalCache[static_cast<std::size_t>(candidateId)];
    if (!attackerCapital.province || !candidateCapital.province)
      continue;
    const auto distance = provinceCenterDistance(*attackerCapital.province,
                                                 *candidateCapital.province);
    const auto sharedLandMass =
        attackerCapital.landMassID >= 0 && candidateCapital.landMassID >= 0 &&
        attackerCapital.landMassID == candidateCapital.landMassID;
    const bool capitalsHaveDifferentLandmasses =
        attackerCapital.landMassID >= 0 && candidateCapital.landMassID >= 0 &&
        attackerCapital.landMassID != candidateCapital.landMassID;
    const auto directLandBorder = borders.contains(candidateId);
    if ((sharedLandMass || directLandBorder) && distance > landWarRange)
      continue;

    bool maritimeConnection = false;
    if (navalChecksEnabled && maritimeRange > 0.0 && !sharedLandMass) {
      maritimeConnection = std::binary_search(
          maritimeTargets.begin(), maritimeTargets.end(), candidateId);
    }
    if (configuration.enableWarfareDiagnostics && navalChecksEnabled &&
        capitalsHaveDifferentLandmasses &&
        !representativePairLogged) {
      const auto *attackerPolity = state::findPolity(state, attackerId);
      const auto *candidatePolity = state::findPolity(state, candidateId);
      if (!attackerPolity || !candidatePolity)
        continue;
      const auto &coastalSources = state::coastalProvincesOf(state, attackerId);
      const auto ownedCoastalSources = coastalSources.size();
      size_t ownedSourceRoutes = 0;
      double shortestOwnedRoute = std::numeric_limits<double>::max();
      for (const auto sourceId : coastalSources) {
        const auto routes = normalized.seaRoutesFrom.find(sourceId);
        if (routes == normalized.seaRoutesFrom.end())
          continue;
        ownedSourceRoutes += routes->second.size();
        for (const auto &route : routes->second)
          shortestOwnedRoute = std::min(shortestOwnedRoute, route.distance);
      }
      std::ostringstream message;
      message << "Naval representative pair in " << nextYear
              << ": attacker=" << attackerId << " defender=" << candidateId
              << " attackerCapital=" << attackerPolity->capitalProvince
              << " defenderCapital=" << candidatePolity->capitalProvince
              << " attackerLandMass=" << attackerCapital.landMassID
              << " defenderLandMass=" << candidateCapital.landMassID
              << " ownedCoastalSources=" << ownedCoastalSources
              << " ownedSourceRoutes=" << ownedSourceRoutes
              << " shortestOwnedRoute="
              << (ownedSourceRoutes == 0 ? 0.0 : shortestOwnedRoute)
              << " maritimeConnection="
              << (maritimeConnection ? "true" : "false");
      Fwg::Utils::Logging::logLine(message.str());
      representativePairLogged = true;
    }
    if (configuration.enableWarfareDiagnostics && navalChecksEnabled &&
        capitalsHaveDifferentLandmasses &&
        !directLandBorder) {
      ++diagnostics.differentLandmassPairs;
      if (maritimeConnection)
        ++diagnostics.navalConnections;
      else
        ++diagnostics.navalRangeFailures;
    } else if (configuration.enableWarfareDiagnostics && navalChecksEnabled &&
               (attackerCapital.landMassID < 0 || candidateCapital.landMassID < 0)) {
      ++diagnostics.missingLandMassData;
    }
    const auto geographicallyReachable =
        navalChecksEnabled
            ? (sharedLandMass || directLandBorder || maritimeConnection)
            : (sharedLandMass || directLandBorder);
    if (geographicallyReachable)
      candidates.emplace_back(candidateId, 10.0 * directLandBorder + 1.0);
  }
  if (!candidates.empty()) {
    const auto totalWeight =
        std::accumulate(candidates.begin(), candidates.end(), 0.0,
                        [](double total, const auto &candidate) {
                          return total + candidate.second;
                        });
    auto selection = RandNum::getRandom<double>(totalWeight);
    for (const auto &[candidateId, weight] : candidates) {
      if (selection < weight) {
        defenderId = candidateId;
        break;
      }
      selection -= weight;
    }
  }
  return defenderId;
}

void formAlliances(PolityId attackerId, PolityId defenderId,
                   const std::vector<std::vector<ProvinceId>> &territories,
                   std::span<const PolityId> activePolities,
                   std::vector<char> &committed,
                   const Configuration &configuration, const State &state,
                   std::vector<PolityId> &attackers,
                   std::vector<PolityId> &defenders) {
  attackers = {attackerId};
  defenders = {defenderId};
  committed[attackerId] = true;
  committed[defenderId] = true;
  for (const auto candidateId : activePolities) {
    const auto &candidateTerritory =
        territories[static_cast<std::size_t>(candidateId)];
    if (committed[static_cast<std::size_t>(candidateId)] ||
        candidateTerritory.empty() ||
        attackers.size() >= configuration.maximumWarAllianceMembers &&
            defenders.size() >= configuration.maximumWarAllianceMembers)
      continue;
    const auto attackerDistance =
        polityDistance(candidateId, attackerId, state);
    const auto defenderDistance =
        polityDistance(candidateId, defenderId, state);
    const auto allianceDistance = configuration.warMaximumDistance *
                                  configuration.warAllianceDistanceMultiplier;
    if (attackerDistance > allianceDistance &&
        defenderDistance > allianceDistance)
      continue;
    if (attackerDistance <= defenderDistance &&
        attackers.size() < configuration.maximumWarAllianceMembers)
      attackers.push_back(candidateId);
    else if (defenders.size() < configuration.maximumWarAllianceMembers)
      defenders.push_back(candidateId);
    else
      continue;
    committed[candidateId] = true;
  }
}

double sideStrength(const State &state, const std::vector<PolityId> &side) {
  return std::accumulate(
      side.begin(), side.end(), 0.0, [&state](double total, PolityId polityId) {
        const auto strength = state.polityStrengths.find(polityId);
        return total + (strength == state.polityStrengths.end()
                            ? 0.0
                            : strength->second.score);
      });
}

void settleTransfers(
    Year nextYear, const Configuration &configuration,
    const detail::NormalizedInput &normalized, const State &state,
    const std::vector<std::vector<ProvinceId>> &territories,
    const std::vector<PolityId> &attackers, const std::vector<PolityId> &winners,
    const std::vector<PolityId> &losers,
    int warId, double maritimeRange, const detail::AppendEvent &append,
    const std::vector<Event> &events, WarEvent &war,
    std::set<ProvinceId> &transferred, ConnectivityScratch &connectivity) {
  struct Candidate {
    ProvinceId provinceId;
    PolityId winner;
    double score;
    bool landAdjacent;
  };

  std::vector<Candidate> candidates;
  size_t landCandidateCount = 0;
  size_t navalCandidateCount = 0;
  for (const auto loser : losers) {
    if (loser < 0 || static_cast<std::size_t>(loser) >= territories.size())
      continue;
    for (const auto provinceId : territories[static_cast<std::size_t>(loser)]) {
      if (transferred.contains(provinceId))
        continue;
      const auto province = state::findProvince(state, provinceId);
      if (!province)
        continue;

      const auto neighbours = normalized.neighbours.find(provinceId);
      if (neighbours == normalized.neighbours.end())
        continue;
      std::vector<PolityId> borderingWinners;
      for (const auto neighbourId : neighbours->second) {
        const auto *neighbour = state::findProvince(state, neighbourId);
        if (!neighbour ||
            std::find(winners.begin(), winners.end(), neighbour->owner) ==
                winners.end() ||
            std::find(borderingWinners.begin(), borderingWinners.end(),
                      neighbour->owner) != borderingWinners.end())
          continue;
        borderingWinners.push_back(neighbour->owner);
      }

      size_t borderScore = borderingWinners.size();
      std::vector<PolityId> reachableWinners;
      double shortestMaritimeDistance =
          std::numeric_limits<double>::max();
      if (province->coastal && maritimeRange > 0.0 &&
          configuration.maximumWarMaritimeTransfers > 0) {
        const auto routes = normalized.seaRoutesTo.find(provinceId);
        if (routes != normalized.seaRoutesTo.end()) {
          for (const auto &route : routes->second) {
            if (route.distance > maritimeRange)
              break;
            const auto *source = state::findProvince(state, route.provinceId);
            if (!source || source->landMassID < 0 || province->landMassID < 0 ||
                source->landMassID == province->landMassID ||
                std::find(winners.begin(), winners.end(), source->owner) ==
                    winners.end())
              continue;
            shortestMaritimeDistance =
                std::min(shortestMaritimeDistance, route.distance);
            if (std::find(reachableWinners.begin(), reachableWinners.end(),
                          source->owner) == reachableWinners.end())
              reachableWinners.push_back(source->owner);
          }
        }
      }

      const bool landAdjacent = !borderingWinners.empty();
      if (!landAdjacent && reachableWinners.empty())
        continue;
      const auto &eligibleWinners =
          landAdjacent ? borderingWinners : reachableWinners;
      const auto winner =
          *std::max_element(eligibleWinners.begin(), eligibleWinners.end(),
                            [&state](PolityId left, PolityId right) {
                              const auto leftStrength =
                                  state.polityStrengths.find(left);
                              const auto rightStrength =
                                  state.polityStrengths.find(right);
                              return (leftStrength == state.polityStrengths.end()
                                          ? 0.0
                                          : leftStrength->second.score) <
                                     (rightStrength == state.polityStrengths.end()
                                          ? 0.0
                                          : rightStrength->second.score);
                            });
      const auto defenderStrength =
          state.polityStrengths.contains(loser)
              ? state.polityStrengths.at(loser).score
              : 0.0;
      auto capitalProximity = 1.0;
      if (landAdjacent) {
        const auto *winningPolity = state::findPolity(state, winner);
        const ArdaProvince *capitalGeography = nullptr;
        if (winningPolity) {
          const auto capitalIt =
              normalized.provinces.find(winningPolity->capitalProvince);
          if (capitalIt != normalized.provinces.end() && capitalIt->second)
            capitalGeography = capitalIt->second.get();
        }
        const auto candidateIt = normalized.provinces.find(provinceId);
        if (capitalGeography && candidateIt != normalized.provinces.end() &&
            candidateIt->second) {
          const auto capitalDistance =
              provinceCenterDistance(*capitalGeography, *candidateIt->second);
          const auto mapWidth =
              std::max(1.0, static_cast<double>(Fwg::Cfg::Values().width));
          capitalProximity =
              1.0 + 2.0 * std::clamp(1.0 - capitalDistance / mapWidth, 0.0, 1.0);
        }
      }
      const auto peripheralScore =
          1.0 / std::max(1.0, static_cast<double>(neighbours->second.size()));
      const auto navalProximity =
          shortestMaritimeDistance == std::numeric_limits<double>::max()
              ? 0.0
              : std::clamp(1.0 - shortestMaritimeDistance / maritimeRange,
                           0.0, 1.0);
      const auto baseScore = static_cast<double>(borderScore) * 4.0 +
                             peripheralScore * 2.0 +
                             1.0 / std::max(1.0, defenderStrength);
      candidates.push_back({provinceId, winner,
                            landAdjacent ? baseScore * 5.0 * capitalProximity
                                         : std::max(0.01, baseScore) *
                                               (1.0 + navalProximity),
                            landAdjacent});
      if (configuration.enableWarfareDiagnostics) {
        if (landAdjacent)
          ++landCandidateCount;
        else
          ++navalCandidateCount;
      }
    }
  }

  size_t loserProvinceCount = 0;
  for (const auto loser : losers)
    if (loser >= 0 && static_cast<std::size_t>(loser) < territories.size())
      loserProvinceCount += territories[static_cast<std::size_t>(loser)].size();
  size_t attackerProvinceCount = 0;
  for (const auto attacker : attackers)
    if (attacker >= 0 && static_cast<std::size_t>(attacker) < territories.size())
      attackerProvinceCount +=
          territories[static_cast<std::size_t>(attacker)].size();
  const auto loserBasedCap = loserProvinceCount == 0
                                 ? size_t{0}
                                 : std::max<size_t>(1, (loserProvinceCount + 4) / 5);
  const auto attackerMinimum =
      attackerProvinceCount == 0
          ? size_t{0}
          : std::max<size_t>(1, (attackerProvinceCount + 99) / 100);
  const auto mapBasedCap = std::max<size_t>(1, state.provinces.size() / 100);
  const auto transferCap =
      std::min(mapBasedCap, std::max(attackerMinimum, loserBasedCap));

  size_t maritimeTransferCount = 0;
  size_t landTransferCount = 0;
  while (!candidates.empty()) {
    if (war.transferEventIndices.size() >= transferCap)
      break;

    auto totalScore = 0.0;
    for (const auto &candidate : candidates)
      if (candidate.landAdjacent ||
          maritimeTransferCount < configuration.maximumWarMaritimeTransfers)
        totalScore += candidate.score;
    if (totalScore <= 0.0)
      break;

    auto selection = RandNum::getRandom<double>(totalScore);
    auto candidateIt = candidates.end();
    for (auto it = candidates.begin(); it != candidates.end(); ++it) {
      if (!it->landAdjacent &&
          maritimeTransferCount >= configuration.maximumWarMaritimeTransfers)
        continue;
      if (selection < it->score) {
        candidateIt = it;
        break;
      }
      selection -= it->score;
    }
    if (candidateIt == candidates.end())
      break;
    const auto candidate = *candidateIt;
    candidates.erase(candidateIt);

    const auto *province = state::findProvince(state, candidate.provinceId);
    if (!province ||
        (candidate.landAdjacent &&
         !territoryRemainsContiguous(
             province->owner, candidate.provinceId,
             territories.at(static_cast<std::size_t>(province->owner)),
             normalized.neighbours, state.provinces, connectivity)))
      continue;
    Event transfer{nextYear,
                   EventType::TransferProvince,
                   candidate.provinceId,
                   normalized.provinceRegions.at(candidate.provinceId),
                   candidate.winner,
                   province->owner,
                   -1,
                   NoReligion,
                   0.0,
                   0.0,
                   candidate.landAdjacent ? "war territorial settlement"
                                          : "war overseas settlement"};
    transfer.warId = warId;
    transfer.overseas = !candidate.landAdjacent;
    transfer.colony = province->colony;
    const auto previousEventCount = events.size();
    append(std::move(transfer));
    if (events.size() > previousEventCount) {
      war.transferEventIndices.push_back(events.size() - 1);
      transferred.insert(candidate.provinceId);
      if (!candidate.landAdjacent)
        ++maritimeTransferCount;
      else if (configuration.enableWarfareDiagnostics)
        ++landTransferCount;
    }
  }
  if (configuration.enableWarfareDiagnostics && war.transferEventIndices.empty()) {
    std::ostringstream message;
    message << "Settlement produced no transfers in " << nextYear
            << ": landCandidates=" << landCandidateCount
            << " navalCandidates=" << navalCandidateCount
            << " landTransfers=" << landTransferCount
            << " navalTransfers=" << maritimeTransferCount
            << " transferCap=" << transferCap
            << " maritimeRange=" << maritimeRange;
    Fwg::Utils::Logging::logLine(message.str());
  }
}

void resolveWars(const Year nextYear, double centuries,
                 const Configuration &configuration,
                 const detail::NormalizedInput &normalized, State &state,
                 const detail::AppendEvent &append, std::vector<WarEvent> &wars,
                 int &nextWarId, const std::vector<Event> &events) {
  const auto territories = Arda::Simulation::state::territoriesByPolity(state);
  std::vector<PolityId> activePolities;
  for (PolityId polityId = 0;
       polityId < static_cast<PolityId>(territories.size()); ++polityId)
    if (!territories[static_cast<std::size_t>(polityId)].empty())
      activePolities.push_back(polityId);

  std::vector<char> committed(state.polities.size(), false);
  ConnectivityScratch connectivity;
  const auto landWarRange = landWarRangeForYear(nextYear, configuration);
  const auto maritimeRange = maritimeRangeForYear(nextYear, configuration);
  // Transfers only involve already-committed polities. Queries between the
  // remaining uncommitted polities retain their phase-start coastal ownership.
  const auto maritimeReachability =
      maritimeRange > 0.0
          ? buildPolityReachability(normalized, state, activePolities, maritimeRange)
          : PolityReachability{};
  const auto capitalCache = buildCapitalCache(state, activePolities);
  NavalDiagnostics diagnostics;
  if (configuration.enableWarfareDiagnostics) {
    size_t stateCoastalProvinces = 0;
    for (const auto &province : state.provinces)
      if (province.initialized && province.coastal)
        ++stateCoastalProvinces;
    size_t routeCount = 0;
    size_t ownedSourceRoutes = 0;
    size_t ownedTargetRoutes = 0;
    size_t crossPolityRoutes = 0;
    size_t crossLandMassRoutes = 0;
    size_t crossLandMassCrossPolityRoutes = 0;
    bool crossLandMassSampleLogged = false;
    bool sampleLogged = false;
    for (const auto &[sourceId, routes] : normalized.seaRoutesFrom) {
      for (const auto &route : routes) {
        if (route.distance > maritimeRange)
          break;
        ++routeCount;
        const auto *source = state::findProvince(state, sourceId);
        const auto *target = state::findProvince(state, route.provinceId);
        if (!source || !target)
          continue;
        if (source->owner != NoPolity)
          ++ownedSourceRoutes;
        if (target->owner != NoPolity)
          ++ownedTargetRoutes;
        if (source->owner != NoPolity && target->owner != NoPolity &&
            source->owner != target->owner)
          ++crossPolityRoutes;
        if (source->landMassID >= 0 && target->landMassID >= 0 &&
            source->landMassID != target->landMassID) {
          ++crossLandMassRoutes;
          if (source->owner != NoPolity && target->owner != NoPolity &&
              source->owner != target->owner) {
            ++crossLandMassCrossPolityRoutes;
            if (!crossLandMassSampleLogged) {
              std::ostringstream crossLandMassSample;
              crossLandMassSample << "Cross-landmass route ownership sample in "
                                  << nextYear << ": source=" << sourceId
                                  << " sourceOwner=" << source->owner
                                  << " sourceLandMass=" << source->landMassID
                                  << " target=" << route.provinceId
                                  << " targetOwner=" << target->owner
                                  << " targetLandMass=" << target->landMassID
                                  << " distance=" << route.distance;
              Fwg::Utils::Logging::logLine(crossLandMassSample.str());
              crossLandMassSampleLogged = true;
            }
          }
        }
        if (!sampleLogged) {
          std::ostringstream sample;
          sample << "Naval route ownership sample in " << nextYear
                 << ": source=" << sourceId << " sourceOwner=" << source->owner
                 << " sourceCoastal=" << (source->coastal ? "true" : "false")
                 << " target=" << route.provinceId
                 << " targetOwner=" << target->owner
                 << " distance=" << route.distance;
          Fwg::Utils::Logging::logLine(sample.str());
          sampleLogged = true;
        }
      }
    }
    std::ostringstream message;
    message << "Naval evaluation in " << nextYear << ": range=" << maritimeRange
            << " stateProvinces=" << state.provinces.size()
            << " stateCoastalProvinces=" << stateCoastalProvinces
            << " routeSources=" << normalized.seaRoutesFrom.size()
            << " routeCount=" << routeCount
            << " ownedSourceRoutes=" << ownedSourceRoutes
            << " ownedTargetRoutes=" << ownedTargetRoutes
            << " crossPolityRoutes=" << crossPolityRoutes
            << " crossLandMassRoutes=" << crossLandMassRoutes
            << " crossLandMassCrossPolityRoutes="
            << crossLandMassCrossPolityRoutes;
    Fwg::Utils::Logging::logLine(message.str());
  }
  const auto navalChecksEnabled =
      nextYear >= configuration.maritimeExpansionStartYear;
  bool representativePairLogged = false;
  for (std::size_t activeIndex = 0; activeIndex < activePolities.size();
       ++activeIndex) {
    const auto attackerId = activePolities[activeIndex];
    const auto &attackerTerritory =
        territories[static_cast<std::size_t>(attackerId)];
    if (committed.at(attackerId) || attackerTerritory.empty())
      continue;
    const auto borders =
        gatherBorders(attackerId, attackerTerritory, normalized, state);
    const std::span<const PolityId> maritimeTargets =
        maritimeReachability.empty()
            ? std::span<const PolityId>{}
            : std::span<const PolityId>(maritimeReachability[activeIndex]);
    const auto defenderId =
        selectDefender(attackerId, territories, activePolities, borders,
                       committed, configuration, normalized, state, nextYear,
                       landWarRange, maritimeRange, maritimeTargets, navalChecksEnabled,
                       representativePairLogged, diagnostics, capitalCache);
    if (defenderId == NoPolity)
      continue;
    if (configuration.enableWarfareDiagnostics && navalChecksEnabled &&
        attackerId >= 0 && defenderId >= 0 &&
        static_cast<std::size_t>(attackerId) < capitalCache.size() &&
        static_cast<std::size_t>(defenderId) < capitalCache.size()) {
      const auto &attackerCapital =
          capitalCache[static_cast<std::size_t>(attackerId)];
      const auto &defenderCapital =
          capitalCache[static_cast<std::size_t>(defenderId)];
      if (attackerCapital.province && defenderCapital.province &&
          attackerCapital.landMassID >= 0 && defenderCapital.landMassID >= 0 &&
          attackerCapital.landMassID != defenderCapital.landMassID &&
          !borders.contains(defenderId))
        ++diagnostics.navalDefenderSelections;
    }
    const auto warChanceMultiplier =
        nextYear < configuration.industrialRevolutionStartYear
            ? std::max(0.0, configuration.preIndustrialWarChanceMultiplier)
            : 1.0;
    const auto warProbability =
        1.0 - std::exp(-configuration.warChancePerCentury *
                       warChanceMultiplier * centuries);
    if (!chance(warProbability))
      continue;

    std::vector<PolityId> attackers;
    std::vector<PolityId> defenders;
    formAlliances(attackerId, defenderId, territories, activePolities,
                  committed, configuration, state, attackers, defenders);
    const auto attackersWin =
        sideStrength(state, attackers) >= sideStrength(state, defenders);
    const auto &winners = attackersWin ? attackers : defenders;
    const auto &losers = attackersWin ? defenders : attackers;
    const auto warId = nextWarId++;
    WarEvent war{warId, nextYear, attackers, defenders, winners, losers, {}};
    std::set<ProvinceId> transferred;
    settleTransfers(nextYear, configuration, normalized, state, territories,
                    attackers, winners, losers, warId, maritimeRange, append,
                    events, war, transferred, connectivity);
    if (configuration.enableWarfareDiagnostics && war.transferEventIndices.empty()) {
      std::ostringstream message;
      message << "War produced no territorial transfers in " << nextYear
              << ": attacker=" << attackerId << " defender=" << defenderId
              << " maritimeRange=" << maritimeRange;
      Fwg::Utils::Logging::logLine(message.str());
    }
    if (!war.transferEventIndices.empty())
      wars.push_back(std::move(war));
  }
  if (configuration.enableWarfareDiagnostics && navalChecksEnabled &&
      diagnostics.differentLandmassPairs > 0) {
    std::ostringstream message;
    message << "Naval war eligibility in " << nextYear
            << ": range=" << maritimeRange
            << " differentLandmassPairs=" << diagnostics.differentLandmassPairs
            << " connections=" << diagnostics.navalConnections
            << " rangeFailures=" << diagnostics.navalRangeFailures
            << " missingLandMassData=" << diagnostics.missingLandMassData
            << " defenderSelections=" << diagnostics.navalDefenderSelections;
    Fwg::Utils::Logging::logLine(message.str());
  }
}

} // namespace Arda::Simulation::warfare
