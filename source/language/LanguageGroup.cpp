#include "language/LanguageGroup.h"
namespace Arda {

void LanguageGroup::generate(int languageAmount, const Dataset &dataset, int seed) {
  mergedDataset = dataset;
  languages.clear();
  for (int i = 0; i < languageAmount; i++) {
    Language language;
    language.reducedDataset = dataset;
    language.train(seed);
    language.generateVocabulary();
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