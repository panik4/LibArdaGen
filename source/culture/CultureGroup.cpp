#include "culture/CultureGroup.h"

void Arda::CultureGroup::determineVisualType() {
  // placeholder, select one of the VisualTypes randomly
  visualType = static_cast<VisualType>(rand() % 5);
}