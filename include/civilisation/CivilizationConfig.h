#pragma once

#include <cmath>
#include <stdexcept>

namespace Arda::Civilization {

struct CivilizationConfig {
  bool randomDevelopment = true;
  double minimumDevelopment = 0.5;
  double maximumDevelopment = 1.0;
  double developmentInfluence = 0.5;
  double wastelandThreshold = 0.90;

  void validate() const {
    if (!std::isfinite(minimumDevelopment) ||
        !std::isfinite(maximumDevelopment) ||
        !std::isfinite(developmentInfluence) ||
        !std::isfinite(wastelandThreshold) ||
        minimumDevelopment < 0.0 ||
        maximumDevelopment < minimumDevelopment ||
        developmentInfluence < 0.0 || wastelandThreshold < 0.0 ||
        wastelandThreshold > 1.0)
      throw std::invalid_argument("Invalid civilization generation settings");
  }

  template <class Archive>
  void serialize(Archive &ar, const unsigned int /*version*/) {
    ar & randomDevelopment & minimumDevelopment & maximumDevelopment &
        developmentInfluence & wastelandThreshold;
  }
};

} // namespace Arda::Civilization
