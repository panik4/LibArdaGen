#pragma once
#include "RandNum.h"
#include "utils/ArdaUtils.h"
#include "utils/Utils.h"
#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace Arda::Names {
struct NameData {
  // containers
  std::set<std::string> tags;
  std::map<Arda::Utils::Ideology, std::vector<std::string>> ideologyNames;
  std::map<Arda::Utils::Ideology, std::vector<std::string>> factionNames;
  std::set<std::string> disallowedTokens;
};
std::string generateTag(const std::string name,
                        std::set<std::string> &disallowedTokens);

} // namespace Arda::Names