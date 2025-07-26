#pragma once
#include "language/Language.h"
#include "entities/Colour.h"
#include <string>
namespace Arda {
class CultureGroup;

enum VisualType { ASIAN, AFRICAN, ARABIC, CAUCASIAN, SOUTH_AMERICAN };

class Culture {

public:
  std::string name;
  std::string adjective;
  // ID of the province that is the center
  int centerOfCulture;
  Fwg::Gfx::Colour colour;
  std::shared_ptr<Arda::Language> language;
  std::shared_ptr<CultureGroup> cultureGroup;
  VisualType visualType;
};

} // namespace Arda