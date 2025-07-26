#pragma once
#include "FastWorldGenerator.h"
#include "io/Parsing.h"
#include <regex>
#include <filesystem>
#include <string>

namespace Arda::Parsing {
std::vector<int> getNumbers(const std::string &content, const char delimiter,
                            const std::set<int> tokensToConvert = {});


}; // namespace Rpx::Parsing
