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
  // now print the vocabulary by key
  for (const auto &[key, words] : vocabulary) {
    std::cout << "Vocabulary for " << key << ":\n";
    for (const auto &word : words) {
      std::cout << word << "\n";
    }
    std::cout << "\n";
  }
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

  // for (int i = 0; i < 3; i++) {
  //   std::string article;
  //   bool hasVowel = false;
  //   int reqSize = 2 + rand() % 2;
  //   for (int j = 0; j < reqSize; j++) {
  //     std::string letter = "";
  //     if ((!hasVowel && j == reqSize - 1) || rand() % reqSize == 0) {
  //       letter = getRandomLetter(vowels, cumulativeVowelWeights, vowelDis);
  //       hasVowel = true;
  //     } else {
  //       letter = getRandomLetter(consonants, cumulativeConsonantWeights,
  //                                consonantDis);
  //     }
  //     article += letter;
  //   }
  //   if (!hasVowel) {
  //     article[rand() % article.size()] =
  //         getRandomLetter(vowels, cumulativeVowelWeights, vowelDis)[0];
  //   }
  //   articles.push_back(article);
  // }
  //// generate adjective endings
  // for (int i = 0; i < 3; i++) {
  //   std::string adjectiveEnding;
  //   bool hasConsonant = false;
  //   int reqSize = 1 + rand() % 3;
  //   for (int j = 0; j < reqSize; j++) {
  //     std::string letter = "";
  //     if ((!hasConsonant && j == reqSize - 1) || rand() % reqSize == 0) {
  //       letter = getRandomLetter(consonants, cumulativeConsonantWeights,
  //                                consonantDis);
  //       hasConsonant = true;
  //     } else {
  //       letter = getRandomLetter(vowels, cumulativeVowelWeights, vowelDis);
  //     }
  //     adjectiveEnding += letter;
  //   }
  //   if (!hasConsonant) {
  //     adjectiveEnding[rand() % adjectiveEnding.size()] =
  //         getRandomLetter(vowels, cumulativeVowelWeights, vowelDis)[0];
  //   }
  //   adjectiveEndings.push_back(adjectiveEnding);
  // }

  for (int i = 0; i < 2; i++) {
    citySuffixes.push_back(getRandomWordFromVocabulary("CitySuffix"));
  }
  for (int i = 0; i < 2; i++) {
    cityPrefixes.push_back(getRandomWordFromVocabulary("CityPrefix"));
  }
  bool articlesUsed = false;
  //if (rand() % 3 == 0) {
  //  articlesUsed = true;
  //}
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
  port = generateWord();

  for (int i = 0; i < 100; i++) {
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
    auto coreName = generateGenericWord();
    coreName[0] = toupper(coreName[0]);
    cityName += coreName;
    if (rand() % 3 == 0) {
      auto suffix = Fwg::Utils::selectRandom(citySuffixes);
      if (rand() % 3 == 0) {
        cityName += " ";
        suffix[0] = toupper(suffix[0]);
      }
      cityName += suffix;
    }
    cityNames.push_back(cityName);
  }

  std::set<std::string> usedMaleNames;
  for (int i = 0; i < 100; i++) {
    std::string firstName = getRandomWordFromVocabulary("MaleNames");
    if (usedMaleNames.find(firstName) == usedMaleNames.end()) {
      maleNames.push_back(firstName);
      usedMaleNames.insert(firstName);
    }
  }
  std::set<std::string> usedFemaleNames;
  for (int i = 0; i < 100; i++) {
    std::string firstName = getRandomWordFromVocabulary("FemaleNames");
    if (usedFemaleNames.find(firstName) == usedFemaleNames.end() &&
        usedMaleNames.find(firstName) == usedMaleNames.end()) {
      femaleNames.push_back(firstName);
      usedFemaleNames.insert(firstName);
    }
  }
  std::set<std::string> usedLastNames;
  for (int i = 0; i < 100; i++) {
    std::string lastName = generateGenericCapitalizedWord();
    if (usedLastNames.find(lastName) == usedLastNames.end() &&
        usedMaleNames.find(lastName) == usedMaleNames.end() &&
        usedFemaleNames.find(lastName) == usedFemaleNames.end()) {
      surnames.push_back(lastName);
      usedLastNames.insert(lastName);
    }
  }
  std::set<std::string> usedNames;
  for (int i = 0; i < 100; i++) {
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

std::string Language::getRandomWordFromVocabulary(const std::string &category) {
  return vocabulary.at(category)[rand() % vocabulary.at(category).size()];
}

std::string Language::generateWord() {
  return getRandomWordFromVocabulary("GenericWords");
}

std::string Language::generateGenericWord() {
  return getRandomWordFromVocabulary("GenericWords");
}

std::string Language::generateGenericCapitalizedWord() {
  auto word = getRandomWordFromVocabulary("GenericWords");
  word[0] = toupper(word[0]);
  // tolower for the rest
  for (size_t i = 1; i < word.size(); ++i) {
    word[i] = tolower(word[i]);
  }
  return word;
}
std::string Arda::Language::getAdjectiveForm(const std::string &word) {
  return word/* + Fwg::Utils::selectRandom(adjectiveEndings)*/;
}
std::string Arda::Language::generateAreaName(const std::string &trait) {
  return generateGenericCapitalizedWord();
}
} // namespace Arda