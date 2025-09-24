#include "utils/ArdaUtils.h"
namespace Arda::Utils {
Coordinate strToPos(const std::vector<std::string> &tokens,
                    const std::vector<int> positions) {
  Coordinate p;
  p.x = std::stoi(tokens[positions[0]]);
  p.y = std::stoi(tokens[positions[1]]);
  p.z = std::stoi(tokens[positions[2]]);
  p.rotation = std::stoi(tokens[positions[3]]);
  return p;
}
} // namespace Arda::Utils
