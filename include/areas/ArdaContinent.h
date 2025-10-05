#pragma once
#include "areas/Continent.h"
namespace Arda {
class ArdaContinent : public Fwg::Areas::Continent {

public:
  // value between 0.0 and 1.0
  double developmentModifier = 1.0;
  double totalEconomicActivity = 0.0;
  double worldPopulationShare = 0.0;
  double worldEconomicActivityShare = 0.0;





  ArdaContinent(const Continent &continent);
  ~ArdaContinent();
};
} // namespace Arda