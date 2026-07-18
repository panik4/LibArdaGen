#include "culture/Culture.h"
#include "culture/CultureGroup.h"
#include "utils/Archive.h"

namespace Arda {
void Culture::serialise(Fwg::Utils::Serialisation::Archive &ar) {
  ar &name &adjective;
  ar.polymorphicPtr(centerOfCulture);
  colour.serialise(ar);
  ar &language;
  ar &cultureGroup;
  ar.serialiseEnum(visualType);
}
} // namespace Arda