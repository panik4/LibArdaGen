#pragma once
#include <optional>
#include <unordered_map>
#include <vector>

namespace Arda::Civilization {
enum class TopographyType {
  MARSH,
  WETLANDS,
  FLOODLANDS,
  IMPASSABLE,
  FARMLAND,
  CITY,
  SUBURBS
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
  std::vector<float> wastelandChance;
  void clear() {}

  size_t byteSize() const { return 0; }
};

} // namespace Fwg::Civilization