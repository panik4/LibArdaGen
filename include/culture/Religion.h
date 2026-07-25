#pragma once
#include "utils/SerialisationFwd.h"
#include "entities/Colour.h"
#include <string>
namespace Arda {
class Religion {

public:
  std::string name;
  // ID of the province that is the center
  int centerOfReligion;
  Fwg::Gfx::Colour colour;

  template <class Archive>
  void serialize(Archive &ar, const unsigned int /*version*/) {
    ar & name & centerOfReligion & colour;
  }
};

} // namespace Arda