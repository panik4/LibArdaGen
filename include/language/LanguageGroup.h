#pragma once
#include "LanguageGenerator.h"
#include "RandNum.h"
#include "language/Language.h"
#include <algorithm>
#include <filesystem>
namespace Arda {
class LanguageGroup {
  Dataset mergedDataset;
  std::string name;

public:
  std::vector<std::shared_ptr<Language>> languages;
  void generate(int languageAmount, std::string baseDir);
};
} // namespace Arda