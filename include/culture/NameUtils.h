#pragma once
#include "RandNum.h"
#include "utils/ArdaUtils.h"
#include "utils/Utils.h"
#include "utils/Archive.h"
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
  std::set<std::string> originalDisallowedTokens;
  std::set<std::string> disallowedTokens;

  void serialise(Fwg::Utils::Serialisation::Archive &ar) {
    ar &tags &originalDisallowedTokens &disallowedTokens;
    // Manual map serialisation for enum-keyed maps
    if (ar.isWriting()) {
      auto writeMap = [&](auto &m) {
        uint64_t sz = m.size();
        ar &sz;
        for (auto &[k, v] : m) {
          ar.serialiseEnum(k);
          ar &v;
        }
      };
      writeMap(ideologyNames);
      writeMap(factionNames);
    } else {
      auto readMap = [&](auto &m) {
        uint64_t sz;
        ar &sz;
        m.clear();
        for (uint64_t i = 0; i < sz; ++i) {
          Arda::Utils::Ideology k;
          std::vector<std::string> v;
          ar.serialiseEnum(k);
          ar &v;
          m.emplace(k, std::move(v));
        }
      };
      readMap(ideologyNames);
      readMap(factionNames);
    }
  }
  void deserialise(Fwg::Utils::Serialisation::Archive &ar) {
    serialise(ar);
  }
};
std::string generateTag(const std::string name,
                        std::set<std::string> &disallowedTokens);

} // namespace Arda::Names