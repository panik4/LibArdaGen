#include "language/LanguageGroup.h"
namespace Arda {

void LanguageGroup::generate(int languageAmount, const Dataset &dataset,
                             int seed) {
  mergedDataset = dataset;
  languages.clear();
  for (int i = 0; i < languageAmount; i++) {
    Language language;
    language.reducedDataset = dataset;
    const auto languageSeed =
        static_cast<unsigned int>(seed) + static_cast<unsigned int>(i);
    language.train(static_cast<int>(languageSeed));
    language.fillAllLists();

    // languageGenerator.reduceDataset(        {mergedDataset});
    languages.push_back(std::make_shared<Language>(language));
  }
  if (languages.size()) {
    name = languages[0]->getRandomCapitalisedWordFromVocabulary("GenericWords");
  } else {
    name = "NoLanguages";
  }

  return;
}
} // namespace Arda
