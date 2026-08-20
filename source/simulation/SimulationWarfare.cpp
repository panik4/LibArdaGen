#include "simulation/SimulationWarfare.h"
#include "simulation/SimulationState.h"

#include "RandNum.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace Arda::Simulation::warfare {
namespace {

using MaritimeReachability = std::vector<std::vector<bool>>;

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

MaritimeReachability
buildMaritimeReachability(const detail::NormalizedInput &normalized,
                          double maritimeRange) {
  ProvinceId maximumProvinceId = -1;
  for (const auto &[sourceId, routes] : normalized.seaRoutesFrom) {
    maximumProvinceId = std::max(maximumProvinceId, sourceId);
    for (const auto &route : routes)
      maximumProvinceId = std::max(maximumProvinceId, route.provinceId);
  }
  if (maximumProvinceId < 0)
    return {};

  const auto provinceCount = static_cast<std::size_t>(maximumProvinceId) + 1;
  MaritimeReachability reachability(provinceCount,
                                    std::vector<bool>(provinceCount, false));
  for (const auto &[sourceId, routes] : normalized.seaRoutesFrom) {
    if (sourceId < 0 || sourceId > maximumProvinceId)
      continue;
    for (const auto &route : routes) {
      if (route.distance > maritimeRange)
        break;
      if (route.provinceId >= 0 && route.provinceId <= maximumProvinceId)
        reachability[static_cast<std::size_t>(sourceId)]
                    [static_cast<std::size_t>(route.provinceId)] = true;
    }
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
  std::vector<ProvinceId> remaining;
  remaining.reserve(provinces.size());
  for (ProvinceId provinceId = 0;
       provinceId < static_cast<ProvinceId>(provinces.size()); ++provinceId)
    if (provinces[static_cast<std::size_t>(provinceId)].initialized &&
        provinces[static_cast<std::size_t>(provinceId)].owner == owner &&
        provinceId != removedProvince)
      remaining.push_back(provinceId);
  if (remaining.size() < 2)
    return true;

  std::set<ProvinceId> unvisited(remaining.begin(), remaining.end());
  std::vector<ProvinceId> frontier{remaining.front()};
  unvisited.erase(remaining.front());
  while (!frontier.empty()) {
    const auto provinceId = frontier.back();
    frontier.pop_back();
    for (const auto neighbourId : neighbours.at(provinceId))
      if (unvisited.erase(neighbourId) > 0)
        frontier.push_back(neighbourId);
  }
  return unvisited.empty();
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
  if (year < configuration.maritimeExpansionStartYear)
    return 0.0;
  const auto centuriesSinceStart = std::max(
      0.0,
      static_cast<double>(year - configuration.maritimeExpansionStartYear) /
          100.0);
  const auto preRenaissanceRange =
      configuration.initialMaritimeRange *
      std::pow(1.0 + configuration.maritimeRangeGrowthPerCentury,
               centuriesSinceStart);
  if (year < configuration.renaissanceStartYear)
    return std::max(1.0, preRenaissanceRange);
  const auto renaissanceYears = std::max(
      0.0,
      static_cast<double>(year - configuration.renaissanceStartYear) / 100.0);
  return std::max(
      1.0,
      preRenaissanceRange * configuration.renaissanceMaritimeRangeMultiplier *
          std::pow(1.0 + configuration.renaissanceMaritimeRangeGrowthPerCentury,
                   renaissanceYears));
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

std::vector<CapitalCacheEntry> buildCapitalCache(const State &state) {
  std::vector<CapitalCacheEntry> cache(state.polities.size());
  for (PolityId polityId = 0;
       polityId < static_cast<PolityId>(state.polities.size()); ++polityId) {
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
                        const std::vector<char> &committed,
                        const Configuration &configuration,
                        const detail::NormalizedInput &normalized,
                        const State &state, Year nextYear, double landWarRange,
                        double maritimeRange,
                        const MaritimeReachability &maritimeReachability,
                        bool navalChecksEnabled, bool &representativePairLogged,
                        NavalDiagnostics &diagnostics,
                        const std::vector<CapitalCacheEntry> &capitalCache) {
  PolityId defenderId = NoPolity;
  auto closestDistance = std::numeric_limits<double>::max();
  std::vector<PolityId> candidateIds;
  if (nextYear < configuration.maritimeExpansionStartYear) {
    std::vector<char> candidateSeen(territories.size(), false);
    if (attackerId >= 0 &&
        static_cast<std::size_t>(attackerId) < territories.size()) {
      for (const auto provinceId :
           territories[static_cast<std::size_t>(attackerId)]) {
        const auto neighbours = normalized.neighbours.find(provinceId);
        if (neighbours == normalized.neighbours.end())
          continue;
        for (const auto neighbourId : neighbours->second) {
          const auto *neighbour = state::findProvince(state, neighbourId);
          if (!neighbour || neighbour->owner < 0 ||
              neighbour->owner == attackerId ||
              static_cast<std::size_t>(neighbour->owner) >=
                  candidateSeen.size() ||
              candidateSeen[static_cast<std::size_t>(neighbour->owner)])
            continue;
          candidateSeen[static_cast<std::size_t>(neighbour->owner)] = true;
          candidateIds.push_back(neighbour->owner);
        }
      }
    }
  } else {
    candidateIds.resize(territories.size());
    std::iota(std::begin(candidateIds), std::end(candidateIds), 0);
  }

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
    if (sharedLandMass && distance > landWarRange)
      continue;

    bool maritimeConnection = false;
    if (navalChecksEnabled && maritimeRange > 0.0 && !sharedLandMass)
      maritimeConnection = hasMaritimeWarConnection(
          attackerId, candidateId, state, maritimeReachability);
    if (navalChecksEnabled && capitalsHaveDifferentLandmasses &&
        !representativePairLogged) {
      const auto *attackerPolity = state::findPolity(state, attackerId);
      const auto *candidatePolity = state::findPolity(state, candidateId);
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
    if (navalChecksEnabled && capitalsHaveDifferentLandmasses) {
      ++diagnostics.differentLandmassPairs;
      if (maritimeConnection)
        ++diagnostics.navalConnections;
      else
        ++diagnostics.navalRangeFailures;
    } else if (navalChecksEnabled && (attackerCapital.landMassID < 0 ||
                                      candidateCapital.landMassID < 0)) {
      ++diagnostics.missingLandMassData;
    }
    const auto candidateDistance = sharedLandMass ? distance
                                   : maritimeConnection
                                       ? configuration.warMaximumDistance
                                       : std::numeric_limits<double>::max();
    const auto geographicallyReachable =
        navalChecksEnabled ? (sharedLandMass || maritimeConnection)
                           : sharedLandMass;
    if (geographicallyReachable && candidateDistance < closestDistance) {
      closestDistance = candidateDistance;
      defenderId = candidateId;
      if (capitalsHaveDifferentLandmasses)
        ++diagnostics.navalDefenderSelections;
    }
  }
  return defenderId;
}

void formAlliances(PolityId attackerId, PolityId defenderId,
                   const std::vector<std::vector<ProvinceId>> &territories,
                   std::vector<char> &committed,
                   const Configuration &configuration, const State &state,
                   std::vector<PolityId> &attackers,
                   std::vector<PolityId> &defenders) {
  attackers = {attackerId};
  defenders = {defenderId};
  committed[attackerId] = true;
  committed[defenderId] = true;
  for (PolityId candidateId = 0;
       candidateId < static_cast<PolityId>(territories.size()); ++candidateId) {
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

void settleLandTransfers(
    Year nextYear, const Configuration &configuration,
    const detail::NormalizedInput &normalized, const State &state,
    const std::vector<std::vector<ProvinceId>> &territories,
    const std::vector<PolityId> &winners, const std::vector<PolityId> &losers,
    int warId, const detail::AppendEvent &append,
    const std::vector<Event> &events, WarEvent &war,
    std::set<ProvinceId> &transferred) {
  std::vector<std::pair<ProvinceId, double>> candidates;
  std::map<ProvinceId, PolityId> candidateWinners;
  for (const auto loser : losers) {
    if (loser < 0 || static_cast<std::size_t>(loser) >= territories.size())
      continue;
    for (const auto provinceId : territories[static_cast<std::size_t>(loser)]) {
      if (transferred.contains(provinceId))
        continue;
      size_t borderScore = 0;
      for (const auto neighbourId : normalized.neighbours.at(provinceId))
        if (const auto *neighbour = state::findProvince(state, neighbourId);
            neighbour && std::find(winners.begin(), winners.end(),
                                   neighbour->owner) != winners.end())
          ++borderScore;
      if (borderScore == 0)
        continue;
      std::vector<PolityId> borderingWinners;
      for (const auto neighbourId : normalized.neighbours.at(provinceId)) {
        const auto *neighbour = state::findProvince(state, neighbourId);
        if (!neighbour)
          continue;
        const auto owner = neighbour->owner;
        if (std::find(winners.begin(), winners.end(), owner) != winners.end() &&
            std::find(borderingWinners.begin(), borderingWinners.end(),
                      owner) == borderingWinners.end())
          borderingWinners.push_back(owner);
      }
      if (borderingWinners.empty())
        continue;
      const auto winner =
          *std::max_element(borderingWinners.begin(), borderingWinners.end(),
                            [&state](PolityId left, PolityId right) {
                              return state.polityStrengths.at(left).score <
                                     state.polityStrengths.at(right).score;
                            });
      const auto defenderStrength = state.polityStrengths.contains(loser)
                                        ? state.polityStrengths.at(loser).score
                                        : 0.0;
      const auto peripheralScore =
          1.0 / std::max(1.0, static_cast<double>(
                                  normalized.neighbours.at(provinceId).size()));
      candidates.emplace_back(provinceId,
                              static_cast<double>(borderScore) * 4.0 +
                                  peripheralScore * 2.0 +
                                  1.0 / std::max(1.0, defenderStrength));
      candidateWinners[provinceId] = winner;
    }
  }
  std::sort(candidates.begin(), candidates.end(),
            [](const auto &left, const auto &right) {
              return left.second > right.second;
            });
  for (const auto &[provinceId, score] : candidates) {
    (void)score;
    if (war.transferEventIndices.size() >=
        std::min(configuration.maximumWarTransfers,
                 configuration.maximumWarLandTransfers))
      break;
    const auto *province = state::findProvince(state, provinceId);
    if (!province ||
        !remainsContiguousAfterConquest(province->owner, provinceId,
                                        normalized.neighbours, state.provinces))
      continue;
    Event transfer{nextYear,
                   EventType::TransferProvince,
                   provinceId,
                   normalized.provinceRegions.at(provinceId),
                   candidateWinners.at(provinceId),
                   province->owner,
                   -1,
                   NoReligion,
                   0.0,
                   0.0,
                   "war territorial settlement"};
    transfer.warId = warId;
    transfer.overseas = province->overseas;
    transfer.colony = province->colony;
    const auto previousEventCount = events.size();
    append(std::move(transfer));
    if (events.size() > previousEventCount)
      war.transferEventIndices.push_back(events.size() - 1);
    transferred.insert(provinceId);
  }
}

void settleMaritimeTransfers(
    Year nextYear, const Configuration &configuration,
    const detail::NormalizedInput &normalized, const State &state,
    const std::vector<std::vector<ProvinceId>> &territories,
    const std::vector<PolityId> &winners, const std::vector<PolityId> &losers,
    int warId, double maritimeRange, const detail::AppendEvent &append,
    const std::vector<Event> &events, WarEvent &war,
    std::set<ProvinceId> &transferred) {
  if (maritimeRange <= 0.0 ||
      war.transferEventIndices.size() >= configuration.maximumWarTransfers ||
      configuration.maximumWarMaritimeTransfers == 0)
    return;
  size_t maritimeTransferCount = 0;
  for (const auto loser : losers) {
    if (loser < 0 || static_cast<std::size_t>(loser) >= territories.size())
      continue;
    for (const auto provinceId : territories[static_cast<std::size_t>(loser)]) {
      const auto *province = state::findProvince(state, provinceId);
      if (!province || transferred.contains(provinceId) ||
          (!province->island && !province->overseas))
        continue;
      const auto routes = normalized.seaRoutesTo.find(provinceId);
      if (routes == normalized.seaRoutesTo.end())
        continue;
      std::vector<PolityId> reachableWinners;
      for (const auto &route : routes->second) {
        if (route.distance > maritimeRange)
          continue;
        const auto *source = state::findProvince(state, route.provinceId);
        if (!source)
          continue;
        if (std::find(winners.begin(), winners.end(), source->owner) !=
                winners.end() &&
            std::find(reachableWinners.begin(), reachableWinners.end(),
                      source->owner) == reachableWinners.end())
          reachableWinners.push_back(source->owner);
      }
      if (reachableWinners.empty())
        continue;
      const auto winner =
          *std::max_element(reachableWinners.begin(), reachableWinners.end(),
                            [&state](PolityId left, PolityId right) {
                              return state.polityStrengths.at(left).score <
                                     state.polityStrengths.at(right).score;
                            });
      Event transfer{nextYear,
                     EventType::TransferProvince,
                     provinceId,
                     normalized.provinceRegions.at(provinceId),
                     winner,
                     loser,
                     -1,
                     NoReligion,
                     0.0,
                     0.0,
                     "war overseas settlement"};
      transfer.warId = warId;
      transfer.overseas = true;
      transfer.colony = province->colony;
      const auto previousEventCount = events.size();
      append(std::move(transfer));
      if (events.size() > previousEventCount)
        war.transferEventIndices.push_back(events.size() - 1);
      transferred.insert(provinceId);
      ++maritimeTransferCount;
      if (war.transferEventIndices.size() >=
              configuration.maximumWarTransfers ||
          maritimeTransferCount >= configuration.maximumWarMaritimeTransfers)
        break;
    }
    if (war.transferEventIndices.size() >= configuration.maximumWarTransfers ||
        maritimeTransferCount >= configuration.maximumWarMaritimeTransfers)
      break;
  }
}

void resolveWars(const Year nextYear, double centuries,
                 const Configuration &configuration,
                 const detail::NormalizedInput &normalized, State &state,
                 const detail::AppendEvent &append, std::vector<WarEvent> &wars,
                 int &nextWarId, const std::vector<Event> &events) {
  const auto territories = Arda::Simulation::state::territoriesByPolity(state);

  std::vector<char> committed(state.polities.size(), false);
  const auto landWarRange = landWarRangeForYear(nextYear, configuration);
  const auto maritimeRange = maritimeRangeForYear(nextYear, configuration);
  const auto maritimeReachability =
      maritimeRange > 0.0 ? buildMaritimeReachability(normalized, maritimeRange)
                          : MaritimeReachability{};
  const auto capitalCache = buildCapitalCache(state);
  NavalDiagnostics diagnostics;
  size_t stateCoastalProvinces = 0;
  for (const auto &province : state.provinces)
    if (province.initialized && province.coastal)
      ++stateCoastalProvinces;
  {
    size_t routeCount = 0;
    size_t ownedSourceRoutes = 0;
    size_t ownedTargetRoutes = 0;
    size_t crossPolityRoutes = 0;
    size_t crossLandMassRoutes = 0;
    size_t crossLandMassCrossPolityRoutes = 0;
    bool crossLandMassSampleLogged = false;
    bool sampleLogged = false;
    for (const auto &[sourceId, routes] : normalized.seaRoutesFrom)
      for (const auto &route : routes) {
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
  for (PolityId attackerId = 0;
       attackerId < static_cast<PolityId>(territories.size()); ++attackerId) {
    const auto &attackerTerritory =
        territories[static_cast<std::size_t>(attackerId)];
    if (committed.at(attackerId) || attackerTerritory.empty())
      continue;
    const auto defenderId =
        selectDefender(attackerId, territories, committed, configuration,
                       normalized, state, nextYear, landWarRange, maritimeRange,
                       maritimeReachability, navalChecksEnabled,
                       representativePairLogged, diagnostics, capitalCache);
    if (defenderId == NoPolity)
      continue;
    const auto warProbability =
        1.0 - std::exp(-configuration.warChancePerCentury * centuries);
    if (!chance(warProbability))
      continue;

    std::vector<PolityId> attackers;
    std::vector<PolityId> defenders;
    formAlliances(attackerId, defenderId, territories, committed, configuration,
                  state, attackers, defenders);
    const auto attackersWin =
        sideStrength(state, attackers) >= sideStrength(state, defenders);
    const auto &winners = attackersWin ? attackers : defenders;
    const auto &losers = attackersWin ? defenders : attackers;
    const auto warId = nextWarId++;
    WarEvent war{warId, nextYear, attackers, defenders, winners, losers, {}};
    std::set<ProvinceId> transferred;
    settleLandTransfers(nextYear, configuration, normalized, state, territories,
                        winners, losers, warId, append, events, war,
                        transferred);
    if (maritimeRange > 0.0)
      settleMaritimeTransfers(nextYear, configuration, normalized, state,
                              territories, winners, losers, warId, maritimeRange,
                              append, events, war, transferred);
    if (!war.transferEventIndices.empty())
      wars.push_back(std::move(war));
  }
  if (navalChecksEnabled && diagnostics.differentLandmassPairs > 0) {
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
