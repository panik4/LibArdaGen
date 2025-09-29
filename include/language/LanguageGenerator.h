#pragma once
#include "LanguageGroup.h"
#include "language/Dataset.h"
#include "language/MarkovNameGenerator.h"

#include <iostream>
#include <map>
namespace Arda {
class LanguageGenerator {
public:
  LanguageGenerator(const std::string &path);

  void loadDatasets(const std::vector<std::string> &filenames);
  Dataset getRandomMergedDataset();
  Dataset mergeDatasets(const std::vector<Dataset> &datasets);
  Dataset reduceDataset(const std::vector<Dataset> &dataset);
  LanguageGroup
  generateLanguageGroup(int languageAmount,
                        const std::vector<std::string> &datasetsToUse);

  std::map<std::string, Dataset> datasetsByLanguage;

};
} // namespace Arda