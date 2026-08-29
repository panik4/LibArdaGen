#include "simulation/SimulationExporter.h"

#include "simulation/SimulationState.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <tuple>
#include <utility>

namespace Arda::Simulation {
namespace {

struct ProvinceExportMetrics {
  double maximumPopulationDensity = 0.0;
  double maximumDevelopment = 0.0;
};

ProvinceExportMetrics calculateProvinceExportMetrics(
    const State &state, const std::map<ProvinceId, size_t> &provinceAreas) {
  ProvinceExportMetrics metrics;
  for (ProvinceId provinceId = 0;
       provinceId < static_cast<ProvinceId>(state.provinces.size());
       ++provinceId) {
    const auto *province = state::findProvince(state, provinceId);
    if (!province)
      continue;
    const auto area = provinceAreas.find(provinceId);
    const auto populationDensity =
        area != provinceAreas.end() && area->second > 0
            ? province->population / static_cast<double>(area->second)
            : 0.0;
    metrics.maximumPopulationDensity =
        std::max(metrics.maximumPopulationDensity, populationDensity);
    metrics.maximumDevelopment =
        std::max(metrics.maximumDevelopment, province->development);
  }
  return metrics;
}

void exportFormables(SimulationExport &exported, const State &state,
                     const std::map<ProvinceId, RegionId> &provinceRegions,
                     const SimulationPolityHistory &polityHistory) {
  std::size_t largestPeakSize = 0;
  for (const auto &[polityId, peakProvinceIds] : polityHistory.peakProvinceIds)
    largestPeakSize = std::max(largestPeakSize, peakProvinceIds.size());
  const auto minimumPeakSize =
      static_cast<std::size_t>(std::ceil(largestPeakSize * 0.05));
  for (const auto &[historicalPolityId, peakProvinceIds] :
       polityHistory.peakProvinceIds) {
    if (peakProvinceIds.empty() || peakProvinceIds.size() < minimumPeakSize)
      continue;
    if (!exported.polities.contains(historicalPolityId))
      continue;
    const auto historicalPolity = state::findPolity(state, historicalPolityId);
    if (!historicalPolity)
      continue;
    const auto firstProvince =
        state::findProvince(state, peakProvinceIds.front());
    if (!firstProvince || firstProvince->owner == NoPolity)
      continue;
    const auto currentOwner = firstProvince->owner;
    if (!std::all_of(peakProvinceIds.begin(), peakProvinceIds.end(),
                     [&state, currentOwner](ProvinceId provinceId) {
                       const auto *province =
                           state::findProvince(state, provinceId);
                       return province && province->owner == currentOwner;
                     }))
      continue;

    SimulationFormableExport formable;
    formable.historicalPolityId = historicalPolityId;
    formable.requiredProvinceIds = peakProvinceIds;
    formable.currentOwnerId = currentOwner;
    for (const auto provinceId : peakProvinceIds) {
      const auto region = provinceRegions.find(provinceId);
      if (region != provinceRegions.end() && region->second >= 0 &&
          std::find(formable.requiredRegionIds.begin(),
                    formable.requiredRegionIds.end(),
                    region->second) == formable.requiredRegionIds.end())
        formable.requiredRegionIds.push_back(region->second);
    }
    exported.formables.emplace(historicalPolityId, std::move(formable));
  }
}

void appendRegion(SimulationTerritorySetExport &territory, RegionId regionId) {
  if (regionId >= 0 &&
      std::find(territory.regionIds.begin(), territory.regionIds.end(),
                regionId) == territory.regionIds.end())
    territory.regionIds.push_back(regionId);
}

struct OwnershipEpisode {
  PolityId polityId = NoPolity;
  Year startYear = StartYear;
  Year endYear = StartYear;
};

using OwnershipEpisodes = std::map<ProvinceId, std::vector<OwnershipEpisode>>;

OwnershipEpisodes buildOwnershipEpisodes(const State &state,
                                         const std::vector<Event> &events) {
  OwnershipEpisodes episodes;
  std::map<ProvinceId, std::pair<PolityId, Year>> current;
  for (const auto &event : events) {
    const bool initializes = event.type == EventType::InitializeProvince;
    const bool transfers = event.type == EventType::TransferProvince ||
                           event.type == EventType::ConsolidateRegion ||
                           event.type == EventType::ColonizeProvince;
    if (!initializes && !transfers)
      continue;
    if (initializes) {
      current[event.provinceId] = {event.polityId, event.year};
      continue;
    }
    const auto currentOwner = current.find(event.provinceId);
    const auto previousOwner = currentOwner == current.end()
                                   ? event.previousPolityId
                                   : currentOwner->second.first;
    const auto startYear = currentOwner == current.end()
                               ? event.year
                               : currentOwner->second.second;
    if (previousOwner != NoPolity && previousOwner != event.polityId)
      episodes[event.provinceId].push_back(
          {previousOwner, startYear, event.year});
    current[event.provinceId] = {event.polityId, event.year};
  }
  for (const auto &[provinceId, ownership] : current)
    if (ownership.first != NoPolity)
      episodes[provinceId].push_back(
          {ownership.first, ownership.second, state.year});
  return episodes;
}

void appendRegion(const std::map<ProvinceId, RegionId> &provinceRegions,
                  SimulationTerritorySetExport &territory,
                  ProvinceId provinceId) {
  const auto region = provinceRegions.find(provinceId);
  if (region != provinceRegions.end() && region->second >= 0 &&
      std::find(territory.regionIds.begin(), territory.regionIds.end(),
                region->second) == territory.regionIds.end())
    territory.regionIds.push_back(region->second);
}

void exportCoresAndClaims(SimulationExport &exported, const State &state,
                          const std::map<ProvinceId, RegionId> &provinceRegions,
                          const std::vector<Event> &events,
                          const SimulationPolityHistory &polityHistory,
                          const Configuration &configuration) {
  const auto addClaim = [&exported](PolityId polityId, RegionId regionId,
                                    ClaimSource source) {
    if (polityId == NoPolity || regionId < 0)
      return;
    auto &records = exported.claimRecords;
    if (std::none_of(records.begin(), records.end(),
                     [polityId, regionId, source](const auto &record) {
                       return record.polityId == polityId &&
                              record.regionId == regionId &&
                              record.source == source;
                     }))
      records.push_back({polityId, regionId, source});
    appendRegion(exported.claims[polityId], regionId);
  };

  const auto episodes = buildOwnershipEpisodes(state, events);
  Year coreDuration = configuration.coreOwnershipYears;
  if (state.year >= configuration.ideologicalTransitionStartYear)
    coreDuration = configuration.modernCoreOwnershipYears;
  else if (state.year >= configuration.industrialRevolutionStartYear)
    coreDuration = configuration.industrialCoreOwnershipYears;
  coreDuration = std::max<Year>(0, coreDuration);
  const auto claimDuration = std::max<Year>(0, configuration.coreClaimYears);
  for (const auto &[provinceId, provinceEpisodes] : episodes) {
    for (const auto &episode : provinceEpisodes) {
      if (episode.endYear - episode.startYear < coreDuration)
        continue;
      const auto region = provinceRegions.find(provinceId);
      if (region == provinceRegions.end())
        continue;
      auto &cores = exported.cores[episode.polityId];
      appendRegion(cores, region->second);
      const auto *province = state::findProvince(state, provinceId);
      if (episode.endYear < state.year &&
          state.year - episode.endYear <= claimDuration &&
          (!province || province->owner != episode.polityId)) {
        addClaim(episode.polityId, region->second,
                 ClaimSource::HistoricalOwnership);
      }
    }
  }

  std::map<RegionId, PolityId> currentRegionOwners;
  for (const auto &[provinceId, regionId] : provinceRegions) {
    const auto *province = state::findProvince(state, provinceId);
    if (province && province->owner != NoPolity)
      currentRegionOwners.try_emplace(regionId, province->owner);
  }
  std::set<RegionId> regionSimulationRegions;
  for (const auto &event : events)
    if (event.regionId >= 0 &&
        event.year >= configuration.regionOwnershipYear &&
        (event.type == EventType::ConsolidateRegion ||
         event.type == EventType::TransferProvince ||
         event.type == EventType::ColonizeProvince))
      regionSimulationRegions.insert(event.regionId);

  for (const auto &[regionId, historicalOwners] :
       polityHistory.historicalRegionOwners) {
    if (regionSimulationRegions.contains(regionId))
      continue;
    const auto currentOwner = currentRegionOwners.find(regionId);
    for (const auto historicalOwner : historicalOwners) {
      if (currentOwner != currentRegionOwners.end() &&
          historicalOwner == currentOwner->second)
        continue;
      addClaim(historicalOwner, regionId, ClaimSource::HistoricalOwnership);
    }
  }

  for (const auto &polity : state.polities) {
    if (!polity.initialized || !polity.dissolvedYear ||
        state.year - *polity.dissolvedYear > claimDuration)
      continue;
    const auto cores = exported.cores.find(polity.id);
    if (cores == exported.cores.end())
      continue;
    for (const auto regionId : cores->second.regionIds)
      addClaim(polity.id, regionId, ClaimSource::HistoricalOwnership);
  }

  std::map<PolityId, std::set<int>> landMasses;
  std::map<PolityId, std::set<int>> continents;
  std::map<PolityId, int> capitalLandMass;
  std::map<PolityId, CultureId> primaryCultures;
  for (const auto &[polityId, polityExport] : exported.polities) {
    primaryCultures[polityId] = polityExport.polity.primaryCulture;
    for (const auto provinceId : polityExport.provinceIds) {
      const auto *province = state::findProvince(state, provinceId);
      if (!province)
        continue;
      if (province->landMassID >= 0)
        landMasses[polityId].insert(province->landMassID);
      if (province->continentId >= 0)
        continents[polityId].insert(province->continentId);
    }
    const auto *capital = state::findProvince(
        state, polityExport.polity.capitalProvince);
    if (capital)
      capitalLandMass[polityId] = capital->landMassID;
  }
  for (const auto &[regionId, provinceIds] : exported.regions) {
    for (const auto &[polityId, polityExport] : exported.polities) {
      bool cultureMatch = false;
      bool capitalLandMassMatch = false;
      bool detachedLandMassMatch = false;
      bool continentMatch = false;
      for (const auto provinceId : provinceIds) {
        const auto *province = state::findProvince(state, provinceId);
        if (!province)
          continue;
        cultureMatch |= primaryCultures[polityId] >= 0 &&
                        province->culture == primaryCultures[polityId];
        capitalLandMassMatch |= capitalLandMass.contains(polityId) &&
                                province->landMassID >= 0 &&
                                province->landMassID == capitalLandMass[polityId];
        detachedLandMassMatch |=
            province->landMassID >= 0 &&
            capitalLandMass.contains(polityId) &&
            province->landMassID != capitalLandMass[polityId] &&
            landMasses[polityId].contains(province->landMassID);
        continentMatch |= province->continentId >= 0 &&
                          continents[polityId].contains(province->continentId);
      }
      if (cultureMatch)
        addClaim(polityId, regionId, ClaimSource::CultureUnification);
      if (capitalLandMassMatch)
        addClaim(polityId, regionId,
                 ClaimSource::CapitalLandmassUnification);
      if (detachedLandMassMatch)
        addClaim(polityId, regionId,
                 ClaimSource::DetachedLandmassUnification);
      if (continentMatch)
        addClaim(polityId, regionId, ClaimSource::ContinentUnification);
      (void)polityExport;
    }
  }
}

void exportProvinceData(SimulationExport &exported, const State &state,
                        const std::map<ProvinceId, RegionId> &provinceRegions,
                        const std::map<ProvinceId, size_t> &provinceAreas,
                        const ProvinceExportMetrics &metrics) {
  for (ProvinceId provinceId = 0;
       provinceId < static_cast<ProvinceId>(state.provinces.size());
       ++provinceId) {
    const auto *province = state::findProvince(state, provinceId);
    if (!province)
      continue;
    const auto region = provinceRegions.find(provinceId);
    const auto regionId = region == provinceRegions.end() ? -1 : region->second;
    const auto area = provinceAreas.find(provinceId);
    const auto populationDensity =
        area != provinceAreas.end() && area->second > 0
            ? province->population / static_cast<double>(area->second)
            : 0.0;
    exported.provinces.emplace(
        provinceId,
        SimulationProvinceExport{
            provinceId, regionId, province->owner, province->culture,
            province->religion, province->population,
            metrics.maximumPopulationDensity > 0.0
                ? populationDensity / metrics.maximumPopulationDensity
                : 0.0,
            province->development,
            metrics.maximumDevelopment > 0.0
                ? province->development / metrics.maximumDevelopment
                : 0.0,
            province->overseas, province->colony});
    if (regionId >= 0)
      exported.regions[regionId].push_back(provinceId);
  }
}

void exportActiveCivilizationLineages(SimulationExport &exported,
                                      const State &state) {
  std::set<CultureId> activeCultures;
  std::set<ReligionId> activeReligions;
  for (ProvinceId provinceId = 0;
       provinceId < static_cast<ProvinceId>(state.provinces.size());
       ++provinceId) {
    const auto *province = state::findProvince(state, provinceId);
    if (!province)
      continue;
    if (province->culture >= 0)
      activeCultures.insert(province->culture);
    if (province->religion != NoReligion)
      activeReligions.insert(province->religion);
  }

  for (auto culture = exported.cultures.begin();
       culture != exported.cultures.end();) {
    if (!activeCultures.contains(culture->first))
      culture = exported.cultures.erase(culture);
    else
      ++culture;
  }
  for (auto religion = exported.religions.begin();
       religion != exported.religions.end();) {
    if (!activeReligions.contains(religion->first))
      religion = exported.religions.erase(religion);
    else
      ++religion;
  }
}

void exportHistoricalOwnership(
    SimulationExport &exported, const State &state,
    const std::map<ProvinceId, RegionId> &provinceRegions,
    const SimulationPolityHistory &polityHistory) {
  for (const auto &[regionId, owners] : polityHistory.historicalRegionOwners) {
    auto &exportedOwners = exported.historicalRegionOwners[regionId];
    exportedOwners.assign(owners.begin(), owners.end());
  }

  const auto lineageRoot = [&state](PolityId polityId) {
    std::set<PolityId> visited;
    while (polityId != NoPolity && visited.insert(polityId).second) {
      const auto *polity = state::findPolity(state, polityId);
      if (!polity || !polity->predecessorId)
        break;
      polityId = *polity->predecessorId;
    }
    return polityId;
  };
  for (auto &[regionId, owners] : exported.historicalRegionOwners) {
    std::vector<PolityId> consolidatedOwners;
    for (const auto owner : owners) {
      const auto root = lineageRoot(owner);
      if (std::find(consolidatedOwners.begin(), consolidatedOwners.end(),
                    root) == consolidatedOwners.end())
        consolidatedOwners.push_back(root);
    }
    owners = std::move(consolidatedOwners);
  }

  for (const auto &[polityId, provinceIds] : polityHistory.peakProvinceIds) {
    auto &peakExport = exported.historicalPolityPeaks[polityId];
    peakExport.provinceIds = provinceIds;
    for (const auto provinceId : provinceIds) {
      const auto region = provinceRegions.find(provinceId);
      if (region != provinceRegions.end() &&
          std::find(peakExport.regionIds.begin(), peakExport.regionIds.end(),
                    region->second) == peakExport.regionIds.end())
        peakExport.regionIds.push_back(region->second);
    }
  }
}

void exportSignificantPolities(SimulationExport &exported, const State &state) {
  double strongestPolityScore = 0.0;
  for (const auto &[polityId, strength] : state.polityStrengths)
    strongestPolityScore = std::max(strongestPolityScore, strength.score);

  constexpr double significantSizeRatio = 0.05;
  constexpr double recentCollapseSizeRatio = 0.01;
  constexpr Year recentCollapseYears = 100;
  std::set<PolityId> currentPolities;
  for (ProvinceId provinceId = 0;
       provinceId < static_cast<ProvinceId>(state.provinces.size());
       ++provinceId)
    if (const auto *province = state::findProvince(state, provinceId);
        province && province->owner != NoPolity)
      currentPolities.insert(province->owner);

  for (PolityId polityId = 0;
       polityId < static_cast<PolityId>(state.polities.size()); ++polityId) {
    const auto *polity = state::findPolity(state, polityId);
    if (!polity)
      continue;
    const auto strength = state.polityStrengths.find(polityId);
    const auto score =
        strength == state.polityStrengths.end() ? 0.0 : strength->second.score;
    const bool recentlyDissolved =
        polity->dissolvedYear && *polity->dissolvedYear <= state.year &&
        state.year - *polity->dissolvedYear <= recentCollapseYears;
    const double requiredRatio =
        recentlyDissolved ? recentCollapseSizeRatio : significantSizeRatio;
    const bool ownsFinalProvince = currentPolities.contains(polityId);
    if (!ownsFinalProvince && (strongestPolityScore <= 0.0 ||
                               score < strongestPolityScore * requiredRatio))
      continue;

    SimulationPolityExport polityExport;
    polityExport.polity = *polity;
    if (strength != state.polityStrengths.end())
      polityExport.strength = strength->second;
    for (const auto &[provinceId, province] : exported.provinces) {
      if (province.polityId != polityId)
        continue;
      polityExport.provinceIds.push_back(provinceId);
      if (province.regionId >= 0 &&
          std::find(polityExport.regionIds.begin(),
                    polityExport.regionIds.end(),
                    province.regionId) == polityExport.regionIds.end())
        polityExport.regionIds.push_back(province.regionId);
    }
    const auto [exportedPolity, inserted] =
        exported.polities.emplace(polityId, std::move(polityExport));
    if (inserted) {
      auto &formStrength =
          exported.governmentFormStrength[
              exportedPolity->second.polity.governmentForm];
      formStrength.population += exportedPolity->second.strength.population;
      formStrength.development += exportedPolity->second.strength.development;
      formStrength.score += exportedPolity->second.strength.score;
    }
  }
}

void exportPolityStatistics(SimulationExport &exported) {
  for (auto &[polityId, polityExport] : exported.polities) {
    auto &statistics = polityExport.statistics;
    const auto cores = exported.cores.find(polityId);
    if (cores != exported.cores.end())
      statistics.coreProvinceCount = cores->second.regionIds.size();

    std::set<RegionId> coreRegionIds;
    if (cores != exported.cores.end())
      coreRegionIds.insert(cores->second.regionIds.begin(),
                           cores->second.regionIds.end());
    for (const auto regionId : polityExport.regionIds)
      if (!coreRegionIds.contains(regionId))
        ++statistics.nonCoreProvinceCount;

    const auto claims = exported.claims.find(polityId);
    if (claims != exported.claims.end())
      statistics.claimProvinceCount = claims->second.regionIds.size();
  }

  for (const auto &entry : exported.formables) {
    const auto &formable = entry.second;
    const auto owner = exported.polities.find(formable.currentOwnerId);
    if (owner == exported.polities.end())
      continue;
    ++owner->second.statistics.formableCount;
    ++owner->second.statistics.formableCountsByRequiredProvinceCount[
        formable.requiredProvinceIds.size()];
  }
}

} // namespace

SimulationExport SimulationExporter::exportFinalState(
    const State &state, const std::map<ProvinceId, RegionId> &provinceRegions,
    const SimulationPolityHistory &polityHistory,
    const std::vector<Event> &events, const std::vector<WarEvent> &wars,
    const std::map<ProvinceId, size_t> &provinceAreas,
     const Configuration &configuration) {
  SimulationExport exported;
  exported.year = state.year;
  exported.cultures = state.cultures;
  exported.religions = state.religions;
  exported.wars = wars;
  exportActiveCivilizationLineages(exported, state);

  const auto metrics = calculateProvinceExportMetrics(state, provinceAreas);
  exportProvinceData(exported, state, provinceRegions, provinceAreas, metrics);
  exportHistoricalOwnership(exported, state, provinceRegions, polityHistory);
  exportSignificantPolities(exported, state);
  exportCoresAndClaims(exported, state, provinceRegions, events,
                       polityHistory, configuration);
  exportFormables(exported, state, provinceRegions, polityHistory);
  exportPolityStatistics(exported);
  return exported;
}

} // namespace Arda::Simulation
