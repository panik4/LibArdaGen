#include "simulation/SimulationRandom.h"

#include "RandNum.h"

namespace Arda::Simulation::random {

bool chance(double probability) {
  return probability > 0.0 && RandNum::getRandom<double>(1.0) < probability;
}

Fwg::Gfx::Colour colour() {
  return {static_cast<unsigned char>(RandNum::getRandom<int>(256)),
          static_cast<unsigned char>(RandNum::getRandom<int>(256)),
          static_cast<unsigned char>(RandNum::getRandom<int>(256))};
}

} // namespace Arda::Simulation::random
