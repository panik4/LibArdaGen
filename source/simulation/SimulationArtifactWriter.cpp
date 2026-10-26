#include "simulation/SimulationArtifactWriter.h"

#include "rendering/Png.h"
#include "simulation/SimulationArtifacts.h"
#include "simulation/SimulationGeography.h"
#include "simulation/SimulationState.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <map>
#include <sstream>

namespace Arda::Simulation::artifacts {
namespace {

using geography::normalize;

const char *governmentFormName(GovernmentForm form) {
  switch (form) {
  case GovernmentForm::Tribal:
    return "tribal";
  case GovernmentForm::AbsoluteMonarchy:
    return "absolute_monarchy";
  case GovernmentForm::FeudalMonarchy:
    return "feudal_monarchy";
  case GovernmentForm::ConstitutionalMonarchy:
    return "constitutional_monarchy";
  case GovernmentForm::ParliamentaryMonarchy:
    return "parliamentary_monarchy";
  case GovernmentForm::AristocraticMonarchy:
    return "aristocratic_monarchy";
  case GovernmentForm::ElectiveMonarchy:
    return "elective_monarchy";
  case GovernmentForm::TheocraticMonarchy:
    return "theocratic_monarchy";
  case GovernmentForm::Theocracy:
    return "theocracy";
  case GovernmentForm::AristocraticRepublic:
    return "aristocratic_republic";
  case GovernmentForm::OligarchicRepublic:
    return "oligarchic_republic";
  case GovernmentForm::ConstitutionalRepublic:
    return "constitutional_republic";
  case GovernmentForm::ParliamentaryRepublic:
    return "parliamentary_republic";
  case GovernmentForm::DirectDemocracy:
    return "direct_democracy";
  case GovernmentForm::MilitaryGovernment:
    return "military_government";
  case GovernmentForm::CommunistState:
    return "communist_state";
  case GovernmentForm::FascistState:
    return "fascist_state";
  }
  return "unknown";
}

bool validateInputs(const Configuration &configuration, const Input &input,
                    const Fwg::Gfx::Image &baseMap,
                    std::vector<ValidationError> &errors,
                    detail::NormalizedInput &normalized) {
  if (!baseMap.initialised()) {
    errors.push_back(
        {configuration.startYear,
         "Cannot export ownership maps without an initialized world map",
         {},
         {}});
    return false;
  }

  normalized = normalize(input, false);
  if (!normalized.errors.empty()) {
    errors.insert(errors.end(), normalized.errors.begin(),
                  normalized.errors.end());
    return false;
  }
  return true;
}

void createOutputDirectories(const std::filesystem::path &outputDirectory) {
  std::filesystem::create_directories(outputDirectory);
  std::filesystem::create_directories(outputDirectory / "development");
  std::filesystem::create_directories(outputDirectory / "population");
  std::filesystem::create_directories(outputDirectory / "culture");
  std::filesystem::create_directories(outputDirectory / "religion");
}

ArtifactPaths buildArtifactPaths(const std::filesystem::path &outputDirectory) {
  ArtifactPaths paths;
  paths.eventLog = outputDirectory / "events.txt";
  paths.developmentLog = outputDirectory / "development" / "development.tsv";
  paths.populationLog = outputDirectory / "population" / "population.tsv";
  paths.cultureLog = outputDirectory / "culture" / "culture.tsv";
  paths.religionLog = outputDirectory / "religion" / "religion.tsv";
  paths.superRegionLog = outputDirectory / "superregions.tsv";
  paths.polityStateLog = outputDirectory / "polity_state.csv";
  return paths;
}

bool writePolityStateCsvFile(const Result &result,
                             const std::filesystem::path &path, Year errorYear,
                             std::vector<ValidationError> &errors) {
  std::ofstream output(path);
  if (!output) {
    errors.push_back({errorYear, "Unable to open polity state CSV", {}, {}});
    return false;
  }

  constexpr const char *formNames[] = {"tribal",
                                       "absolute_monarchy",
                                       "feudal_monarchy",
                                       "constitutional_monarchy",
                                       "parliamentary_monarchy",
                                       "aristocratic_monarchy",
                                       "elective_monarchy",
                                       "theocratic_monarchy",
                                       "theocracy",
                                       "aristocratic_republic",
                                       "oligarchic_republic",
                                       "constitutional_republic",
                                       "parliamentary_republic",
                                       "direct_democracy",
                                       "military_government",
                                       "communist_state",
                                       "fascist_state"};

  output << "year,polity_count,average_centralization,average_army_tradition,"
            "average_navy_tradition,average_stability,"
            "average_population,average_development,average_population_density,"
            "current_central_authority,current_nobility,current_religious_"
            "institutions,current_people,"
            "desired_central_authority,desired_nobility,desired_religious_"
            "institutions,desired_people,"
            "average_military_influence,average_desired_military_influence,"
            "average_party_control,average_desired_party_control,"
            "average_trait_count,average_trait_strength,";
  for (std::size_t index = 0; index < std::size(formNames); ++index) {
    if (index != 0)
      output << ',';
    output << formNames[index] << "_count";
  }
  for (std::size_t index = 0; index < std::size(formNames); ++index)
    output << ',' << "average_" << formNames[index] << "_popularity";
  output << '\n';

  struct Aggregate {
    std::size_t polityCount = 0;
    double centralization = 0.0;
    double armyTradition = 0.0;
    double navyTradition = 0.0;
    double stability = 0.0;
    double population = 0.0;
    double development = 0.0;
    double populationDensity = 0.0;
    PoliticalPowerDistribution politicalPower{};
    PoliticalPowerDistribution desiredPoliticalPower{};
    double militaryInfluence = 0.0;
    double desiredMilitaryInfluence = 0.0;
    double partyControl = 0.0;
    double desiredPartyControl = 0.0;
    double traitCount = 0.0;
    double traitStrength = 0.0;
    std::array<std::size_t, GovernmentFormCount> governmentCounts{};
    std::array<double, GovernmentFormCount> governmentPopularity{};
  };
  std::map<Year, Aggregate> aggregates;
  for (const auto &[polityId, snapshots] : result.polityTimeline) {
    (void)polityId;
    for (const auto &snapshot : snapshots) {
      auto &aggregate = aggregates[snapshot.year];
      ++aggregate.polityCount;
      aggregate.centralization += snapshot.centralization;
      aggregate.armyTradition += snapshot.armyTradition;
      aggregate.navyTradition += snapshot.navyTradition;
      aggregate.stability += snapshot.stability;
      aggregate.population += snapshot.population;
      aggregate.development += snapshot.development;
      aggregate.populationDensity += snapshot.populationDensity;
      for (std::size_t bloc = 0; bloc < PoliticalBlocCount; ++bloc) {
        aggregate.politicalPower[bloc] += snapshot.politicalPower[bloc];
        aggregate.desiredPoliticalPower[bloc] +=
            snapshot.desiredPoliticalPower[bloc];
      }
      aggregate.militaryInfluence += snapshot.militaryInfluence;
      aggregate.desiredMilitaryInfluence += snapshot.desiredMilitaryInfluence;
      aggregate.partyControl += snapshot.partyControl;
      aggregate.desiredPartyControl += snapshot.desiredPartyControl;
      aggregate.traitCount +=
          static_cast<double>(snapshot.politicalTraits.size());
      for (const auto &trait : snapshot.politicalTraits)
        aggregate.traitStrength += trait.strength;
      const auto government = static_cast<std::size_t>(snapshot.governmentForm);
      if (government < aggregate.governmentCounts.size())
        ++aggregate.governmentCounts[government];
      for (std::size_t index = 0; index < GovernmentFormCount; ++index)
        aggregate.governmentPopularity[index] +=
            snapshot.governmentPopularity[index];
    }
  }

  for (const auto &[year, aggregate] : aggregates) {
    const auto count = static_cast<double>(aggregate.polityCount);
    output << year << ',' << aggregate.polityCount << ','
           << aggregate.centralization / count << ','
           << aggregate.armyTradition / count << ','
           << aggregate.navyTradition / count << ','
           << aggregate.stability / count << ',' << aggregate.population / count
           << ',' << aggregate.development / count << ','
           << aggregate.populationDensity / count;
    for (const auto value : aggregate.politicalPower)
      output << ',' << value / count;
    for (const auto value : aggregate.desiredPoliticalPower)
      output << ',' << value / count;
    output << ',' << aggregate.militaryInfluence / count << ','
           << aggregate.desiredMilitaryInfluence / count << ','
           << aggregate.partyControl / count << ','
           << aggregate.desiredPartyControl / count << ','
           << aggregate.traitCount / count << ','
           << aggregate.traitStrength / count;
    for (const auto governmentCount : aggregate.governmentCounts)
      output << ',' << governmentCount;
    for (const auto popularity : aggregate.governmentPopularity)
      output << ',' << popularity / count;
    output << '\n';
  }
  const auto &diagnostics = result.governmentTransitions;
  output << '\n'
         << "government_transition_diagnostics\n"
         << "metric,value\n"
         << "transition_checks," << diagnostics.transitionChecks << '\n'
         << "transition_successes," << diagnostics.transitionSuccesses << '\n'
         << "empty_candidate_pools," << diagnostics.emptyCandidatePools << '\n'
         << "selected_transitions," << diagnostics.selectedTransitions << '\n'
         << "reversions," << diagnostics.reversions << '\n'
         << "source_government,transition_checks,candidate_pools\n";
  for (std::size_t index = 0; index < GovernmentFormCount; ++index)
    output << formNames[index] << ',' << diagnostics.checksBySource[index]
           << ',' << diagnostics.candidatePoolsBySource[index] << '\n';
  output << '\n' << "destination_government,selected_transitions\n";
  for (std::size_t index = 0; index < GovernmentFormCount; ++index)
    output << formNames[index] << ','
           << diagnostics.selectionsByDestination[index] << '\n';
  return true;
}

bool writeEventLogFile(const ArtifactPaths &paths, const Result &result,
                       Year errorYear, std::vector<ValidationError> &errors) {
  std::ofstream eventLog(paths.eventLog);
  if (!eventLog) {
    errors.push_back(
        {errorYear, "Unable to open simulation event log", {}, {}});
    return false;
  }
  eventLog << "year\ttype\tprovince\tregion\tpolity\tprevious_"
              "polity\tculture\treligion\tparent\tvalue\tsecondary_"
              "value\tscore\tred\tgreen\tblue\tdescription\n";
  for (const auto &event : result.events)
    eventLog << event.year << '\t' << eventTypeName(event.type) << '\t'
             << event.provinceId << '\t' << event.regionId << '\t'
             << event.polityId << '\t' << event.previousPolityId << '\t'
             << event.cultureId << '\t' << event.religionId << '\t'
             << event.parentId << '\t' << event.value << '\t'
             << event.secondaryValue << '\t' << event.score << '\t'
             << static_cast<int>(event.colour.getRed()) << '\t'
             << static_cast<int>(event.colour.getGreen()) << '\t'
             << static_cast<int>(event.colour.getBlue()) << '\t'
             << event.description << '\n';
  return true;
}

bool openCategoryLogs(const ArtifactPaths &paths, Year errorYear,
                      std::vector<ValidationError> &errors,
                      std::ofstream &developmentLog,
                      std::ofstream &populationLog, std::ofstream &cultureLog,
                      std::ofstream &religionLog,
                      std::ofstream &superRegionLog) {
  developmentLog.open(paths.developmentLog);
  populationLog.open(paths.populationLog);
  cultureLog.open(paths.cultureLog);
  religionLog.open(paths.religionLog);
  superRegionLog.open(paths.superRegionLog);
  if (!developmentLog || !populationLog || !cultureLog || !religionLog ||
      !superRegionLog) {
    errors.push_back({errorYear,
                      "Unable to open one or more category simulation logs",
                      {},
                      {}});
    return false;
  }
  developmentLog << "year\tprovince\tdevelopment\n";
  populationLog
      << "year\tprovince\tpopulation\tcarrying_capacity\tregion_phase\n";
  cultureLog << "year\tprovince\tculture\tculture_population\tdominant\t"
                "origin_province\tfounded_year\tparent_culture\n";
  religionLog << "year\tprovince\treligion\torigin_province\tfounded_year\t"
                 "parent_religion\n";
  superRegionLog << "year\tsuperregion\tcontinent\tphase\tdevelopment\n";
  return true;
}

std::vector<Year> artifactYears(const Configuration &configuration) {
  std::vector<Year> years;
  for (Year year = configuration.startYear; year <= configuration.targetYear;
       year += 100)
    years.push_back(year);
  if (years.empty() || years.back() != configuration.targetYear)
    years.push_back(configuration.targetYear);
  return years;
}

void writeTimestepMaps(const State &state,
                       const detail::NormalizedInput &normalized,
                       const Fwg::Gfx::Image &baseMap,
                       const std::filesystem::path &outputDirectory,
                       const std::filesystem::path &developmentDirectory,
                       const std::filesystem::path &populationDirectory,
                       ArtifactPaths &paths, size_t index) {
  auto frame = baseMap;
  auto developmentFrame = baseMap;
  auto populationFrame = baseMap;
  auto cultureFrame = baseMap;
  auto religionFrame = baseMap;
  double maximumDevelopment = 0.0;
  double maximumPopulation = 0.0;
  for (const auto &provinceState : state.provinces) {
    if (!provinceState.initialized)
      continue;
    maximumDevelopment =
        std::max(maximumDevelopment, provinceState.development);
    maximumPopulation = std::max(maximumPopulation, provinceState.population);
  }
  for (const auto &[provinceId, province] : normalized.provinces) {
    const auto *provinceState = state::findProvince(state, provinceId);
    if (!provinceState)
      continue;
    const auto *owner = state::findPolity(state, provinceState->owner);
    if (!owner)
      continue;
    const auto development =
        maximumDevelopment > 0.0
            ? static_cast<unsigned char>(255.0 * provinceState->development /
                                         maximumDevelopment)
            : static_cast<unsigned char>(0);
    const auto population =
        maximumPopulation > 0.0
            ? static_cast<unsigned char>(255.0 *
                                         std::log1p(provinceState->population) /
                                         std::log1p(maximumPopulation))
            : static_cast<unsigned char>(0);
    const auto cultureColour = state.cultures.at(provinceState->culture).colour;
    const auto religionColour =
        provinceState->religion == NoReligion
            ? Fwg::Gfx::Colour{0, 0, 0}
            : state.religions.at(provinceState->religion).colour;
    for (const auto pixel : province->pixels) {
      if (pixel >= 0 && static_cast<std::size_t>(pixel) < frame.size())
        frame.setColourAtIndex(pixel, owner->colour);
      developmentFrame.setColourAtIndex(
          pixel, {development, development, development});
      populationFrame.setColourAtIndex(pixel,
                                       {population, population, population});
      cultureFrame.setColourAtIndex(pixel, cultureColour);
      religionFrame.setColourAtIndex(pixel, religionColour);
    }
  }
  std::ostringstream filename;
  filename << "ownership_" << std::setw(3) << std::setfill('0') << index << '_'
           << state.year << ".png";
  const auto mapPath = outputDirectory / filename.str();
  Fwg::Gfx::Png::save(frame, mapPath.string(), true);
  paths.ownershipMaps.push_back(mapPath);
  const auto developmentPath = developmentDirectory / filename.str();
  const auto populationPath = populationDirectory / filename.str();
  // Fwg::Gfx::Png::save(developmentFrame, developmentPath.string(), true);
  // Fwg::Gfx::Png::save(populationFrame, populationPath.string(), true);
  paths.developmentMaps.push_back(developmentPath);
  paths.populationMaps.push_back(populationPath);
  paths.cultureMaps.push_back(outputDirectory / "culture" / filename.str());
  paths.religionMaps.push_back(outputDirectory / "religion" / filename.str());
}

} // namespace

bool writePolityStateCsv(const Result &result,
                         const std::filesystem::path &path, Year errorYear,
                         std::vector<ValidationError> &errors) {
  return writePolityStateCsvFile(result, path, errorYear, errors);
}

bool writePolityStatisticsCsv(const SimulationExport &simulationExport,
                              const std::filesystem::path &path, Year errorYear,
                              std::vector<ValidationError> &errors) {
  std::ofstream output(path);
  if (!output) {
    errors.push_back(
        {errorYear, "Unable to open polity statistics CSV", {}, {}});
    return false;
  }

  output << "polity_id,core_count,non_core_count,claim_count,formable_count,"
            "formable_counts_by_required_province_count\n";
  for (const auto &[polityId, polityExport] : simulationExport.polities) {
    const auto &statistics = polityExport.statistics;
    output << polityId << ',' << statistics.coreProvinceCount << ','
           << statistics.nonCoreProvinceCount << ','
           << statistics.claimProvinceCount << ',' << statistics.formableCount
           << ',';
    bool first = true;
    for (const auto &[size, count] :
         statistics.formableCountsByRequiredProvinceCount) {
      if (!first)
        output << ';';
      output << size << ':' << count;
      first = false;
    }
    output << '\n';
  }
  output << '\n' <<
            "government_form,polity_count,population,development,score\n";
  for (std::size_t index = 0; index < GovernmentFormCount; ++index) {
    const auto form = static_cast<GovernmentForm>(index);
    const auto strength = simulationExport.governmentFormStrength.find(form);
    const auto &totals =
        strength == simulationExport.governmentFormStrength.end()
            ? SimulationGovernmentFormStrength{}
            : strength->second;
    output << governmentFormName(form) << ',' << totals.polityCount << ','
           << totals.population << ',' << totals.development << ','
           << totals.score << '\n';
  }
  output << '\n' << "political_direction,development,share\n";
  for (const auto direction : {PoliticalDirection::Other,
                               PoliticalDirection::Democracy,
                               PoliticalDirection::Communism,
                               PoliticalDirection::Fascism}) {
    const auto strength = simulationExport.politicalDirectionStrength.find(direction);
    const auto &totals = strength == simulationExport.politicalDirectionStrength.end()
                             ? SimulationPoliticalDirectionStrength{}
                             : strength->second;
    output << static_cast<int>(direction) << ',' << totals.development << ','
           << totals.share << '\n';
  }
  return true;
}

std::optional<ArtifactPaths>
writeSimulationArtifacts(const Configuration &configuration, const Input &input,
                         const Result &result, const Fwg::Gfx::Image &baseMap,
                         const std::filesystem::path &outputDirectory,
                         const StateReconstructor &reconstruct,
                         std::vector<ValidationError> &errors) {
  detail::NormalizedInput normalized;
  if (!validateInputs(configuration, input, baseMap, errors, normalized))
    return std::nullopt;
  try {
    createOutputDirectories(outputDirectory);
    auto paths = buildArtifactPaths(outputDirectory);
    if (!writePolityStateCsv(result, paths.polityStateLog,
                             configuration.startYear, errors))
      return std::nullopt;
    // if (!writeEventLogFile(paths, result, configuration.startYear, errors))
    //   return std::nullopt;
    // std::ofstream developmentLog;
    // std::ofstream populationLog;
    // std::ofstream cultureLog;
    // std::ofstream religionLog;
    // std::ofstream superRegionLog;
    // if (!openCategoryLogs(paths, configuration.startYear, errors,
    //					  developmentLog, populationLog,
    // cultureLog, 					  religionLog,
    // superRegionLog))
    //   return std::nullopt;
    const auto years = artifactYears(configuration);
    for (size_t index = 0; index < years.size(); ++index) {
      // const auto state = reconstruct(result.events, years[index]);
      // writeTimestepMaps(state, normalized, baseMap, outputDirectory,
      //                   outputDirectory / "development",
      //                   outputDirectory / "population", paths, index);
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

} // namespace Arda::Simulation::artifacts
