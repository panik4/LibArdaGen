
#include "language/LanguageGroup.h"
namespace Arda {

void LanguageGroup::generate(int languageAmount, std::string baseDir) {
  LanguageGenerator languageGenerator;

  std::vector<std::string> paths;
  for (const auto &entry : std::filesystem::directory_iterator(baseDir)) {
    if (entry.is_directory()) {
    }
    paths.push_back(entry.path().string());
  }

  // Load datasets from the selected paths
  languageGenerator.loadDatasets(paths);
  // load all datasets from the folder using directory_iterator

  mergedDataset = languageGenerator.getRandomMergedDataset();
  if (mergedDataset.vocabulary.empty()) {
    std::cerr << "No vocabulary loaded, cannot generate languages."
              << std::endl;
    return;
  }
  for (int i = 0; i < languageAmount; i++) {
    Language language;
    language.reducedDataset = mergedDataset;
    language.train();
    language.generateVocabulary();

    // languageGenerator.reduceDataset(        {mergedDataset});
    languages.push_back(std::make_shared<Language>(language));
  }

  return;
}
} // namespace Arda