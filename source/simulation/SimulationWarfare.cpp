#include "simulation/SimulationWarfare.h"
#include "simulation/SimulationState.h"

#include "RandNum.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace {

bool chance(double probability) {
  return probability > 0.0 && RandNum::getRandom<double>(1.0) < probability;
}

}

namespace Arda::Simulation::warfare {
namespace {

bool hasMaritimeWarConnectionOneWay(PolityId left, PolityId right,
                                    const State &state,
                                    const detail::NormalizedInput &normalized,
                                    double maritimeRange) {
  for (const auto &[sourceId, routes] : normalized.seaRoutesFrom) {
    const auto source = state.provinces.find(sourceId);
    if (source == state.provinces.end() || source->second.owner != left ||
        !source->second.coastal)
      continue;
    for (const auto &route : routes) {
      if (route.distance > maritimeRange)
        break;
      const auto target = state.provinces.find(route.provinceId);
      if (target != state.provinces.end() && target->second.owner == right)
        return true;
    }
  }
  return false;
}

} // namespace

double expansionBorderScore(
    ProvinceId targetProvince, PolityId attacker,
    const std::map<ProvinceId, std::vector<ProvinceId>> &neighbours,
    const std::map<ProvinceId, ProvinceState> &provinces) {
  const auto &targetNeighbours = neighbours.at(targetProvince);
  double score = static_cast<double>(targetNeighbours.size());
  for (const auto neighbourId : targetNeighbours) {
    const auto &neighbour = provinces.at(neighbourId);
    if (neighbour.owner == attacker)
      score += 3.0;

    size_t attackerNeighbours = 0;
    size_t foreignNeighbours = 0;
    for (const auto surroundingId : neighbours.at(neighbourId)) {
      const auto &surrounding = provinces.at(surroundingId);
      if (surrounding.owner == attacker)
        ++attackerNeighbours;
      else if (surrounding.owner == neighbour.owner)
        ++foreignNeighbours;
    }
    if (foreignNeighbours > 0 && attackerNeighbours + 1 >= foreignNeighbours)
      score -= 8.0;
    if (foreignNeighbours == 1 && attackerNeighbours > 0)
      score -= 12.0;
  }
  return score;
}

double expansionTargetWeakness(
    PolityId attacker, PolityId defender,
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
    const std::map<ProvinceId, ProvinceState> &provinces) {
  std::vector<ProvinceId> remaining;
  remaining.reserve(provinces.size());
  for (const auto &[provinceId, province] : provinces)
    if (province.owner == owner && provinceId != removedProvince)
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

double polityDistance(
    PolityId left, PolityId right, const State &state,
    const std::map<ProvinceId, std::shared_ptr<ArdaProvince>> &provinces) {
  const auto leftPolity = state.polities.find(left);
  const auto rightPolity = state.polities.find(right);
  if (leftPolity == state.polities.end() ||
      rightPolity == state.polities.end() ||
      leftPolity->second.capitalProvince < 0 ||
      rightPolity->second.capitalProvince < 0)
    return std::numeric_limits<double>::max();

  const auto leftProvince = provinces.find(leftPolity->second.capitalProvince);
  const auto rightProvince =
      provinces.find(rightPolity->second.capitalProvince);
  if (leftProvince == provinces.end() || rightProvince == provinces.end() ||
      !leftProvince->second || !rightProvince->second)
    return std::numeric_limits<double>::max();

  return provinceCenterDistance(*leftProvince->second, *rightProvince->second);
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
                              const detail::NormalizedInput &normalized,
                              double maritimeRange) {
  return hasMaritimeWarConnectionOneWay(left, right, state, normalized,
                                        maritimeRange) ||
         hasMaritimeWarConnectionOneWay(right, left, state, normalized,
                                        maritimeRange);
}

bool capitalsShareLandMass(
    PolityId left, PolityId right, const State &state,
    const std::map<ProvinceId, std::shared_ptr<ArdaProvince>> &provinces) {
  const auto leftPolity = state.polities.find(left);
  const auto rightPolity = state.polities.find(right);
  if (leftPolity == state.polities.end() || rightPolity == state.polities.end())
    return false;

  const auto leftProvince = provinces.find(leftPolity->second.capitalProvince);
  const auto rightProvince =
      provinces.find(rightPolity->second.capitalProvince);
  if (leftProvince == provinces.end() || rightProvince == provinces.end() ||
      !leftProvince->second || !rightProvince->second ||
      leftProvince->second->landMassID < 0 ||
      rightProvince->second->landMassID < 0)
    return false;

  return leftProvince->second->landMassID == rightProvince->second->landMassID;
}

void resolveWars(const Year nextYear, double centuries, const Configuration &configuration, const detail::NormalizedInput &normalized, State &state, const detail::AppendEvent &append, std::vector<WarEvent> &wars, int &nextWarId, const std::vector<Event> &events, const std::map<ProvinceId, std::shared_ptr<ArdaProvince>> &inputProvinces) {
  const std::map<PolityId, std::vector<ProvinceId>> territories =
      Arda::Simulation::state::territoriesByPolity(state);

  std::set<PolityId> committed;
  const auto maritimeRange = maritimeRangeForYear(nextYear, configuration);
  size_t differentLandmassPairs = 0;
  size_t navalConnections = 0;
  size_t navalRangeFailures = 0;
  size_t missingLandMassData = 0;
  size_t navalDefenderSelections = 0;
  size_t stateCoastalProvinces = 0;
  for (const auto &[provinceId, province] : state.provinces)
    if (province.coastal)
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
        const auto source = state.provinces.find(sourceId);
        const auto target = state.provinces.find(route.provinceId);
        if (source == state.provinces.end() || target == state.provinces.end())
          continue;
        if (source->second.owner != NoPolity)
          ++ownedSourceRoutes;
        if (target->second.owner != NoPolity)
          ++ownedTargetRoutes;
        if (source->second.owner != NoPolity &&
            target->second.owner != NoPolity &&
            source->second.owner != target->second.owner)
          ++crossPolityRoutes;
        const auto sourceProvince = inputProvinces.find(sourceId);
        const auto targetProvince = inputProvinces.find(route.provinceId);
        if (sourceProvince != inputProvinces.end() &&
            targetProvince != inputProvinces.end() && sourceProvince->second &&
            targetProvince->second && sourceProvince->second->landMassID >= 0 &&
            targetProvince->second->landMassID >= 0 &&
            sourceProvince->second->landMassID !=
                targetProvince->second->landMassID) {
          ++crossLandMassRoutes;
          if (source->second.owner != NoPolity &&
              target->second.owner != NoPolity &&
              source->second.owner != target->second.owner) {
            ++crossLandMassCrossPolityRoutes;
            if (!crossLandMassSampleLogged) {
              std::ostringstream crossLandMassSample;
              crossLandMassSample
                  << "Cross-landmass route ownership sample in " << nextYear
                  << ": source=" << sourceId
                  << " sourceOwner=" << source->second.owner
                  << " sourceLandMass=" << sourceProvince->second->landMassID
                  << " target=" << route.provinceId
                  << " targetOwner=" << target->second.owner
                  << " targetLandMass=" << targetProvince->second->landMassID
                  << " distance=" << route.distance;
              Fwg::Utils::Logging::logLine(crossLandMassSample.str());
              crossLandMassSampleLogged = true;
            }
          }
        }
        if (!sampleLogged) {
          std::ostringstream sample;
          sample << "Naval route ownership sample in " << nextYear
                 << ": source=" << sourceId
                 << " sourceOwner=" << source->second.owner << " sourceCoastal="
                 << (source->second.coastal ? "true" : "false")
                 << " target=" << route.provinceId
                 << " targetOwner=" << target->second.owner
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
  for (const auto &[attackerId, attackerTerritory] : territories) {
    if (committed.contains(attackerId) || attackerTerritory.empty())
      continue;
    PolityId defenderId = NoPolity;
    auto closestDistance = std::numeric_limits<double>::max();
    for (const auto &[candidateId, candidateTerritory] : territories) {
      if (candidateId == attackerId || committed.contains(candidateId) ||
          candidateTerritory.empty())
        continue;
      const auto distance =
          polityDistance(attackerId, candidateId, state, inputProvinces);
      const auto maritimeConnection =
          navalChecksEnabled && maritimeRange > 0.0 &&
          hasMaritimeWarConnection(attackerId, candidateId, state, normalized,
                                   maritimeRange);
      const auto sharedLandMass =
          capitalsShareLandMass(attackerId, candidateId, state, inputProvinces);
      const auto attackerPolity = state.polities.find(attackerId);
      const auto candidatePolity = state.polities.find(candidateId);
      const auto attackerCapital =
          attackerPolity == state.polities.end()
              ? inputProvinces.end()
              : inputProvinces.find(attackerPolity->second.capitalProvince);
      const auto candidateCapital =
          candidatePolity == state.polities.end()
              ? inputProvinces.end()
              : inputProvinces.find(candidatePolity->second.capitalProvince);
      const auto capitalsHaveDifferentLandmasses =
          attackerCapital != inputProvinces.end() &&
          candidateCapital != inputProvinces.end() && attackerCapital->second &&
          candidateCapital->second &&
          attackerCapital->second->landMassID >= 0 &&
          candidateCapital->second->landMassID >= 0 &&
          attackerCapital->second->landMassID !=
              candidateCapital->second->landMassID;
      if (navalChecksEnabled && capitalsHaveDifferentLandmasses &&
          !representativePairLogged) {
        size_t ownedCoastalSources = 0;
        size_t ownedSourceRoutes = 0;
        double shortestOwnedRoute = std::numeric_limits<double>::max();
        for (const auto &[sourceId, routes] : normalized.seaRoutesFrom) {
          const auto source = state.provinces.find(sourceId);
          if (source == state.provinces.end() ||
              source->second.owner != attackerId || !source->second.coastal)
            continue;
          ++ownedCoastalSources;
          ownedSourceRoutes += routes.size();
          for (const auto &route : routes)
            shortestOwnedRoute = std::min(shortestOwnedRoute, route.distance);
        }
        std::ostringstream message;
        message << "Naval representative pair in " << nextYear
                << ": attacker=" << attackerId << " defender=" << candidateId
                << " attackerCapital=" << attackerPolity->second.capitalProvince
                << " defenderCapital="
                << candidatePolity->second.capitalProvince
                << " attackerLandMass=" << attackerCapital->second->landMassID
                << " defenderLandMass=" << candidateCapital->second->landMassID
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
        ++differentLandmassPairs;
        if (maritimeConnection)
          ++navalConnections;
        else
          ++navalRangeFailures;
      } else if (navalChecksEnabled &&
                 (attackerCapital == inputProvinces.end() ||
                  candidateCapital == inputProvinces.end() ||
                  !attackerCapital->second || !candidateCapital->second ||
                  attackerCapital->second->landMassID < 0 ||
                  candidateCapital->second->landMassID < 0)) {
        ++missingLandMassData;
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
          ++navalDefenderSelections;
      }
    }
    if (defenderId == NoPolity)
      continue;
    const auto warProbability =
        1.0 - std::exp(-configuration.warChancePerCentury * centuries);
    if (!chance(warProbability))
      continue;

    std::vector<PolityId> attackers{attackerId};
    std::vector<PolityId> defenders{defenderId};
    committed.insert(attackerId);
    committed.insert(defenderId);
    for (const auto &[candidateId, candidateTerritory] : territories) {
      if (committed.contains(candidateId) || candidateTerritory.empty() ||
          attackers.size() >= configuration.maximumWarAllianceMembers &&
              defenders.size() >= configuration.maximumWarAllianceMembers)
        continue;
      const auto attackerDistance =
          polityDistance(candidateId, attackerId, state, inputProvinces);
      const auto defenderDistance =
          polityDistance(candidateId, defenderId, state, inputProvinces);
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
      committed.insert(candidateId);
    }

    auto sideStrength = [&state](const std::vector<PolityId> &side) {
      return std::accumulate(
          side.begin(), side.end(), 0.0,
          [&state](double total, PolityId polityId) {
            const auto strength = state.polityStrengths.find(polityId);
            return total + (strength == state.polityStrengths.end()
                                ? 0.0
                                : strength->second.score);
          });
    };
    const auto attackersWin =
        sideStrength(attackers) >= sideStrength(defenders);
    const auto &winners = attackersWin ? attackers : defenders;
    const auto &losers = attackersWin ? defenders : attackers;
    std::set<int> attackerContinents;
    std::set<int> defenderContinents;
    for (const auto &[provinceId, province] : state.provinces) {
      const auto continent = normalized.provinceContinents.find(provinceId);
      if (continent == normalized.provinceContinents.end())
        continue;
      if (std::find(attackers.begin(), attackers.end(), province.owner) !=
          attackers.end())
        attackerContinents.insert(continent->second);
      if (std::find(defenders.begin(), defenders.end(), province.owner) !=
          defenders.end())
        defenderContinents.insert(continent->second);
    }
    bool differentLandmasses = true;
    for (const auto continent : attackerContinents)
      if (defenderContinents.contains(continent)) {
        differentLandmasses = false;
        break;
      }
    if (differentLandmasses) {
      std::ostringstream message;
      message << "War across landmasses in " << nextYear << ": attackers=";
      for (const auto polityId : attackers)
        message << polityId << ' ';
      message << "defenders=";
      for (const auto polityId : defenders)
        message << polityId << ' ';
      message << " attackerContinents=";
      for (const auto continent : attackerContinents)
        message << continent << ' ';
      message << "defenderContinents=";
      for (const auto continent : defenderContinents)
        message << continent << ' ';
      message << " navalChecks="
              << (maritimeRange > 0.0 ? "enabled" : "disabled");
      Fwg::Utils::Logging::logLine(message.str());
    }
    const auto warId = nextWarId++;
    WarEvent war{warId, nextYear, attackers, defenders, winners, losers, {}};
    std::set<ProvinceId> transferred;
    std::vector<std::pair<ProvinceId, double>> landCandidates;
    std::map<ProvinceId, PolityId> landCandidateWinners;
    for (const auto loser : losers) {
      for (const auto provinceId : territories.at(loser)) {
        if (transferred.contains(provinceId))
          continue;
        size_t borderScore = 0;
        for (const auto neighbourId : normalized.neighbours.at(provinceId))
          if (std::find(winners.begin(), winners.end(),
                        state.provinces.at(neighbourId).owner) != winners.end())
            ++borderScore;
        if (borderScore == 0)
          continue;
        std::vector<PolityId> borderingWinners;
        for (const auto neighbourId : normalized.neighbours.at(provinceId)) {
          const auto owner = state.provinces.at(neighbourId).owner;
          if (std::find(winners.begin(), winners.end(), owner) !=
                  winners.end() &&
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
        const auto defenderStrength =
            state.polityStrengths.contains(loser)
                ? state.polityStrengths.at(loser).score
                : 0.0;
        const auto winnerBorderStrength = static_cast<double>(borderScore);
        const auto peripheralScore =
            1.0 /
            std::max(1.0, static_cast<double>(
                              normalized.neighbours.at(provinceId).size()));
        landCandidates.emplace_back(
            provinceId, winnerBorderStrength * 4.0 + peripheralScore * 2.0 +
                            1.0 / std::max(1.0, defenderStrength));
        landCandidateWinners[provinceId] = winner;
      }
    }
    std::sort(landCandidates.begin(), landCandidates.end(),
              [](const auto &left, const auto &right) {
                return left.second > right.second;
              });
    for (const auto &[provinceId, score] : landCandidates) {
      const auto landTransferCount = war.transferEventIndices.size();
      if (landTransferCount >= std::min(configuration.maximumWarTransfers,
                                        configuration.maximumWarLandTransfers))
        break;
      const auto loser = state.provinces.at(provinceId).owner;
      if (!remainsContiguousAfterConquest(
              loser, provinceId, normalized.neighbours, state.provinces))
        continue;
      const auto winner = landCandidateWinners.at(provinceId);
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
                     "war territorial settlement"};
      transfer.warId = warId;
      transfer.overseas = state.provinces.at(provinceId).overseas;
      transfer.colony = state.provinces.at(provinceId).colony;
      const auto previousEventCount = events.size();
      append(std::move(transfer));
      if (events.size() > previousEventCount)
        war.transferEventIndices.push_back(events.size() - 1);
      transferred.insert(provinceId);
    }
    size_t maritimeTransferCount = 0;
    if (maritimeRange > 0.0 &&
        war.transferEventIndices.size() < configuration.maximumWarTransfers &&
        configuration.maximumWarMaritimeTransfers > 0) {
      for (const auto loser : losers) {
        for (const auto provinceId : territories.at(loser)) {
          if (transferred.contains(provinceId) ||
              (!state.provinces.at(provinceId).island &&
               !state.provinces.at(provinceId).overseas))
            continue;
          const auto routes = normalized.seaRoutesTo.find(provinceId);
          if (routes == normalized.seaRoutesTo.end())
            continue;
          std::vector<PolityId> reachableWinners;
          for (const auto &route : routes->second) {
            if (route.distance > maritimeRange)
              continue;
            const auto source = state.provinces.find(route.provinceId);
            if (source == state.provinces.end())
              continue;
            if (std::find(winners.begin(), winners.end(),
                          source->second.owner) != winners.end() &&
                std::find(reachableWinners.begin(), reachableWinners.end(),
                          source->second.owner) == reachableWinners.end())
              reachableWinners.push_back(source->second.owner);
          }
          if (reachableWinners.empty())
            continue;
          const auto winner = *std::max_element(
              reachableWinners.begin(), reachableWinners.end(),
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
          transfer.colony = state.provinces.at(provinceId).colony;
          const auto previousEventCount = events.size();
          append(std::move(transfer));
          if (events.size() > previousEventCount)
            war.transferEventIndices.push_back(events.size() - 1);
          transferred.insert(provinceId);
          ++maritimeTransferCount;
          if (war.transferEventIndices.size() >=
                  configuration.maximumWarTransfers ||
              maritimeTransferCount >=
                  configuration.maximumWarMaritimeTransfers)
            break;
        }
        if (war.transferEventIndices.size() >=
                configuration.maximumWarTransfers ||
            maritimeTransferCount >= configuration.maximumWarMaritimeTransfers)
          break;
      }
    }
    if (!war.transferEventIndices.empty())
      wars.push_back(std::move(war));
  }
  if (navalChecksEnabled && differentLandmassPairs > 0) {
    std::ostringstream message;
    message << "Naval war eligibility in " << nextYear
            << ": range=" << maritimeRange
            << " differentLandmassPairs=" << differentLandmassPairs
            << " connections=" << navalConnections
            << " rangeFailures=" << navalRangeFailures
            << " missingLandMassData=" << missingLandMassData
            << " defenderSelections=" << navalDefenderSelections;
    Fwg::Utils::Logging::logLine(message.str());
  }
}

} // namespace Arda::Simulation::warfare
