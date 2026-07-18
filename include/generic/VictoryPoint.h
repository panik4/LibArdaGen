#pragma once
#include "utils/Archive.h"

namespace Arda {
struct VictoryPoint {
  int amount;
  Fwg::Position position;
  std::string name;

  void serialise(Fwg::Utils::Serialisation::Archive &ar) {
    ar &amount;
    position.serialise(ar);
    ar &name;
  }
  void deserialise(Fwg::Utils::Serialisation::Archive &ar) {
    serialise(ar);
  }
};
} // namespace Arda