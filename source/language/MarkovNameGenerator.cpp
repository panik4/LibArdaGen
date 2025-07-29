#include "language/MarkovNameGenerator.h"

MarkovNameGenerator::MarkovNameGenerator(int order)
    : order(order), rng(std::random_device{}()) {}

void MarkovNameGenerator::train(const std::vector<std::string> &names) {
  chain.clear();
  starters.clear();

  for (const auto &name : names) {
    std::string padded = "^" + name + "$"; // ^ and $ mark start/end
    if (padded.size() > order)
      starters.push_back(padded.substr(1, order));

    for (size_t i = 0; i + order < padded.size(); ++i) {
      std::string key = padded.substr(i, order);
      char nextChar = padded[i + order];
      chain[key].push_back(nextChar);
    }
  }
}

char MarkovNameGenerator::getNextChar(const std::string &key) {
  auto it = chain.find(key);
  if (it == chain.end() || it->second.empty())
    return '$'; // fallback: end of name

  const auto &possibilities = it->second;
  std::uniform_int_distribution<> dist(0, possibilities.size() - 1);
  return possibilities[dist(rng)];
}

std::string MarkovNameGenerator::generate(int minLength, int maxLength) {
  if (starters.empty())
    return "";

  std::uniform_int_distribution<> startDist(0, starters.size() - 1);
  std::string key = starters[startDist(rng)];
  std::string result = key;

  while (true) {
    char next = getNextChar(key);
    if (next == '$' || result.size() >= static_cast<size_t>(maxLength))
      break;

    result += next;
    key = result.substr(result.size() - order, order);
  }

  // Strip start marker and validate length
  result.erase(std::remove(result.begin(), result.end(), '^'), result.end());
  if (result.size() < static_cast<size_t>(minLength))
    return generate(minLength, maxLength); // retry

  return result;
}
