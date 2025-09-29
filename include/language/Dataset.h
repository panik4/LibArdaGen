#pragma once
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
namespace Arda {

struct Dataset {
  std::vector<std::string> influences;
  std::unordered_map<std::string, std::vector<std::string>> vocabulary;

  bool loadFromFile(const std::string &filename) {
    std::ifstream in(filename);
    if (!in)
      return false;

    std::string line;
    while (std::getline(in, line)) {
      if (line.empty())
        continue;

      auto delimPos = line.find(';');
      if (delimPos == std::string::npos)
        continue;

      std::string key = trim(line.substr(0, delimPos));
      std::string values = line.substr(delimPos + 1);
      std::vector<std::string> tokens = splitAndTrim(values, ',');

      vocabulary[key] = tokens;
    }
    return true;
  }

private:
  // Utility: split string by delimiter and trim each element
  static std::vector<std::string> splitAndTrim(const std::string &input,
                                               char delimiter) {
    std::vector<std::string> result;
    std::stringstream ss(input);
    std::string token;

    while (std::getline(ss, token, delimiter)) {
      result.push_back(trim(token));
    }
    return result;
  }

  // Utility: remove leading/trailing whitespace
  static std::string trim(const std::string &str) {
    const char *ws = " \t\n\r";
    size_t start = str.find_first_not_of(ws);
    size_t end = str.find_last_not_of(ws);
    if (start == std::string::npos || end == std::string::npos)
      return "";
    return str.substr(start, end - start + 1);
  }
};

} // namespace Arda