#include "culture/CultureGroup.h"
#include "utils/Archive.h"

void Arda::CultureGroup::determineVisualType() {
  // placeholder, select one of the VisualTypes randomly
  visualType = static_cast<VisualType>(RandNum::getRandom(5));
}

void Arda::CultureGroup::serialise(Fwg::Utils::Serialisation::Archive &ar) {
  ar &name;
  ar.ptrVector(cultures);
  colour.serialise(ar);
  ar &languageGroup;
  ar &center;
  ar.serialiseEnum(visualType);
}