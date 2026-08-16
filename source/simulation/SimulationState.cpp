#include "simulation/SimulationState.h"

#include <algorithm>
#include <stdexcept>

namespace Arda::Simulation {

const ProvinceState *State::findProvince(ProvinceId provinceId) const {
  return state::findProvince(*this, provinceId);
}

std::vector<ProvinceId> State::territoryOf(PolityId polityId) const {
  std::vector<ProvinceId> territory;
  for (ProvinceId provinceId = 0;
       provinceId < static_cast<ProvinceId>(provinces.size()); ++provinceId)
    if (const auto *province = state::findProvince(*this, provinceId);
        province && province->owner == polityId)
      territory.push_back(provinceId);
  return territory;
}

CultureId State::dominantCultureOf(ProvinceId provinceId) const {
  const auto *province = state::findProvince(*this, provinceId);
  if (!province || province->culturePopulations.empty())
    return -1;
  return province->culture;
}

namespace state {

const ProvinceState *findProvince(const State &state, ProvinceId provinceId) {
  if (provinceId < 0 ||
      static_cast<std::size_t>(provinceId) >= state.provinces.size() ||
      !state.provinces[static_cast<std::size_t>(provinceId)].initialized)
    return nullptr;
  return &state.provinces[static_cast<std::size_t>(provinceId)];
}

ProvinceState *findProvince(State &state, ProvinceId provinceId) {
  return const_cast<ProvinceState *>(
      findProvince(static_cast<const State &>(state), provinceId));
}

const Polity *findPolity(const State &state, PolityId polityId) {
  if (polityId < 0 ||
      static_cast<std::size_t>(polityId) >= state.polities.size() ||
      !state.polities[static_cast<std::size_t>(polityId)].initialized)
    return nullptr;
  return &state.polities[static_cast<std::size_t>(polityId)];
}

Polity *findPolity(State &state, PolityId polityId) {
  return const_cast<Polity *>(
      findPolity(static_cast<const State &>(state), polityId));
}

ProvinceState &ensureProvince(State &state, ProvinceId provinceId) {
  if (provinceId < 0)
    throw std::out_of_range("negative province ID");
  const auto index = static_cast<std::size_t>(provinceId);
  if (state.provinces.size() <= index)
    state.provinces.resize(index + 1);
  return state.provinces[index];
}

Polity &ensurePolity(State &state, PolityId polityId) {
  if (polityId < 0)
    throw std::out_of_range("negative polity ID");
  const auto index = static_cast<std::size_t>(polityId);
  if (state.polities.size() <= index)
    state.polities.resize(index + 1);
  return state.polities[index];
}

std::map<PolityId, std::vector<ProvinceId>>
territoriesByPolity(const State &state) {
  std::map<PolityId, std::vector<ProvinceId>> territories;
  for (ProvinceId provinceId = 0;
       provinceId < static_cast<ProvinceId>(state.provinces.size());
       ++provinceId)
    if (const auto *province = findProvince(state, provinceId);
        province && province->owner != NoPolity)
      territories[province->owner].push_back(provinceId);
  return territories;
}

const std::set<ProvinceId> &coastalProvincesOf(const State &state,
                                               PolityId polityId) {
  static const std::set<ProvinceId> empty;
  const auto coastal = state.coastalProvinceIds.find(polityId);
  return coastal == state.coastalProvinceIds.end() ? empty : coastal->second;
}

void setCapital(State &state, PolityId polityId, ProvinceId provinceId) {
  auto *polity = findPolity(state, polityId);
  if (!polity)
    return;
  polity->capitalProvince = provinceId;
  const auto province = state.geography.find(provinceId);
  polity->capital =
      province == state.geography.end() ? nullptr : province->second;
}

const std::shared_ptr<ArdaProvince> &capitalOf(const State &state,
                                                PolityId polityId) {
  static const std::shared_ptr<ArdaProvince> empty;
  const auto *polity = findPolity(state, polityId);
  return polity ? polity->capital : empty;
}

void refreshDominantCulture(ProvinceState &province) {
  if (province.culturePopulations.empty()) {
    province.culture = -1;
    return;
  }
  province.culture = std::max_element(province.culturePopulations.begin(),
                                      province.culturePopulations.end(),
                                      [](const auto &left, const auto &right) {
                                        return left.second < right.second;
                                      })
                         ->first;
}

void relocateCapital(State &state, PolityId polityId) {
  auto *polity = findPolity(state, polityId);
  if (!polity)
    return;
  const auto *capital = findProvince(state, polity->capitalProvince);
  if (capital && capital->owner == polityId)
    return;
  ProvinceId replacement = NoPolity;
  for (ProvinceId provinceId = 0;
       provinceId < static_cast<ProvinceId>(state.provinces.size());
       ++provinceId) {
    const auto *province = findProvince(state, provinceId);
    if (!province || province->owner != polityId)
      continue;
    if (replacement == NoPolity ||
        province->population > findProvince(state, replacement)->population)
      replacement = provinceId;
  }
  setCapital(state, polityId, replacement);
}

} // namespace state
} // namespace Arda::Simulation
