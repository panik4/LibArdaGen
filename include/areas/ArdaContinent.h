#pragma once
#include "areas/ArdaProvince.h"
#include "areas/ArdaRegion.h"
#include "areas/Continent.h"
#include "utils/SerialisationFwd.h"

namespace Fwg::Utils::Serialisation {
class Archive;
}

namespace Arda {
class ArdaContinent : public Fwg::Areas::Continent {

public:
  std::string name;
  std::string adjective;
  // value between 0.0 and 1.0
  double developmentModifier = 1.0;
  double totalEconomicActivity = 0.0;
  double worldPopulationShare = 0.0;
  double worldEconomicActivityShare = 0.0;
  std::vector<std::shared_ptr<Arda::ArdaProvince>> ardaProvinces;
  std::vector<std::shared_ptr<Arda::ArdaRegion>> ardaRegions;

  ArdaContinent(const Continent &continent);
  ArdaContinent() = default;
  ~ArdaContinent();

  // serialisation
  template<class Archive>
  void serialize(Archive &ar, const unsigned int /*version*/);
};
} // namespace Arda

BOOST_CLASS_EXPORT_KEY(Arda::ArdaContinent)

