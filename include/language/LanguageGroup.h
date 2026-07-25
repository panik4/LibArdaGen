#pragma once
#include "language/Language.h"
#include "utils/SerialisationFwd.h"
#include <algorithm>
#include <filesystem>
namespace Arda {
class LanguageGroup {

public:
  std::string name;
  std::vector<std::shared_ptr<Language>> languages;
  Dataset mergedDataset;
  void generate(int languageAmount, const Dataset &dataset, int seed);

  template<class Archive>
  void serialize(Archive &ar, const unsigned int /*version*/) {
    ar &name &languages;
  }
};
} // namespace Arda