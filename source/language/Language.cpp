#include "language/Language.h"
namespace Arda {
void Language::train(int seed) {
  markovGeneratorsByVocabulary.clear();
  resetGeneratedWords();

  for (const auto &[vocabKey, words] : reducedDataset.vocabulary) {
    if (words.empty())
      continue;

    MarkovNameGenerator generator(
        /*order=*/2, seed);
    generator.train(words);
    markovGeneratorsByVocabulary[vocabKey] = std::move(generator);
  }
}

std::string Language::generateWord(const std::string &category) {
  const auto generator = markovGeneratorsByVocabulary.find(category);
  if (generator == markovGeneratorsByVocabulary.end())
    return {};

  std::string duplicateFallback;
  for (int attempt = 0; attempt < 100; ++attempt) {
    auto word = generator->second.generate(4, 12);
    if (word.empty())
      continue;

    if (generatedWords.insert(lowercaseWord(word)).second)
      return word;
    duplicateFallback = std::move(word);
  }

  return duplicateFallback;
}

void Language::resetGeneratedWords() {
  generatedWords.clear();
}

std::string Language::capitaliseName(const std::string &word) {
  // simple rule: First letter capitalised, anything after a dash or space
  // capitalised, rest lowercase
  if (word.empty())
    return word;
  std::string capitalised = word;
  capitalised[0] = toupper(capitalised[0]);
  for (size_t i = 1; i < capitalised.size(); ++i) {
    if (capitalised[i - 1] == '-' || capitalised[i - 1] == ' ') {
      capitalised[i] = toupper(capitalised[i]);
    } else {
      capitalised[i] = tolower(capitalised[i]);
    }
  }

  return capitalised;
}

std::string Language::capitalisedWord(const std::string &word) {
  if (word.empty())
    return word;
  std::string capitalised = word;
  capitalised[0] = toupper(capitalised[0]);
  for (size_t i = 1; i < capitalised.size(); ++i) {
    capitalised[i] = tolower(capitalised[i]);
  }
  return capitalised;
}

std::string Language::lowercaseWord(const std::string &word) {
  if (word.empty())
    return word;
  std::string lowercased = word;
  for (size_t i = 0; i < lowercased.size(); ++i) {
    lowercased[i] = tolower(lowercased[i]);
  }
  return lowercased;
}

void Language::fillAllLists() {
  articles.clear();
  adjectiveEndings.clear();
  citySuffixes.clear();
  cityPrefixes.clear();
  cityNames.clear();
  maleNames.clear();
  femaleNames.clear();
  surnames.clear();
  names.clear();
  shipNames.clear();
  airplaneNames.clear();

  name = generateGenericCapitalizedWord();
  port = generateGenericCapitalizedWord();

  for (int i = 0; i < 10; ++i) {
    cityNames.push_back(generateGenericCapitalizedWord());
    maleNames.push_back(getRandomCapitalisedWordFromVocabulary("MaleNames"));
    femaleNames.push_back(
        getRandomCapitalisedWordFromVocabulary("FemaleNames"));
    surnames.push_back(generateGenericCapitalizedWord());
    names.push_back(generateGenericCapitalizedWord());
  }
}

std::string
Language::getRandomCapitalisedWordFromVocabulary(const std::string &category) {
  return capitalisedWord(generateWord(category));
}

std::string
Language::getRandomLowercaseWordFromVocabulary(const std::string &category) {
  return lowercaseWord(generateWord(category));
}

std::string Language::getRandomWordFromVocabulary(const std::string &category) {
  return generateWord(category);
}

std::string Language::generateGenericWord() {
  return getRandomWordFromVocabulary("GenericWords");
}
std::string Language::generateGenericLowercaseWord() {
  auto word = getRandomWordFromVocabulary("GenericWords");
  return lowercaseWord(word);
}
std::string Language::generateGenericCapitalizedWord() {
  auto word = getRandomWordFromVocabulary("GenericWords");
  return capitalisedWord(word);
}
std::string Arda::Language::getAdjectiveForm(const std::string &word) {
  return word /* + Fwg::Utils::Random::selectRandom(adjectiveEndings)*/;
}
std::string Arda::Language::generateAreaName(const std::string &trait) {
  auto name = generateGenericCapitalizedWord();
  if (trait == "sea") {
    name += " Sea";
  }
  return name;
}
} // namespace Arda