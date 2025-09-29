#pragma once
#include "RandNum.h"
#include "language/Language.h"
#include <algorithm>
#include <filesystem>
namespace Arda {
class LanguageGroup {

public:
  std::string name;
  std::vector<std::shared_ptr<Language>> languages;
  Dataset mergedDataset;
  void generate(int languageAmount, const Dataset &dataset);
};
} // namespace Arda