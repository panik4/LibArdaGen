#pragma once
#include <string>
#include "entities/Colour.h"
namespace Arda {
class Religion {

public:
  std::string name;
  // ID of the province that is the center
  int centerOfReligion;
  Fwg::Gfx::Colour colour;
 //std::vector<Arda::Arda::ArdaProvince> centersOfReligion;
};

} // namespace Arda