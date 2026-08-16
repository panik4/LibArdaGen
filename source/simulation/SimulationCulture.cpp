#include "simulation/SimulationCulture.h"
#include "simulation/SimulationState.h"

#include "RandNum.h"
#include "simulation/SimulationDevelopment.h"
#include "simulation/SimulationWarfare.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Arda::Simulation::culture {
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

} // namespace

void integrate(const Year nextYear, double centuries,
               const Configuration &configuration, State &state,
               const AppendEvent &append) {
  double stabilityThreshold = 0.0;
  for (const auto &[polityId, strength] : state.polityStrengths)
    stabilityThreshold = std::max(stabilityThreshold, strength.score * 0.5);
  for (ProvinceId provinceId = 0;
       provinceId < static_cast<ProvinceId>(state.provinces.size()); ++provinceId) {
    const auto *province = state::findProvince(state, provinceId);
    if (!province || !state.polityStrengths.contains(province->owner) ||
        state.polityStrengths.at(province->owner).score < stabilityThreshold ||
        province->culturePopulations.size() < 2)
      continue;
    const auto dominantCulture = state.dominantCultureOf(provinceId);
    for (const auto &[cultureId, culturalPopulation] :
          province->culturePopulations) {
      if (cultureId == dominantCulture)
        continue;
      const auto conversion = culturalPopulation *
                              configuration.culturalIntegrationRatePerCentury *
                              centuries;
      if (conversion > 0.0)
        append({nextYear, EventType::ConvertCulturePopulation, provinceId, -1,
                NoPolity, NoPolity, dominantCulture, NoReligion, conversion,
                0.0, "stable polity cultural integration", cultureId});
    }
  }
}

void evolveAndReligions(const Year nextYear, double centuries,
                        const Configuration &configuration,
                        const NormalizedInput &normalized, State &state,
                        const AppendEvent &append, CultureId &nextCulture,
                        ReligionId &nextReligion) {
  if (nextYear < configuration.classicalStartYear)
    return;
  for (ProvinceId provinceId = 0;
       provinceId < static_cast<ProvinceId>(state.provinces.size()); ++provinceId) {
    const auto *province = state::findProvince(state, provinceId);
    if (!province)
      continue;
    if (province->religion == NoReligion &&
        chance(configuration.religionEmergenceChance * centuries)) {
      const auto religionId = nextReligion++;
      append({nextYear, EventType::CreateReligion, provinceId, -1, NoPolity,
              NoPolity, -1, religionId, 0.0, 0.0, "religion emergence", -1,
              randomColour()});
      append({nextYear, EventType::SetReligion, provinceId, -1, NoPolity,
              NoPolity, -1, religionId, 0.0, 0.0, "religion adoption"});
    }
    if (province->religion != NoReligion &&
        chance(configuration.religionSplitChance * centuries)) {
      const auto religionId = nextReligion++;
      append({nextYear, EventType::CreateReligion, provinceId, -1, NoPolity,
              NoPolity, -1, religionId, 0.0, 0.0, "religion split",
               province->religion, randomColour()});
      append({nextYear, EventType::SetReligion, provinceId, -1, NoPolity,
              NoPolity, -1, religionId, 0.0, 0.0, "religion schism"});
    }
    for (const auto neighbourId : normalized.neighbours.at(provinceId)) {
      const auto *neighbour = state::findProvince(state, neighbourId);
      if (!neighbour)
        continue;
      if (province->religion == NoReligion && neighbour->religion != NoReligion &&
          chance(configuration.religionSpreadChance * centuries)) {
        append({nextYear, EventType::SetReligion, provinceId, -1, NoPolity,
                NoPolity, -1, neighbour->religion, 0.0, 0.0, "religion spread"});
        break;
      }
      if (province->religion != NoReligion && neighbour->religion != NoReligion &&
          province->religion != neighbour->religion &&
          chance(configuration.religionConversionChance * centuries)) {
        append({nextYear, EventType::SetReligion, provinceId, -1, NoPolity,
                NoPolity, -1, neighbour->religion, 0.0, 0.0,
                "religious consolidation"});
        break;
      }
      if (province->culture == neighbour->culture &&
          chance(configuration.cultureSplitChance * centuries)) {
        const auto cultureId = nextCulture++;
        append({nextYear, EventType::CreateCulture, provinceId, -1, NoPolity,
                NoPolity, cultureId, NoReligion, 0.0, 0.0, "culture split",
                province->culture, randomColour()});
        append({nextYear, EventType::SetCulture, provinceId, -1, NoPolity,
                NoPolity, cultureId, NoReligion, 0.0, 0.0,
                "cultural differentiation"});
        break;
      }
    }

    const auto *polity = state::findPolity(state, province->owner);
    if (!polity || polity->primaryCulture < 0)
      continue;
    const auto primaryCulture = polity->primaryCulture;
    bool primaryCultureNeighbour = false;
    for (const auto neighbourId : normalized.neighbours.at(provinceId)) {
      const auto *neighbour = state::findProvince(state, neighbourId);
      if (neighbour && neighbour->culture == primaryCulture) {
        primaryCultureNeighbour = true;
        break;
      }
    }

    bool overseasAdoption = false;
    if (!primaryCultureNeighbour && province->island &&
        nextYear >= configuration.maritimeExpansionStartYear) {
      double closestSeaDistance = std::numeric_limits<double>::max();
      const auto routesToProvince = normalized.seaRoutesTo.find(provinceId);
      if (routesToProvince != normalized.seaRoutesTo.end())
        for (const auto &route : routesToProvince->second) {
          if (route.distance >= closestSeaDistance)
            continue;
          const auto *source = state::findProvince(state, route.provinceId);
          if (source && source->owner == province->owner && source->coastal)
            closestSeaDistance = route.distance;
        }
      const auto maritimeRange =
          warfare::maritimeRangeForYear(nextYear, configuration);
      if (closestSeaDistance <= maritimeRange &&
          chance(configuration.overseasCultureAdoptionChance * centuries *
                 std::exp(-closestSeaDistance / maritimeRange)))
        overseasAdoption = true;
    }

    if ((primaryCultureNeighbour || overseasAdoption) &&
        province->culture != primaryCulture &&
        chance(configuration.cultureAssimilationChance * centuries)) {
      append({nextYear, EventType::SetCulture, provinceId, -1, NoPolity,
              NoPolity, primaryCulture, NoReligion, 0.0, 0.0,
              overseasAdoption ? "overseas primary culture adoption"
                               : "polity primary culture expansion"});
    } else if (province->culture != primaryCulture && !primaryCultureNeighbour &&
               chance(configuration.primaryCultureChangeChance * centuries)) {
      for (const auto neighbourId : normalized.neighbours.at(provinceId)) {
        const auto *neighbour = state::findProvince(state, neighbourId);
        if (!neighbour)
          continue;
        const auto neighbourCulture = neighbour->culture;
        if (neighbourCulture != primaryCulture && neighbourCulture >= 0) {
          append({nextYear, EventType::SetPrimaryCulture, -1, -1,
                  province->owner, NoPolity, neighbourCulture, NoReligion, 0.0,
                  0.0, "rare polity primary culture change"});
          break;
        }
      }
    }
  }
}

} // namespace Arda::Simulation::culture
