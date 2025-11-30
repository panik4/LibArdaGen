#pragma once
#include "language/Language.h"
#include "entities/Colour.h"
#include "areas/Area.h"
#include <string>
namespace Arda {
class CultureGroup;

enum VisualType { ASIAN, AFRICAN, ARABIC, CAUCASIAN, SOUTH_AMERICAN };

class Culture {

public:
  std::string name;
  std::string adjective;
  // the area that is the center
  std::shared_ptr<Fwg::Areas::Area> centerOfCulture;
  Fwg::Gfx::Colour colour;
  std::shared_ptr<Arda::Language> language;
  std::shared_ptr<CultureGroup> cultureGroup;
  VisualType visualType;
};

} // namespace Arda