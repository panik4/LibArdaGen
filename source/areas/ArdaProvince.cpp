#include "areas/ArdaProvince.h"
#include "utils/Archive.h"
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

void ArdaProvince::serialise(Fwg::Utils::Serialisation::Archive &ar) {
  Province::serialise(ar);
  ar &owner &terrainType;
  // topographyTypes is a set of enum - serialise as int vector
  if (ar.isWriting()) {
    std::vector<int> types;
    for (auto t : topographyTypes)
      types.push_back(static_cast<int>(t));
    ar &types;
  } else {
    std::vector<int> types;
    ar &types;
    topographyTypes.clear();
    for (int t : types)
      topographyTypes.insert(static_cast<Civilization::TopographyType>(t));
  }
  ar &populationDensity &worldPopulationShare &population;
  ar &worldGdpShare &gdp;
  ar &victoryPoint;
  // positions is vector<ScenarioPosition> - manual loop
  if (ar.isWriting()) {
    uint64_t sz = positions.size();
    ar &sz;
    for (auto &p : positions)
      p.serialise(ar);
  } else {
    uint64_t sz;
    ar &sz;
    positions.resize(static_cast<size_t>(sz));
    for (auto &p : positions)
      p.deserialise(ar);
  }
  ar &religions;
  ar &cultures;
}

void ArdaProvince::deserialise(Fwg::Utils::Serialisation::Archive &ar) {
  serialise(ar);
}

uint32_t ArdaProvince::typeTag() const {
  return Fwg::Utils::Serialisation::TypeRegistry::hashString(
      "Arda::ArdaProvince");
}

} // namespace Arda