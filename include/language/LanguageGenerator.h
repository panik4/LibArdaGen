#pragma once
#include "language/Dataset.h"
#include "language/MarkovNameGenerator.h"
#include <iostream>
#include <map>
namespace Arda {
class LanguageGenerator {
public:
  void loadDatasets(const std::vector<std::string> &filenames);
  Dataset getRandomMergedDataset();
  Dataset mergeDatasets(const std::vector<Dataset> &datasets);
  Dataset reduceDataset(const std::vector<Dataset> &dataset);


private:
  std::map<std::string, Dataset> datasetsByLanguage;

};
} // namespace Arda