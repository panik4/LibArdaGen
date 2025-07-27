#pragma once
#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include <iostream>
#include "RandNum.h"
#include "utils/Utils.h"

namespace Arda::Names {
struct NameData {
  // containers
  std::set<std::string> tags;
  std::map<std::string, std::vector<std::string>> ideologyNames;
  std::map<std::string, std::vector<std::string>> factionNames;
  std::set<std::string> disallowedTokens;
};
std::string generateTag(const std::string name, std::set<std::string>& disallowedTokens);



} // namespace Arda