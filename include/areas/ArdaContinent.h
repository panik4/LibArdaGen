#pragma once
#include "areas/ArdaProvince.h"
#include "areas/ArdaRegion.h"
#include "areas/Continent.h"
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
  ~ArdaContinent();
};
} // namespace Arda