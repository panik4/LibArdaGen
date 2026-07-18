#pragma once
#include "entities/Colour.h"
#include "utils/Archive.h"
#include <string>
namespace Arda {
class Religion {

public:
  std::string name;
  // ID of the province that is the center
int centerOfReligion;
  Fwg::Gfx::Colour colour;

  void serialise(Fwg::Utils::Serialisation::Archive &ar) {
    ar &name &centerOfReligion;
    colour.serialise(ar);
  }
  void deserialise(Fwg::Utils::Serialisation::Archive &ar) {
    serialise(ar);
  }
};

} // namespace Arda