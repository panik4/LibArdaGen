#include "language/LanguageGenerator.h"
namespace Arda {
LanguageGenerator::LanguageGenerator(const std::string &baseDir) {

  std::vector<std::string> paths;
  for (const auto &entry : std::filesystem::directory_iterator(baseDir)) {
    if (entry.is_directory()) {
    }
    paths.push_back(entry.path().string());
  }

  // Load datasets from the selected paths
  loadDatasets(paths);
  // load all datasets from the folder using directory_iterator
}
void LanguageGenerator::loadDatasets(
    const std::vector<std::string> &filenames) {
  datasetsByLanguage.clear();
  for (const auto &filename : filenames) {
    Dataset dataset;
    if (dataset.loadFromFile(filename)) {
      // Use the filename as the key, stripping the path and extension
      std::string key = filename.substr(filename.find_last_of("/\\") + 1);
      key = key.substr(0, key.find_last_of('.'));
      dataset.influences.push_back(key);
      datasetsByLanguage[key] = std::move(dataset);
    } else {
      std::cerr << "Failed to load dataset from " << filename << std::endl;
    }
  }
}

Dataset LanguageGenerator::getRandomMergedDataset() {
  // Convert map to vector of pairs to access keys and values
  std::vector<std::pair<std::string, Dataset>> availableDatasets(
      datasetsByLanguage.begin(), datasetsByLanguage.end());

  if (availableDatasets.empty())
    return Dataset();

  // Parameters for random selection count
  size_t minGroups = 2;
  size_t maxGroups = 3;
  size_t selectionCount = 1;
  /* std::min(availableDatasets.size(),
               minGroups + rand() % (maxGroups - minGroups + 1));*/

  // Shuffle available datasets
  std::random_device rd;
  std::mt19937 g(rd());
  std::shuffle(availableDatasets.begin(), availableDatasets.end(), g);

  // Collect selected datasets
  std::vector<Dataset> selected;
  for (size_t i = 0; i < selectionCount; ++i) {
    selected.push_back(availableDatasets[i].second);
  }

  // Merge and return
  return mergeDatasets(selected);
}

Dataset LanguageGenerator::mergeDatasets(const std::vector<Dataset> &datasets) {
  {
    Dataset result;

    for (const auto &ds : datasets) {
      result.influences.insert(result.influences.end(), ds.influences.begin(),
                               ds.influences.end());
      for (const auto &[key, words] : ds.vocabulary) {
        auto &outVec = result.vocabulary[key];
        outVec.insert(outVec.end(), words.begin(), words.end());
      }
    }

    return result;
  }
}

Dataset LanguageGenerator::reduceDataset(const std::vector<Dataset> &dataset) {
  return Dataset();
}

LanguageGroup LanguageGenerator::generateLanguageGroup(
    int languageAmount, const std::vector<std::string> &datasetsToUse) {

  Arda::Dataset mergedDataset;
  if (datasetsToUse.empty()) {
    mergedDataset = getRandomMergedDataset();
  } else {
    // Convert map to vector of pairs to access keys and values
    std::vector<std::pair<std::string, Dataset>> availableDatasets(
        datasetsByLanguage.begin(), datasetsByLanguage.end());
    std::vector<Dataset> selected;
    for (const auto &name : datasetsToUse) {
      auto it = datasetsByLanguage.find(name);
      if (it != datasetsByLanguage.end()) {
        selected.push_back(it->second);
      } else {
        std::cerr << "Dataset " << name << " not found." << std::endl;
      }
    }
    mergedDataset = mergeDatasets(selected);
  }
  if (mergedDataset.vocabulary.empty()) {
    std::cerr << "No vocabulary loaded, cannot generate languages."
              << std::endl;
    return LanguageGroup();
  }
  LanguageGroup languageGroup;
  languageGroup.generate(languageAmount, mergedDataset);

  return languageGroup;
}

} // namespace Arda