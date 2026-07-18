#include "areas/ArdaContinent.h"
#include "utils/Archive.h"
namespace Arda {
ArdaContinent::ArdaContinent(const Continent &continent)
    : Continent(continent) {}

ArdaContinent::~ArdaContinent() {}

void ArdaContinent::serialise(Fwg::Utils::Serialisation::Archive &ar) {
  Continent::serialise(ar);
  ar &name &adjective;
  ar &developmentModifier &totalEconomicActivity;
  ar &worldPopulationShare &worldEconomicActivityShare;
  ar.polymorphicPtrVector(ardaProvinces);
  ar.polymorphicPtrVector(ardaRegions);
}

void ArdaContinent::deserialise(Fwg::Utils::Serialisation::Archive &ar) {
  serialise(ar);
}

uint32_t ArdaContinent::typeTag() const {
  return Fwg::Utils::Serialisation::TypeRegistry::hashString(
      "Arda::ArdaContinent");
}

} // namespace Arda