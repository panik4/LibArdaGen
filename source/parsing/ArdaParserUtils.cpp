#include "parsing/ArdaParserUtils.h"

namespace Arda::Parsing {
std::vector<int> getNumbers(const std::string &content, const char delimiter,
                            const std::set<int> tokensToConvert) {
  bool convertAll = !tokensToConvert.size();
  std::vector<int> numbers{};
  std::stringstream sstream(content);
  std::string token;
  int counter = 0;
  while (std::getline(sstream, token, delimiter)) {
    if (token.size())
      if (convertAll || tokensToConvert.find(counter) != tokensToConvert.end())
        numbers.push_back(stoi(token));
    counter++;
  }
  return numbers;
};
}; // namespace Arda::Parsing
