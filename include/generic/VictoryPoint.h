#pragma once
#include "utils/SerialisationFwd.h"

namespace Arda {
struct VictoryPoint {
  int amount;
  Fwg::Position position;
  std::string name;

  template<class Archive>
  void serialize(Archive &ar, const unsigned int /*version*/) {
    ar &amount &position &name;
  }
};
} // namespace Arda