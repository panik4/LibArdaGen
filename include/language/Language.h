#pragma once
#include "language/LanguageGenerator.h"
#include "utils/Utils.h"
#include <string>
#include <unordered_set>
#include <vector>
namespace Arda {

enum class AreaNameType {
  Ocean,
  Sea,
  Lake,
  Plains,
  Valley,
};

class Language {
public:
  Dataset reducedDataset;
  std::map<std::string, MarkovNameGenerator> markovGeneratorsByVocabulary;
  std::unordered_map<std::string, std::vector<std::string>> vocabulary;
  void train();

  void generateVocabulary();
  static std::string capitaliseName(const std::string &word);
  static std::string capitalisedWord(const std::string &word);
  static std::string lowercaseWord(const std::string &word);
  std::string name;
  std::vector<std::string> articles;         // like the, la, le, der, die, das
  std::vector<std::string> adjectiveEndings; // like -ian, -ese, -ish, -ese,
                                             // -an, -ese, -ic, -ese, -ish, -ese
  std::string port;

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

  void fillAllLists();

  std::string
  getRandomCapitalisedWordFromVocabulary(const std::string &category);

  std::string getRandomLowercaseWordFromVocabulary(const std::string &category);

  std::string generateWord(const std::vector<std::string> &tokenSet);
  std::string getRandomWordFromVocabulary(const std::string &category);
  std::string generateGenericWord();
  std::string generateGenericLowercaseWord();
  std::string generateGenericCapitalizedWord();
  std::string getAdjectiveForm(const std::string &word);
  std::string generateAreaName(const std::string &trait);
};
} // namespace Arda