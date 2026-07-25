#include "areas/ArdaContinent.h"
namespace Arda {
ArdaContinent::ArdaContinent(const Continent &continent)
    : Continent(continent) {}

ArdaContinent::~ArdaContinent() {}

template <class Archive>
void ArdaContinent::serialize(Archive &ar, const unsigned int /*version*/) {
  ar &boost::serialization::base_object<Fwg::Areas::Continent>(*this);
  ar & name & adjective;
  ar & developmentModifier & totalEconomicActivity;
  ar & worldPopulationShare & worldEconomicActivityShare;
  ar & ardaProvinces;
  ar & ardaRegions;
}

} // namespace Arda

BOOST_CLASS_EXPORT_IMPLEMENT(Arda::ArdaContinent)
template void Arda::ArdaContinent::serialize(boost::archive::binary_oarchive &,
                                             unsigned int);
template void Arda::ArdaContinent::serialize(boost::archive::binary_iarchive &,
                                             unsigned int);
