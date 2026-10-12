#pragma once
#include "civilization/Location.h"
#include "utils/SerialisationFwd.h"

namespace Arda {
struct VictoryPoint {
  int amount;
  Fwg::Position position;
  std::shared_ptr<Fwg::Civilization::Location> location;

  template<class Archive>
  void serialize(Archive &ar, const unsigned int /*version*/) {
    ar &amount &position &location;
  }
};
} // namespace Arda