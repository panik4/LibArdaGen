#include "areas/ArdaProvince.h"
namespace Arda {
ArdaProvince::ArdaProvince(std::shared_ptr<Fwg::Areas::Province> province) {}

ArdaProvince::ArdaProvince() {}

ArdaProvince::~ArdaProvince() {}

std::string ArdaProvince::toHexString() {
  std::string hexString = "x";
  std::stringstream stream;
  for (int i = 2; i >= 0; i--)
    stream << std::setfill('0') << std::setw(sizeof(char) * 2) << std::uppercase
           << std::hex << (int)this->colour.getBGR()[i];
  hexString.append(stream.str());
  return hexString;
}
} // namespace Arda