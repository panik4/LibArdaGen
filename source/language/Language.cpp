#include "language/Language.h"
namespace Arda {
void Language::train() {
  markovGeneratorsByVocabulary.clear();

  for (const auto &[vocabKey, words] : reducedDataset.vocabulary) {
    if (words.empty())
      continue;

    MarkovNameGenerator generator(
        /*order=*/2);
    generator.train(words);
    markovGeneratorsByVocabulary[vocabKey] = std::move(generator);
  }
}

void Language::generateVocabulary() {
  vocabulary.clear();

  for (auto &[key, generator] : markovGeneratorsByVocabulary) {
    std::vector<std::string> generatedWords;

    // Determine how many words to generate
    int count = 10;
    if (key == "MaleNames" || key == "FemaleNames" || key == "Surnames") {
      count = 100;
    }

    std::unordered_set<std::string> uniqueWords; // avoid duplicates
    int attempts = 0;
    while (uniqueWords.size() < static_cast<size_t>(count) &&
           attempts++ < 100) {
      std::string word = generator.generate(4, 12);
      if (!word.empty())
        uniqueWords.insert(word);
    }

    // Move into vocabulary
    vocabulary[key] =
        std::vector<std::string>(uniqueWords.begin(), uniqueWords.end());
  }
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

  for (int i = 0; i < 2; i++) {
    citySuffixes.push_back(getRandomLowercaseWordFromVocabulary("CitySuffix"));
  }
  for (int i = 0; i < 2; i++) {
    cityPrefixes.push_back(
        getRandomCapitalisedWordFromVocabulary("CityPrefix"));
  }
  bool articlesUsed = false;
  // if (rand() % 3 == 0) {
  //   articlesUsed = true;
  // }
  std::string prefixSeparator = " ";
  if (rand() % 3 == 0) {
    prefixSeparator = "-";
  }
  for (auto &cityPrefix : cityPrefixes) {
    cityPrefix[0] = toupper(cityPrefix[0]);
  }
  for (auto &article : articles) {
    article[0] = toupper(article[0]);
  }
  port = generateGenericCapitalizedWord();

  std::unordered_set<std::string> usedCoreNames;
  for (int i = 0; i < 4000; i++) {
    std::string cityName;
    if (rand() % 3 == 0) {
      if (articlesUsed && rand() % 2 == 0) {
        cityName += Fwg::Utils::selectRandom(articles);
        cityName += " ";
      } else {
        cityName += Fwg::Utils::selectRandom(cityPrefixes);
        cityName += prefixSeparator;
      }
    }
    auto coreName = generateGenericLowercaseWord();
    while (usedCoreNames.find(coreName) != usedCoreNames.end()) {
      coreName = generateGenericLowercaseWord();
    }
    cityName += coreName;
    if (rand() % 3 == 0) {
      auto suffix = Fwg::Utils::selectRandom(citySuffixes);
      if (rand() % 3 == 0) {
        cityName += " ";
        suffix[0] = toupper(suffix[0]);
      }
      cityName += suffix;
    }
    cityName = capitaliseName(cityName);
    cityNames.push_back(cityName);
  }

  std::unordered_set<std::string> usedMaleNames;
  for (int i = 0; i < 1000; i++) {
    std::string firstName = getRandomCapitalisedWordFromVocabulary("MaleNames");
    if (usedMaleNames.find(firstName) == usedMaleNames.end()) {
      maleNames.push_back(firstName);
      usedMaleNames.insert(firstName);
    }
  }
  std::unordered_set<std::string> usedFemaleNames;
  for (int i = 0; i < 1000; i++) {
    std::string firstName =
        getRandomCapitalisedWordFromVocabulary("FemaleNames");
    if (usedFemaleNames.find(firstName) == usedFemaleNames.end() &&
        usedMaleNames.find(firstName) == usedMaleNames.end()) {
      femaleNames.push_back(firstName);
      usedFemaleNames.insert(firstName);
    }
  }
  std::unordered_set<std::string> usedLastNames;
  for (int i = 0; i < 1000; i++) {
    std::string lastName = generateGenericCapitalizedWord();
    if (usedLastNames.find(lastName) == usedLastNames.end() &&
        usedMaleNames.find(lastName) == usedMaleNames.end() &&
        usedFemaleNames.find(lastName) == usedFemaleNames.end()) {
      surnames.push_back(lastName);
      usedLastNames.insert(lastName);
    }
  }
  std::unordered_set<std::string> usedNames;
  for (int i = 0; i < 1000; i++) {
    std::string shipName = generateGenericCapitalizedWord();
    if (usedNames.find(shipName) == usedNames.end()) {
      shipNames.push_back(shipName);
      usedNames.insert(shipName);
    }
    std::string airplaneName = generateGenericCapitalizedWord();
    if (usedNames.find(airplaneName) == usedNames.end()) {
      airplaneNames.push_back(airplaneName);
      usedNames.insert(airplaneName);
    }
  }
}

std::string
Language::getRandomCapitalisedWordFromVocabulary(const std::string &category) {
  return capitalisedWord(
      vocabulary.at(category)[rand() % vocabulary.at(category).size()]);
}

std::string
Language::getRandomLowercaseWordFromVocabulary(const std::string &category) {
  return capitalisedWord(
      vocabulary.at(category)[rand() % vocabulary.at(category).size()]);
}

std::string Language::getRandomWordFromVocabulary(const std::string &category) {
  return vocabulary.at(category)[rand() % vocabulary.at(category).size()];
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
  return word /* + Fwg::Utils::selectRandom(adjectiveEndings)*/;
}
std::string Arda::Language::generateAreaName(const std::string &trait) {
  auto name = generateGenericCapitalizedWord();
  if (trait == "sea") {
    name += " Sea";
  }
  return name;
}
} // namespace Arda