#include "simulation/SimulationPolity.h"

#include "RandNum.h"
#include "simulation/SimulationDevelopment.h"
#include "simulation/SimulationState.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <set>

namespace Arda::Simulation::polity {
namespace {

using detail::AppendEvent;
using detail::NormalizedInput;

bool chance(double probability) {
  return probability > 0.0 && RandNum::getRandom<double>(1.0) < probability;
}

PoliticalEra eraFor(Year year, const Configuration &configuration);
double traitAgeMultiplier(PoliticalEra traitEra, PoliticalEra currentEra,
                          const Configuration &configuration);
PoliticalTraitDefinition traitDefinition(PoliticalTrait trait);

double governmentTraitAffinity(const Polity &polity, GovernmentForm form,
                               PoliticalEra currentEra,
                               const Configuration &configuration) {
  double affinity = 0.0;
  const auto add = [&affinity, &polity, currentEra,
                    &configuration](PoliticalTrait trait, double weight) {
    for (const auto &traitState : polity.politicalTraits)
      if (traitState.trait == trait)
        affinity += weight * traitState.strength *
                    traitAgeMultiplier(traitDefinition(trait).era, currentEra,
                                       configuration);
  };
  const auto addDemocratic = [&add](double weight) {
    add(PoliticalTrait::LiberalReform, weight);
    add(PoliticalTrait::ParliamentaryTradition, weight);
    add(PoliticalTrait::ConstitutionalTradition, weight);
    add(PoliticalTrait::RepublicanTradition, weight);
    add(PoliticalTrait::LiberalDemocracy, weight);
    add(PoliticalTrait::SocialDemocracy, weight);
    add(PoliticalTrait::SocialReform, weight * 0.75);
  };
  switch (form) {
  case GovernmentForm::ParliamentaryMonarchy:
  case GovernmentForm::ConstitutionalRepublic:
  case GovernmentForm::ParliamentaryRepublic:
  case GovernmentForm::DirectDemocracy:
    addDemocratic(1.0);
    break;
  case GovernmentForm::ConstitutionalMonarchy:
    add(PoliticalTrait::ConstitutionalMonarchy, 1.4);
    add(PoliticalTrait::ConstitutionalTradition, 0.8);
    add(PoliticalTrait::LiberalReform, 0.6);
    add(PoliticalTrait::ParliamentaryTradition, 0.5);
    break;
  case GovernmentForm::CommunistState:
    add(PoliticalTrait::CommunistMovement,
        configuration.governmentExplicitMovementAffinity);
    add(PoliticalTrait::SocialistMovement, 1.0);
    add(PoliticalTrait::RevolutionaryMovement, 0.75);
    add(PoliticalTrait::RevolutionarySentiment, 0.6);
    break;
  case GovernmentForm::FascistState:
    add(PoliticalTrait::FascistMovement,
        configuration.governmentExplicitMovementAffinity);
    add(PoliticalTrait::Nationalism, 0.9);
    add(PoliticalTrait::ReactionaryOrder, 0.7);
    add(PoliticalTrait::PopulistMovement, 0.5);
    break;
  case GovernmentForm::MilitaryGovernment:
    add(PoliticalTrait::MilitaryAuthoritarianism, 1.0);
    add(PoliticalTrait::MilitaryState, 0.9);
    add(PoliticalTrait::MilitaryStrongman, 0.7);
    add(PoliticalTrait::AuthoritarianModernization, 0.65);
    break;
  case GovernmentForm::AbsoluteMonarchy:
  case GovernmentForm::FeudalMonarchy:
  case GovernmentForm::AristocraticMonarchy:
  case GovernmentForm::ElectiveMonarchy:
  case GovernmentForm::TheocraticMonarchy:
    add(PoliticalTrait::PowerfulCrown, 0.8);
    add(PoliticalTrait::RoyalAbsolutism, 0.9);
    add(PoliticalTrait::FeudalOrder, 0.8);
    add(PoliticalTrait::WarriorAristocracy, 0.6);
    break;
  case GovernmentForm::Theocracy:
    add(PoliticalTrait::PriestlyAuthority, 0.9);
    add(PoliticalTrait::ReligiousEstablishment, 0.9);
    add(PoliticalTrait::ReligiousConservatism, 0.8);
    add(PoliticalTrait::EcclesiasticalAuthority, 0.8);
    break;
  case GovernmentForm::AristocraticRepublic:
  case GovernmentForm::OligarchicRepublic:
    add(PoliticalTrait::AristocraticRepublic, 0.9);
    add(PoliticalTrait::StrongEstates, 0.8);
    break;
  default:
    break;
  }
  return affinity *
         std::clamp(configuration.governmentTraitAffinityWeight, 0.0, 2.0);
}

PoliticalEra eraFor(Year year, const Configuration &configuration) {
  if (year >= configuration.ideologicalTransitionStartYear)
    return PoliticalEra::Modern;
  if (year >= configuration.industrialRevolutionStartYear)
    return PoliticalEra::Industrial;
  if (year >= configuration.renaissanceStartYear)
    return PoliticalEra::EarlyModern;
  if (year >= configuration.medievalStartYear)
    return PoliticalEra::Medieval;
  return PoliticalEra::AncientClassical;
}

double traitAgeMultiplier(PoliticalEra traitEra, PoliticalEra currentEra,
                          const Configuration &configuration) {
  const auto traitEraIndex = static_cast<int>(traitEra);
  const auto currentEraIndex = static_cast<int>(currentEra);
  const auto eraDistance = std::max(0, currentEraIndex - traitEraIndex);
  if (eraDistance == 0)
    return 1.0;
  return std::pow(std::clamp(configuration.olderEraTraitMultiplier, 0.0, 1.0),
                  static_cast<double>(eraDistance));
}

PoliticalTraitDefinition traitDefinition(PoliticalTrait trait) {
  using B = PoliticalBloc;
  const auto modifier = [](double central, double noble, double religious,
                           double people) {
    return PoliticalPowerDistribution{central, noble, religious, people};
  };
  switch (trait) {
  case PoliticalTrait::DivineRulership:
    return {trait, PoliticalEra::AncientClassical, modifier(20, -5, 10, -25)};
  case PoliticalTrait::AristocraticRepublic:
    return {trait, PoliticalEra::AncientClassical, modifier(-15, 30, -5, -10)};
  case PoliticalTrait::CivicTradition:
    return {trait, PoliticalEra::AncientClassical, modifier(-10, -5, 0, 15)};
  case PoliticalTrait::PriestlyAuthority:
    return {trait, PoliticalEra::AncientClassical, modifier(-5, -5, 30, -20)};
  case PoliticalTrait::MilitaryStrongman:
    return {trait, PoliticalEra::AncientClassical, modifier(20, -10, -5, -5),
            0.15};
  case PoliticalTrait::LocalAutonomy:
    return {trait, PoliticalEra::AncientClassical, modifier(-15, 15, 0, 0)};
  case PoliticalTrait::FeudalOrder:
    return {trait, PoliticalEra::Medieval, modifier(-10, 30, 5, -25)};
  case PoliticalTrait::PowerfulCrown:
    return {trait, PoliticalEra::Medieval, modifier(25, -15, 0, -10)};
  case PoliticalTrait::EcclesiasticalAuthority:
    return {trait, PoliticalEra::Medieval, modifier(-5, -5, 30, -20)};
  case PoliticalTrait::FragmentedRealm:
    return {trait, PoliticalEra::Medieval, modifier(-15, 20, 5, -10)};
  case PoliticalTrait::MerchantPrivileges:
    return {trait, PoliticalEra::Medieval, modifier(-5, -5, -5, 15)};
  case PoliticalTrait::CommunalTraditions:
    return {trait, PoliticalEra::Medieval, modifier(-10, -10, 0, 20)};
  case PoliticalTrait::WarriorAristocracy:
    return {trait, PoliticalEra::Medieval, modifier(-5, 25, -5, -15), 0.10};
  case PoliticalTrait::ImperialBureaucracy:
    return {trait, PoliticalEra::Medieval, modifier(20, -15, 5, -10), 0.05};
  case PoliticalTrait::RoyalAbsolutism:
    return {trait, PoliticalEra::EarlyModern, modifier(35, -15, -5, -15)};
  case PoliticalTrait::CentralizedAdministration:
    return {trait, PoliticalEra::EarlyModern, modifier(20, -15, 0, -5)};
  case PoliticalTrait::StrongEstates:
    return {trait, PoliticalEra::EarlyModern, modifier(-10, 20, 0, 5)};
  case PoliticalTrait::ReligiousEstablishment:
    return {trait, PoliticalEra::EarlyModern, modifier(5, -5, 20, -20)};
  case PoliticalTrait::CommercialElite:
    return {trait, PoliticalEra::EarlyModern, modifier(5, -10, -5, 10)};
  case PoliticalTrait::ColonialAdministration:
    return {trait, PoliticalEra::EarlyModern, modifier(15, 5, -5, -15)};
  case PoliticalTrait::EnlightenedReform:
    return {trait, PoliticalEra::EarlyModern, modifier(10, -10, -5, 15)};
  case PoliticalTrait::MilitaryState:
    return {trait, PoliticalEra::EarlyModern, modifier(20, 5, -10, -15), 0.20};
  case PoliticalTrait::ConstitutionalTradition:
    return {trait, PoliticalEra::EnlightenmentRevolutionary,
            modifier(-10, -10, -5, 25)};
  case PoliticalTrait::EnlightenedAbsolutism:
    return {trait, PoliticalEra::EnlightenmentRevolutionary,
            modifier(20, -10, -10, 0)};
  case PoliticalTrait::RepublicanTradition:
    return {trait, PoliticalEra::EnlightenmentRevolutionary,
            modifier(-20, -15, -5, 40)};
  case PoliticalTrait::AristocraticResistance:
    return {trait, PoliticalEra::EnlightenmentRevolutionary,
            modifier(-10, 30, 5, -25)};
  case PoliticalTrait::Secularization:
    return {trait, PoliticalEra::EnlightenmentRevolutionary,
            modifier(0, 0, -25, 10)};
  case PoliticalTrait::RevolutionarySentiment:
    return {trait, PoliticalEra::EnlightenmentRevolutionary,
            modifier(-20, -20, -10, 50), 0.05, 0.10};
  case PoliticalTrait::NationalAwakening:
    return {trait, PoliticalEra::EnlightenmentRevolutionary,
            modifier(-5, -5, -5, 20)};
  case PoliticalTrait::ReactionaryOrder:
    return {trait, PoliticalEra::EnlightenmentRevolutionary,
            modifier(15, 20, 10, -45)};
  case PoliticalTrait::ConstitutionalMonarchy:
    return {trait, PoliticalEra::Industrial, modifier(5, -10, -10, 25)};
  case PoliticalTrait::LiberalReform:
    return {trait, PoliticalEra::Industrial, modifier(-5, -15, -20, 35)};
  case PoliticalTrait::ParliamentaryTradition:
    return {trait, PoliticalEra::Industrial, modifier(-10, -10, -15, 35)};
  case PoliticalTrait::ConservativeEstablishment:
    return {trait, PoliticalEra::Industrial, modifier(10, 15, 5, -25)};
  case PoliticalTrait::Nationalism:
    return {trait, PoliticalEra::Industrial, modifier(10, 0, -10, 20), 0.20,
            0.15};
  case PoliticalTrait::SocialReform:
    return {trait, PoliticalEra::Industrial, modifier(-5, -10, -20, 35)};
  case PoliticalTrait::SocialistMovement:
    return {trait, PoliticalEra::Industrial, modifier(-30, -25, -25, 70), 0.05,
            0.50};
  case PoliticalTrait::RevolutionaryMovement:
    return {trait, PoliticalEra::Industrial, modifier(-30, -20, -25, 70), 0.05,
            0.25};
  case PoliticalTrait::AuthoritarianModernization:
    return {trait, PoliticalEra::Industrial, modifier(45, -5, -15, -5), 0.25,
            0.25};
  case PoliticalTrait::LiberalDemocracy:
    return {trait, PoliticalEra::Modern, modifier(-15, -20, -30, 60)};
  case PoliticalTrait::SocialDemocracy:
    return {trait, PoliticalEra::Modern, modifier(-15, -20, -30, 60), 0.0,
            0.40};
  case PoliticalTrait::CommunistMovement:
    return {trait, PoliticalEra::Modern, modifier(-75, -50, -50, 115), 0.15,
            0.75};
  case PoliticalTrait::FascistMovement:
    return {trait, PoliticalEra::Modern, modifier(40, -5, -50, 25), 0.30, 0.25};
  case PoliticalTrait::MilitaryAuthoritarianism:
    return {trait, PoliticalEra::Modern, modifier(40, 5, -30, -25), 0.45, 0.20};
  case PoliticalTrait::OnePartyState:
    return {trait, PoliticalEra::Modern, modifier(30, -20, -20, 15), 0.40,
            0.40};
  case PoliticalTrait::ReligiousConservatism:
    return {trait, PoliticalEra::Modern, modifier(5, 5, 30, -20)};
  case PoliticalTrait::PopulistMovement:
    return {trait, PoliticalEra::Modern, modifier(-20, -15, -20, 55), 0.25,
            0.25};
  case PoliticalTrait::TechnocraticGovernment:
    return {trait, PoliticalEra::Modern, modifier(25, -10, -20, 10), 0.30,
            0.35};
  }
  return {trait, PoliticalEra::AncientClassical, {0.0, 0.0, 0.0, 0.0}};
}

bool hasTrait(const Polity &polity, PoliticalTrait trait) {
  return std::any_of(
      polity.politicalTraits.begin(), polity.politicalTraits.end(),
      [trait](const auto &state) { return state.trait == trait; });
}

bool addRandomTrait(Polity &polity, PoliticalEra era, Year year,
                    double strength) {
  std::vector<PoliticalTrait> candidates;
  for (std::size_t index = 0; index < PoliticalTraitCount; ++index) {
    const auto trait = static_cast<PoliticalTrait>(index);
    if (traitDefinition(trait).era == era && !hasTrait(polity, trait))
      candidates.push_back(trait);
  }
  if (!candidates.empty()) {
    const auto selected = candidates[static_cast<std::size_t>(
        RandNum::getRandom<int>(static_cast<int>(candidates.size())))];
    polity.politicalTraits.push_back({selected, strength, year, std::nullopt});
    return true;
  }
  return false;
}

PoliticalPowerDistribution randomizedPoliticalPower() {
  PoliticalPowerDistribution power{};
  double total = 0.0;
  for (auto &value : power) {
    value = 0.5 + RandNum::getRandom<double>(1.0);
    total += value;
  }
  for (auto &value : power)
    value /= total;
  return power;
}

struct PolityDevelopmentTotal {
  std::size_t provinceCount = 0;
  double development = 0.0;
};

struct PoliticalDirectionSnapshot {
  std::array<double, 4> development{};
  double totalDevelopment = 0.0;
};

PoliticalDirectionSnapshot calculateDirectionSnapshot(
    const State &state, const std::vector<double> &developmentTotals,
    const std::vector<std::vector<ProvinceId>> &territories) {
  PoliticalDirectionSnapshot snapshot;
  for (PolityId polityId = 0;
       polityId < static_cast<PolityId>(state.polities.size()); ++polityId) {
    if (static_cast<std::size_t>(polityId) >= territories.size() ||
        territories[static_cast<std::size_t>(polityId)].empty())
      continue;
    const auto development = developmentTotals[static_cast<std::size_t>(polityId)];
    if (development <= 0.0)
      continue;
    const auto *polity = state::findPolity(state, polityId);
    if (!polity)
      continue;
    const auto direction = static_cast<std::size_t>(
        politicalDirection(polity->governmentForm));
    snapshot.development[direction] += development;
    snapshot.totalDevelopment += development;
  }
  return snapshot;
}

PoliticalPowerDistribution normalizedPower(PoliticalPowerDistribution power) {
  double total = 0.0;
  for (auto &value : power) {
    value = std::max(0.0, value);
    total += value;
  }
  if (total <= 0.0)
    return {0.25, 0.25, 0.25, 0.25};
  for (auto &value : power)
    value /= total;
  return power;
}

void addPoliticalPressure(Polity &polity, PoliticalPressureType type,
                          const PoliticalPowerDistribution &effect,
                          double strength, Year year, Year duration) {
  polity.politicalPressures.push_back(
      {type, effect, strength, year, year + std::max<Year>(0, duration)});
}

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
        3.0 * averageDevelopment *
            configuration.centralizationDevelopmentScale * maximum * affinity,
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

GovernmentForm
governmentFor(Polity &polity, Year year, const Configuration &configuration,
              bool hasCoastalProvince, double stability, double navyTradition,
              double armyTradition, double development, double warPressure,
              double centuries, const PoliticalDirectionSnapshot &directionSnapshot,
              GovernmentTransitionDiagnostics &diagnostics) {
  const auto &power = polity.politicalPower;
  const auto central =
      power[static_cast<std::size_t>(PoliticalBloc::CentralAuthority)];
  const auto nobility =
      power[static_cast<std::size_t>(PoliticalBloc::Nobility)];
  const auto religion =
      power[static_cast<std::size_t>(PoliticalBloc::ReligiousInstitutions)];
  const auto people = power[static_cast<std::size_t>(PoliticalBloc::People)];
  const auto instability =
      std::abs(std::clamp(stability, 0.0, 1.0) - configuration.stabilityMean);
  const auto era = eraFor(year, configuration);
  std::array<double, GovernmentFormCount> scores{};
  const auto setScore = [&scores](GovernmentForm form, double score) {
    scores[static_cast<std::size_t>(form)] = std::max(0.0, score);
  };
  if (year < configuration.classicalStartYear)
    return GovernmentForm::Tribal;

  setScore(GovernmentForm::AbsoluteMonarchy,
           central * (1.0 - nobility) * (1.0 - people));
  setScore(GovernmentForm::FeudalMonarchy, central + nobility * 1.25);
  setScore(GovernmentForm::AristocraticMonarchy,
           nobility * 1.35 + central * 0.7);
  setScore(GovernmentForm::ElectiveMonarchy, nobility * 1.4 + central * 0.35);
  setScore(GovernmentForm::TheocraticMonarchy, central * 0.8 + religion * 1.4);
  setScore(GovernmentForm::Theocracy, religion * 1.7);
  setScore(GovernmentForm::AristocraticRepublic,
           nobility * 1.5 + (1.0 - central) * 0.5);
  setScore(GovernmentForm::OligarchicRepublic,
           nobility * 1.3 + (1.0 - people) * 0.7);
  if (year >= configuration.modernStartYear) {
    setScore(GovernmentForm::ConstitutionalMonarchy,
             people * 1.0 + central * 1.0);
    setScore(GovernmentForm::ParliamentaryMonarchy,
             people * 1.25 + central * 0.75);
    setScore(GovernmentForm::ConstitutionalRepublic,
             people * 1.5 + central * 0.5);
    setScore(GovernmentForm::ParliamentaryRepublic,
             people * 1.8 + central * 0.2);
    if (people >= configuration.directDemocracyPeopleThreshold &&
        people >= nobility + configuration.directDemocracyLeadThreshold &&
        people >= religion + configuration.directDemocracyLeadThreshold)
      setScore(GovernmentForm::DirectDemocracy, people * 1.8);
  }
  if (year >= configuration.ideologicalTransitionStartYear) {
    setScore(GovernmentForm::MilitaryGovernment,
             (central * 0.75 + polity.militaryInfluence * armyTradition +
              std::max(0.0, warPressure) * 0.45));
    setScore(GovernmentForm::CommunistState,
             (polity.partyControl * 1.15 + people * 1.2 + central * 0.8 +
              instability * 0.45 + std::max(0.0, warPressure) * 0.2));
    setScore(GovernmentForm::FascistState,
             (polity.partyControl * 1.15 + people * 0.45 + central * 1.0 +
              instability * 0.2 + std::max(0.0, warPressure) * 0.25 +
              nobility * 0.15 + 0.5 * polity.militaryInfluence * armyTradition));
    for (std::size_t index =
             static_cast<std::size_t>(GovernmentForm::MilitaryGovernment);
         index < GovernmentFormCount; ++index)
      if (scores[index] > 0.0)
        scores[index] = std::max(
            scores[index], configuration.modernGovernmentMinimumPopularity);
  }
  if (hasCoastalProvince && navyTradition >= 0.2)
    setScore(GovernmentForm::OligarchicRepublic,
             people * 0.6 + nobility * 1.1 + navyTradition * 0.35);

  const auto targetMultiplier = [&configuration, year](GovernmentForm form) {
    const auto modern = year >= configuration.modernStartYear;
    const auto ideological =
        year >= configuration.ideologicalTransitionStartYear;
    switch (form) {
    case GovernmentForm::AbsoluteMonarchy:
    case GovernmentForm::FeudalMonarchy:
    case GovernmentForm::AristocraticMonarchy:
    case GovernmentForm::ElectiveMonarchy:
      return modern ? configuration.modernMonarchyTargetMultiplier
                    : configuration.preModernMonarchyTargetMultiplier;
    case GovernmentForm::TheocraticMonarchy:
      return (modern ? configuration.modernMonarchyTargetMultiplier
                     : configuration.preModernMonarchyTargetMultiplier) *
             (modern ? configuration.modernTheocracyTargetMultiplier
                     : configuration.preModernTheocracyTargetMultiplier);
    case GovernmentForm::Theocracy:
      return modern ? configuration.modernTheocracyTargetMultiplier
                    : configuration.preModernTheocracyTargetMultiplier;
    case GovernmentForm::ConstitutionalMonarchy:
    case GovernmentForm::ParliamentaryMonarchy:
    case GovernmentForm::ConstitutionalRepublic:
    case GovernmentForm::ParliamentaryRepublic:
    case GovernmentForm::DirectDemocracy:
      return modern ? configuration.modernDemocracyTargetMultiplier : 1.0;
    case GovernmentForm::CommunistState:
      return ideological ? configuration.modernCommunismTargetMultiplier : 1.0;
    case GovernmentForm::FascistState:
      return ideological ? configuration.modernFascismTargetMultiplier : 1.0;
    default:
      return 1.0;
    }
  };
  for (std::size_t index = 0; index < GovernmentFormCount; ++index)
    if (scores[index] > 0.0) {
      const auto form = static_cast<GovernmentForm>(index);
      scores[index] *= targetMultiplier(form);
      scores[index] +=
          governmentTraitAffinity(polity, form, era, configuration);
      const auto direction = politicalDirection(form);
      if ((direction == PoliticalDirection::Democracy ||
           direction == PoliticalDirection::Communism ||
           direction == PoliticalDirection::Fascism) &&
          directionSnapshot.totalDevelopment > 0.0) {
        const auto target =
            direction == PoliticalDirection::Democracy
                ? configuration.democracyDevelopmentTarget
                : direction == PoliticalDirection::Communism
                      ? configuration.communismDevelopmentTarget
                      : configuration.fascismDevelopmentTarget;
        const auto share =
            directionSnapshot.development[static_cast<std::size_t>(direction)] /
            directionSnapshot.totalDevelopment;
        const auto feedback = std::clamp(
            std::max(0.0, configuration.governmentDirectionFeedbackStrength) *
                (target - share),
            -1.0, 1.0);
        const auto multiplier = std::clamp(
            1.0 + feedback,
            configuration.governmentDirectionMinimumMultiplier,
            configuration.governmentDirectionMaximumMultiplier);
        scores[index] *= multiplier;
      }
    }

  const auto current = static_cast<std::size_t>(polity.governmentForm);
  const auto modernMultiplier =
      year >= configuration.modernStartYear
          ? std::max(1.0,
                     configuration.modernGovernmentPopularityDriftMultiplier)
          : 1.0;
  const auto drift = std::clamp(
      std::max(0.0, configuration.governmentPopularityDriftPerCentury) *
          std::max(centuries, 0.0) * modernMultiplier *
          (1.0 + 2.0 * instability),
      0.0, 1.0);
  for (std::size_t index = 0; index < GovernmentFormCount; ++index) {
    const auto target = scores[index] > 0.0 ? scores[index] : 0.0;
    polity.governmentPopularity[index] +=
        (target - polity.governmentPopularity[index]) * drift;
    polity.governmentPopularity[index] =
        std::max(0.0, polity.governmentPopularity[index]);
  }
  const auto variation =
      std::max(0.0, configuration.governmentPopularityVariation) *
      std::sqrt(std::max(centuries, 0.0));
  for (auto &value : polity.governmentPopularity)
    value = std::max(0.0, value + (RandNum::getRandom<double>(2.0) - 1.0) *
                                      variation);

  auto best = current;
  for (std::size_t index = 0; index < GovernmentFormCount; ++index) {
    if (polity.governmentPopularity[index] > polity.governmentPopularity[best])
      best = index;
  }
  if (best != current &&
      polity.governmentPopularity[best] <=
          polity.governmentPopularity[current] +
              std::max(0.0, configuration.governmentPopularityHysteresis))
    best = current;
  ++diagnostics.transitionChecks;
  if (current < diagnostics.checksBySource.size())
    ++diagnostics.checksBySource[current];
  if (current < diagnostics.candidatePoolsBySource.size())
    ++diagnostics.candidatePoolsBySource[current];
  if (best != current) {
    ++diagnostics.transitionSuccesses;
    ++diagnostics.selectedTransitions;
    ++diagnostics.selectionsByDestination[best];
    if (best < current)
      ++diagnostics.reversions;
  }
  return static_cast<GovernmentForm>(best);
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
    snapshot.politicalPower = polity->politicalPower;
    snapshot.desiredPoliticalPower = polity->desiredPoliticalPower;
    snapshot.politicalTraits = polity->politicalTraits;
    snapshot.politicalPressures = polity->politicalPressures;
    snapshot.governmentPopularity = polity->governmentPopularity;
    snapshot.militaryInfluence = polity->militaryInfluence;
    snapshot.desiredMilitaryInfluence = polity->desiredMilitaryInfluence;
    snapshot.partyControl = polity->partyControl;
    snapshot.desiredPartyControl = polity->desiredPartyControl;

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
  const auto directionSnapshot =
      calculateDirectionSnapshot(state, developmentTotals, territories);
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
    const auto eventMaximum =
        std::max(eventMinimum, configuration.stabilityRandomEventMaximumImpact);
    const auto randomEvent =
        chance(
            std::max(0.0, configuration.stabilityRandomEventChancePerCentury) *
            centuries)
            ? (eventMinimum + RandNum::getRandom<double>(1.0) *
                                  (eventMaximum - eventMinimum)) *
                  (RandNum::getRandom<double>(2.0) - 1.0)
            : 0.0;
    const auto randomVariation =
        (RandNum::getRandom<double>(2.0) - 1.0) *
        std::max(0.0, configuration.stabilityVolatilityPerCentury) *
        std::sqrt(std::max(centuries, 0.0));
    const auto stability = std::clamp(polity->stability + stabilityReversion +
                                          stabilityWarEffects[index] +
                                          randomEvent + randomVariation,
                                      0.0, 1.0);
    polity->politicalPressures.erase(
        std::remove_if(polity->politicalPressures.begin(),
                       polity->politicalPressures.end(),
                       [nextYear](const auto &pressure) {
                         return pressure.expiryYear &&
                                *pressure.expiryYear < nextYear;
                       }),
        polity->politicalPressures.end());
    if (stabilityWarEffects[index] > 0.0)
      addPoliticalPressure(*polity, PoliticalPressureType::WarVictory,
                           {0.25, 0.0, 0.0, -0.10}, 1.0, nextYear,
                           configuration.politicalPressureDurationYears);
    if (stabilityWarEffects[index] < 0.0)
      addPoliticalPressure(*polity, PoliticalPressureType::WarDefeat,
                           {-0.20, 0.20, 0.0, 0.15}, 1.0, nextYear,
                           configuration.politicalPressureDurationYears);
    if (developmentProvinceCounts[index] > 0)
      addPoliticalPressure(
          *polity, PoliticalPressureType::Development, {0.10, 0.0, 0.0, 0.10},
          std::clamp(developmentTotals[index] /
                         static_cast<double>(developmentProvinceCounts[index]),
                     0.0, 1.0),
          nextYear, configuration.politicalPressureDurationYears);
    if (chance(
            std::max(0.0, configuration.politicalGenericShockChancePerCentury) *
            centuries)) {
      const auto direction = RandNum::getRandom<double>(2.0) - 1.0;
      addPoliticalPressure(
          *polity, PoliticalPressureType::GenericShock,
          {direction, -direction * 0.5, direction * 0.25, -direction * 0.5},
          std::max(0.0, configuration.politicalGenericShockStrength), nextYear,
          configuration.politicalPressureDurationYears);
    }
    const auto era = eraFor(nextYear, configuration);
    if (era != polity->lastTraitEra) {
      if (chance(configuration.eraTraitChance))
        addRandomTrait(*polity, era, nextYear,
                       configuration.traitInitialStrength);
      polity->lastTraitEra = era;
    }
    if (stabilityWarEffects[index] < 0.0 &&
        chance(configuration.warLossTraitChancePerCentury * centuries))
      addRandomTrait(*polity, era, nextYear, configuration.traitShockStrength);
    if (chance(configuration.randomTraitShockChancePerCentury * centuries))
      addRandomTrait(*polity, era, nextYear, configuration.traitShockStrength);
    auto desiredMilitaryInfluence =
        polity->militaryInfluence * configuration.militaryInfluencePersistence;
    auto desiredPartyControl =
        polity->partyControl * configuration.partyControlPersistence;
    if (stabilityWarEffects[index] > 0.0)
      desiredMilitaryInfluence +=
          configuration.militaryInfluenceWarVictoryPressure;
    if (stabilityWarEffects[index] < 0.0)
      desiredMilitaryInfluence +=
          configuration.militaryInfluenceWarDefeatPressure;
    if (nextYear >= configuration.ideologicalTransitionStartYear)
      desiredPartyControl += configuration.partyControlModernPressure;
    for (const auto &pressure : polity->politicalPressures) {
      desiredMilitaryInfluence += pressure.effect[static_cast<std::size_t>(
                                      PoliticalBloc::CentralAuthority)] *
                                  pressure.strength * 0.25;
      desiredPartyControl +=
          pressure.effect[static_cast<std::size_t>(PoliticalBloc::People)] *
          pressure.strength * 0.15;
    }
    if (chance(
            std::max(0.0, configuration.politicalGenericShockChancePerCentury) *
            centuries)) {
      desiredMilitaryInfluence += (RandNum::getRandom<double>(2.0) - 1.0) *
                                  configuration.politicalGenericShockStrength;
      desiredPartyControl += (RandNum::getRandom<double>(2.0) - 1.0) *
                             configuration.politicalGenericShockStrength;
    }
    polity->desiredMilitaryInfluence =
        std::clamp(desiredMilitaryInfluence, 0.0, 1.0);
    polity->desiredPartyControl = std::clamp(desiredPartyControl, 0.0, 1.0);
    const auto militaryConvergence = std::clamp(
        1.0 -
            std::exp(
                -std::max(
                    0.0, configuration.militaryInfluenceConvergencePerCentury) *
                std::max(centuries, 0.0)),
        0.0, 1.0);
    const auto partyConvergence = std::clamp(
        1.0 - std::exp(
                  -std::max(0.0,
                            configuration.partyControlConvergencePerCentury) *
                  std::max(centuries, 0.0) *
                  (nextYear >= configuration.ideologicalTransitionStartYear
                       ? std::max(0.0,
                                  configuration
                                      .modernPartyControlConvergenceMultiplier)
                       : 1.0)),
        0.0, 1.0);
    polity->militaryInfluence = std::clamp(
        polity->militaryInfluence +
            (polity->desiredMilitaryInfluence - polity->militaryInfluence) *
                militaryConvergence +
            (RandNum::getRandom<double>(2.0) - 1.0) *
                configuration.militaryInfluenceVariation *
                std::sqrt(std::max(centuries, 0.0)),
        0.0, 1.0);
    polity->partyControl =
        std::clamp(polity->partyControl +
                       (polity->desiredPartyControl - polity->partyControl) *
                           partyConvergence +
                       (RandNum::getRandom<double>(2.0) - 1.0) *
                           configuration.partyControlVariation *
                           std::sqrt(std::max(centuries, 0.0)),
                   0.0, 1.0);
    PoliticalPowerDistribution desired{0.25, 0.25, 0.25, 0.25};
    for (const auto &traitState : polity->politicalTraits) {
      const auto definition = traitDefinition(traitState.trait);
      const auto ageMultiplier =
          traitAgeMultiplier(definition.era, era, configuration);
      for (std::size_t bloc = 0; bloc < PoliticalBlocCount; ++bloc)
        desired[bloc] += definition.powerModifier[bloc] * traitState.strength *
                         ageMultiplier;
      desiredMilitaryInfluence += definition.militaryInfluenceModifier *
                                  traitState.strength * ageMultiplier;
      desiredPartyControl +=
          definition.partyControlModifier * traitState.strength * ageMultiplier;
    }
    for (const auto &pressure : polity->politicalPressures)
      for (std::size_t bloc = 0; bloc < PoliticalBlocCount; ++bloc)
        desired[bloc] += pressure.effect[bloc] * pressure.strength;
    for (std::size_t bloc = 0; bloc < PoliticalBlocCount; ++bloc)
      desired[bloc] += polity->desiredPoliticalPower[bloc] *
                       configuration.politicalPowerPersistence[bloc];
    polity->desiredPoliticalPower = normalizedPower(desired);
    const auto powerConvergence = std::clamp(
        1.0 - std::exp(
                  -std::max(0.0,
                            configuration.politicalPowerConvergencePerCentury) *
                  std::max(centuries, 0.0) *
                  (1.0 + std::abs(stability - configuration.stabilityMean))),
        0.0, 1.0);
    for (std::size_t bloc = 0; bloc < PoliticalBlocCount; ++bloc)
      polity->politicalPower[bloc] +=
          (polity->desiredPoliticalPower[bloc] - polity->politicalPower[bloc]) *
          powerConvergence;
    for (auto &value : polity->politicalPower)
      value += (RandNum::getRandom<double>(2.0) - 1.0) *
               std::max(0.0, configuration.politicalPowerVariation) *
               std::sqrt(std::max(centuries, 0.0));
    polity->politicalPower = normalizedPower(polity->politicalPower);
    Event update{nextYear, EventType::PolityUpdate};
    update.polityId = polityId;
    update.centralizationAffinity = polity->centralizationAffinity;
    update.governmentForm = governmentFor(
        *polity, nextYear, configuration, coastal, stability, navyTradition,
        armyTradition,
        developmentProvinceCounts[index] == 0
            ? 0.0
            : developmentTotals[index] /
                  static_cast<double>(developmentProvinceCounts[index]),
        std::clamp(warPressure[index], 0.0, 1.0), centuries,
        directionSnapshot, diagnostics);
    update.lastGovernmentTransitionYear = polity->lastGovernmentTransitionYear;
    update.lastTraitEra = polity->lastTraitEra;
    update.politicalPower = polity->politicalPower;
    update.desiredPoliticalPower = polity->desiredPoliticalPower;
    update.politicalTraits = polity->politicalTraits;
    update.politicalPressures = polity->politicalPressures;
    update.governmentPopularity = polity->governmentPopularity;
    update.militaryInfluence = polity->militaryInfluence;
    update.desiredMilitaryInfluence = polity->desiredMilitaryInfluence;
    update.partyControl = polity->partyControl;
    update.desiredPartyControl = polity->desiredPartyControl;
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
  for (PolityId polityId = 0;
       polityId < static_cast<PolityId>(territories.size()); ++polityId) {
    const auto &provinceIds = territories[static_cast<std::size_t>(polityId)];
    if (provinceIds.empty())
      continue;
    if (provinceIds.size() < 2 || !state::findPolity(state, polityId) ||
        !state.polityStrengths.contains(polityId))
      continue;
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
    const auto *polity = state::findPolity(state, polityId);
    const auto stabilityPressure =
        configuration.stabilityMean <= 0.0
            ? 0.0
            : std::clamp((configuration.stabilityMean - polity->stability) /
                             configuration.stabilityMean,
                         0.0, 1.0);
    const auto capacity =
        development::polityCapacityAt(configuration, nextYear);
    const auto overCapacityPressure =
        capacity <= 0.0
            ? 0.0
            : std::clamp((static_cast<double>(provinceIds.size()) - capacity) /
                             capacity,
                         0.0, 1.0);
    const auto decayPressure =
        1.0 + 2.0 * stabilityPressure + 6.0 * overCapacityPressure;
    const bool decaySplit =
        (stabilityPressure > 0.0 || overCapacityPressure > 0.0) &&
        chance(configuration.decayFragmentationChance * centuries *
               decayPressure * stabilizationMultiplier * maturationMultiplier *
               smallPolityMultiplier);
    bool ordinarySplit = false;
    /*chance(
        configuration.splitOffChance * fragmentationMultiplier * centuries *
        std::max(0.25, 1.0 - consolidationPressure) * stabilizationMultiplier *
        maturationMultiplier * smallPolityMultiplier);*/
    if (decaySplit) {
      const auto desiredSuccessorCount = provinceIds.size() >= 8 ? 4u : 3u;
      const auto successorCount =
          std::min(provinceIds.size(),
                   std::min(static_cast<size_t>(
                                configuration.maximumImplosionSuccessors),
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
          successor.lastTraitEra = parent->lastTraitEra;
          successor.politicalPower = parent->politicalPower;
          successor.desiredPoliticalPower = parent->desiredPoliticalPower;
          successor.politicalTraits = parent->politicalTraits;
          successor.politicalPressures = parent->politicalPressures;
          successor.governmentPopularity = parent->governmentPopularity;
          successor.militaryInfluence = parent->militaryInfluence;
          successor.desiredMilitaryInfluence = parent->desiredMilitaryInfluence;
          successor.partyControl = parent->partyControl;
          successor.desiredPartyControl = parent->desiredPartyControl;
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
    } else if (ordinarySplit) {
      const auto splitPolity = nextPolity++;
      Event split{nextYear, EventType::CreatePolity};
      split.polityId = splitPolity;
      split.description = "tribe";
      split.colour = randomColour();
      split.centralizationAffinity =
          randomCentralizationAffinity(configuration);
      split.politicalPower = randomizedPoliticalPower();
      split.desiredPoliticalPower = split.politicalPower;
      split.governmentPopularity.fill(0.0);
      split.governmentPopularity[static_cast<std::size_t>(
          GovernmentForm::Tribal)] = 1.0;
      split.militaryInfluence = RandNum::getRandom<double>(0.25);
      split.desiredMilitaryInfluence = split.militaryInfluence;
      split.partyControl = RandNum::getRandom<double>(0.15);
      split.desiredPartyControl = split.partyControl;
      if (const auto *parent =
              ::Arda::Simulation::state::findPolity(state, polityId)) {
        split.politicalTraits = parent->politicalTraits;
        split.lastTraitEra = parent->lastTraitEra;
      }
      append(std::move(split));
      const auto splitProvince = provinceIds.back();
      append({nextYear, EventType::TransferProvince, splitProvince,
              normalized.provinceRegions.at(splitProvince), splitPolity,
              polityId, -1, NoReligion, 0.0, 0.0, "fragmentation"});
    }
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
