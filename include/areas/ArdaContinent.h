#pragma once
#include "areas/Continent.h"
namespace Arda {
class ArdaContinent : public Fwg::Areas::Continent {
public:
  ArdaContinent(const Continent &continent);
  ~ArdaContinent();
};
} // namespace Arda