#pragma once
#include "language/Language.h"
#include "utils/Archive.h"
#include <algorithm>
#include <filesystem>
namespace Arda {
class LanguageGroup {

public:
  std::string name;
  std::vector<std::shared_ptr<Language>> languages;
  Dataset mergedDataset;
  void generate(int languageAmount, const Dataset &dataset, int seed);

  void serialise(Fwg::Utils::Serialisation::Archive &ar) {
    ar &name;
    ar.ptrVector(languages);
  }
  void deserialise(Fwg::Utils::Serialisation::Archive &ar) {
    serialise(ar);
  }
};
} // namespace Arda