#pragma once
#include "LanguageGroup.h"
#include "language/Dataset.h"

#include <map>
#include <string>
#include <vector>
namespace Arda {
class LanguageGenerator {

public:
  LanguageGenerator(const std::string &path);

  void loadDatasets(const std::vector<std::string> &filenames);
  Dataset getRandomMergedDataset(int seed);
  Dataset mergeDatasets(const std::vector<Dataset> &datasets);
  Dataset reduceDataset(const std::vector<Dataset> &dataset);
  LanguageGroup
  generateLanguageGroup(int languageAmount,
                        const std::vector<std::string> &datasetsToUse,
                        int seed);

  std::map<std::string, Dataset> datasetsByLanguage;
};
} // namespace Arda
