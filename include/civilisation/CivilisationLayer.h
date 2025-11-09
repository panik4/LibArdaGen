#pragma once
#include <optional>
#include <unordered_map>
#include <vector>

namespace Arda::Civilization {

enum class TopographyType : int {
  // Hydrological
  MARSH,
  WETLANDS,
  FLOODLANDS,
  SWAMP,
  DELTA,

  // Dry or barren
  WASTELAND,
  SCRUBLAND,
  BADLANDS,
  SALTPLAIN,

  // Human-modified
  FARMLAND,
  CITY,
  SUBURBS,
  INDUSTRIAL,
  RUINS,
  MINE,

  // Hazardous / unstable
  IMPASSABLE,
  VOLCANIC,
  CRATER,
  LANDSLIDE_ZONE,
  ERODED,
};
struct CivilizationLayer {
  using Mask = uint32_t;

  struct TileTopo {
    Mask flags = 0;

    void set(TopographyType type) {
      flags |= (1u << static_cast<uint8_t>(type));
    }

    void clear(TopographyType type) {
      flags &= ~(1u << static_cast<uint8_t>(type));
    }

    bool has(TopographyType type) const {
      return flags & (1u << static_cast<uint8_t>(type));
    }
  };

  std::vector<TileTopo> tiles;

  // Only types that have extra data need to use this
  std::unordered_map<TopographyType, std::unordered_map<int, float>> extraData;

  void ensureSize(int index) {
    if (index >= tiles.size())
      tiles.resize(index + 1);
  }

  void set(int index, TopographyType type,
           std::optional<float> extra = std::nullopt) {
    ensureSize(index);
    tiles[index].set(type);

    if (extra) {
      extraData[type][index] = *extra;
    }
  }

  void clear(int index, TopographyType type) {
    if (index >= tiles.size())
      return;
    tiles[index].clear(type);
    extraData[type].erase(index);
  }

  bool has(int index, TopographyType type) const {
    return index < tiles.size() && tiles[index].has(type);
  }

  std::optional<float> getData(int index, TopographyType type) const {
    if (!has(index, type))
      return std::nullopt;

    auto itType = extraData.find(type);
    if (itType != extraData.end()) {
      auto itVal = itType->second.find(index);
      if (itVal != itType->second.end()) {
        return itVal->second;
      }
    }
    return std::nullopt;
  }

  void clear(TopographyType type) {
    // 1. Clear the bit flag from every tile
    for (auto &tile : tiles) {
      tile.clear(type);
    }

    // 2. Remove all associated extra data
    extraData.erase(type);
  }

  std::vector<int> getAll(TopographyType type) const {
    std::vector<int> result;
    result.reserve(tiles.size() / 8); // small optimization guess
    for (int i = 0; i < static_cast<int>(tiles.size()); ++i) {
      if (tiles[i].has(type))
        result.push_back(i);
    }
    return result;
  }

  std::vector<std::pair<int, float>> getAllWithData(TopographyType type) const {
    std::vector<std::pair<int, float>> result;
    auto itType = extraData.find(type);
    if (itType == extraData.end())
      return result; // no data for this type

    result.reserve(itType->second.size());
    for (auto &[index, value] : itType->second) {
      // only include if tile still has that flag (for safety)
      if (index < static_cast<int>(tiles.size()) && tiles[index].has(type))
        result.emplace_back(index, value);
    }
    return result;
  }

  int countOfTypeInRange(const std::vector<int> &indices,
                         TopographyType type) const {
    if (tiles.empty() || indices.empty())
      return 0;

    int count = 0;
    const int tileCount = static_cast<int>(tiles.size());

    for (int idx : indices) {
      if (idx >= 0 && idx < tileCount && this->has(idx, type))
        ++count;
    }

    return count;
  }

  std::vector<float> wastelandChance;
  void clear() {
    tiles.clear();
    extraData.clear();
    wastelandChance.clear();
  }

  size_t byteSize() const { return 0; }
};

} // namespace Arda::Civilization