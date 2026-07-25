#include "areas/ArdaProvince.h"
namespace Arda {
ArdaProvince::ArdaProvince(std::shared_ptr<Fwg::Areas::Province> province) {}

ArdaProvince::ArdaProvince() {}

ArdaProvince::~ArdaProvince() {}

std::string ArdaProvince::toHexString(bool prefix, bool uppercase) {
  std::string hexString = "";
  if (prefix) {
    hexString.append("x");
  }
  std::stringstream stream;
  for (int i = 2; i >= 0; i--)
    stream << std::setfill('0') << std::setw(sizeof(char) * 2)
           << (uppercase ? std::nouppercase : std::nouppercase) << std::hex
           << (int)this->colour.getBGR()[i];
  hexString.append(stream.str());
  return hexString;
}

template <class Archive>
void ArdaProvince::serialize(Archive &ar, const unsigned int /*version*/) {
  ar &boost::serialization::base_object<Fwg::Areas::Province>(*this);
  ar & owner & terrainType;
  if constexpr (Archive::is_saving::value) {
    std::vector<int> types;
    for (auto t : topographyTypes)
      types.push_back(static_cast<int>(t));
    ar & types;
  } else {
    std::vector<int> types;
    ar & types;
    topographyTypes.clear();
    for (int t : types)
      topographyTypes.insert(static_cast<Civilization::TopographyType>(t));
  }
  ar & populationDensity & worldPopulationShare & population;
  ar & worldGdpShare & gdp;
  ar & victoryPoint;
  ar & positions;
  ar & religions;
  ar & cultures;
}

} // namespace Arda

BOOST_CLASS_EXPORT_IMPLEMENT(Arda::ArdaProvince)
template void Arda::ArdaProvince::serialize(boost::archive::binary_oarchive &,
                                            unsigned int);
template void Arda::ArdaProvince::serialize(boost::archive::binary_iarchive &,
                                            unsigned int);
