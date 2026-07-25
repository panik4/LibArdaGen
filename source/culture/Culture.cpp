#include "culture/Culture.h"
#include "culture/CultureGroup.h"
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>

namespace Arda {
template<class Archive>
void Culture::serialize(Archive &ar, const unsigned int /*version*/) {
  ar & name & adjective & centerOfCulture & colour & language & cultureGroup & visualType;
}
template void Culture::serialize(boost::archive::binary_oarchive&, unsigned int);
template void Culture::serialize(boost::archive::binary_iarchive&, unsigned int);
} // namespace Arda