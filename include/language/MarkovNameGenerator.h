#pragma once
#include <map>
#include <random>
#include <string>
#include <vector>

class MarkovNameGenerator {
public:
  MarkovNameGenerator(int order = 2);
  void train(const std::vector<std::string> &names);
  std::string generate(int minLength = 4, int maxLength = 10);

private:
  int order;
  std::map<std::string, std::vector<char>> chain;
  std::vector<std::string> starters;
  std::mt19937 rng;

  char getNextChar(const std::string &key);
};

