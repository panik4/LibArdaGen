#include "parsing/ArdaParserUtils.h"

namespace Arda::Parsing {

inline void trim(std::string &s) {
  s.erase(0, s.find_first_not_of(" \t\r\n"));
  s.erase(s.find_last_not_of(" \t\r\n") + 1);
}
std::vector<int> getNumbers(const std::string &content, const char delimiter,
                            const std::set<int> tokensToConvert) {
  bool convertAll = tokensToConvert.empty();
  std::vector<int> numbers;
  std::stringstream sstream(content);
  std::string token;
  int index = 0;

  while (std::getline(sstream, token, delimiter)) {
    trim(token); // remove whitespace for Linux / cross-platform

    if (!token.empty() && (convertAll || tokensToConvert.count(index))) {
      try {
        numbers.push_back(std::stoi(token));
      } catch (const std::exception &e) {
        std::cerr << "Failed to convert token '" << token
                  << "' to int: " << e.what() << std::endl;
      }
    }

    index++; // increment **every token**, even empty
  }

  return numbers;
}


}; // namespace Arda::Parsing
