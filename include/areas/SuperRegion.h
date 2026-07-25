#pragma once
#include "FastWorldGenerator.h"
#include "areas/ArdaRegion.h"
#include "utils/SerialisationFwd.h"
#include <map>

namespace Arda {
struct Cluster : Fwg::Areas::Area {
  std::vector<std::shared_ptr<ArdaRegion>> regions;

  template<class Archive>
  void serialize(Archive &ar, const unsigned int /*version*/) {
    ar & boost::serialization::base_object<Fwg::Areas::Area>(*this);
    ar & regions;
  }
};
class SuperRegion : public Fwg::Areas::Area {

public:
  SuperRegion();

  // member variables
  std::string name;
  // containers
  std::vector<std::shared_ptr<ArdaRegion>> ardaRegions;
  std::vector<Cluster> regionClusters;
  std::vector<std::shared_ptr<SuperRegion>> neighbourSuperRegions;
  bool centerOutsidePixels = false;

  void addRegion(std::shared_ptr<ArdaRegion> region);
  void removeRegion(std::shared_ptr<ArdaRegion> region);
  void setType();
  virtual bool
  checkPosition(const std::vector<std::shared_ptr<SuperRegion>> &superRegions);
  std::vector<Cluster>
  getClusters(const std::vector<std::shared_ptr<ArdaRegion>> &regions);

  // serialisation
  template<class Archive>
  void serialize(Archive &ar, const unsigned int /*version*/);
};
} // namespace Arda

BOOST_CLASS_EXPORT_KEY(Arda::SuperRegion)

