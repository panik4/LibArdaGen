#pragma once
#include "FastWorldGenerator.h"
#include "areas/ArdaRegion.h"
#include "areas/ArdaContinent.h"
#include "culture/CultureGroup.h"
#include "culture/Religion.h"
#include "utils/Archive.h"

namespace Arda::Civilization {
struct CivilizationData {
  std::vector<std::shared_ptr<Religion>> religions;
  std::vector<std::shared_ptr<Culture>> cultures;
  std::vector<std::shared_ptr<CultureGroup>> cultureGroups;

  std::vector<std::shared_ptr<Arda::LanguageGroup>> languageGroups;
  std::map<std::string, std::shared_ptr<Arda::Language>> languages;
  double worldPopulationFactorSum = 0.0;
  double worldEconomicActivitySum = 0.0;

  void serialise(Fwg::Utils::Serialisation::Archive &ar) {
    ar.ptrVector(religions);
    ar.ptrVector(cultures);
    ar.ptrVector(cultureGroups);
    ar.ptrVector(languageGroups);
    ar &languages;
    ar &worldPopulationFactorSum &worldEconomicActivitySum;
  }
  void deserialise(Fwg::Utils::Serialisation::Archive &ar) {
    serialise(ar);
  }
};

} // namespace Arda::Civilization
