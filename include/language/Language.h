#pragma once
#include "RandNum.h"
#include "language/LanguageGenerator.h"
#include "utils/Utils.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <set>
#include <string>
#include <vector>
namespace Arda {
// Function to get a random letter based on weights
static std::string
getRandomLetter(const std::vector<std::string> &letters,
                std::vector<float> &cumulativeWeights,
                std::uniform_real_distribution<> &distribution) {
  // Generate a random number in the range [0, total weight)
  std::random_device rd;
  std::mt19937 gen(rd());
  float randomWeight = distribution(gen);

  // Find the letter corresponding to the random weight
  auto it = std::lower_bound(cumulativeWeights.begin(), cumulativeWeights.end(),
                             randomWeight);
  if (it == cumulativeWeights.end()) {
    return ""; // Return an empty string if no match is found
  }
  return letters[std::distance(cumulativeWeights.begin(), it)];
}

class Language {
public:
  Dataset reducedDataset;
  std::map<std::string, MarkovNameGenerator> markovGeneratorsByVocabulary;
  std::unordered_map<std::string, std::vector<std::string>> vocabulary;
  void train();

  void generateVocabulary();

  std::string name;
  std::vector<std::string> articles;         // like the, la, le, der, die, das
  std::vector<std::string> adjectiveEndings; // like -ian, -ese, -ish, -ese,
                                             // -an, -ese, -ic, -ese, -ish, -ese
  std::string
      port; // like Port, Puerto, Porto, Haven, can be either prefix or suffix

  std::vector<std::string> cityPrefixes; // like Bad, New, Saint, San, Los, Las,
                                         // El, La, but randomly generated
  std::vector<std::string> citySuffixes; // like ville, city, town, burg, but
                                         // randomly generated
  std::vector<std::string> cityNames;
  std::vector<std::string> portNames;
  // std::vector<std::string> mountainCityNames;
  // std::vector<std::string> valleyCityNames;
  // std::vector<std::string> riverCityNames; // should follow patterns like

  std::vector<std::string> maleNames;
  std::vector<std::string> femaleNames;
  std::vector<std::string> surnames;
  std::vector<std::string> names;

  std::vector<std::string> shipNames;
  std::vector<std::string> airplaneNames;

  // usually a variation of the language group alphabet
  std::map<std::string, float> alphabet;
  // separate grouping
  std::vector<std::string> consonants;
  std::vector<std::string> vowels;
  // tokens
  std::set<std::string> hardTokens;
  std::set<std::string> softTokens;

  std::set<std::string> startTokens;
  std::set<std::string> middleTokens;
  std::set<std::string> endTokens;
  // rulesets for generating names, with keys to describe where to draw from
  std::vector<std::vector<std::string>> tokenSets;

  // add slight variations to the language, by changing the weights of the
  // letters in the alphabet, and then randomly replace some of the letters in
  // the tokens
  void vary();

  void fillAllLists();

  std::string generateWord(const std::vector<std::string> &tokenSet);

  // method to generate a word, but using only a random tokenset of specified
  // length
  
  std::string getRandomWordFromVocabulary(const std::string&category);
  std::string generateWord();

  std::string generateGenericWord();
  std::string generateGenericCapitalizedWord();
  std::string getAdjectiveForm(const std::string &word);
  std::string generateAreaName(const std::string &trait);
};
} // namespace Arda