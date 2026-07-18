#pragma once
#include "FastWorldGenerator.h"
#include "areas/ArdaRegion.h"
#include "utils/Archive.h"
#include <map>

namespace Arda {
struct Cluster : Fwg::Areas::Area {
  std::vector<std::shared_ptr<ArdaRegion>> regions;

  void serialise(Fwg::Utils::Serialisation::Archive &ar) {
    Area::serialise(ar);
    ar.polymorphicPtrVector(regions);
  }
  void deserialise(Fwg::Utils::Serialisation::Archive &ar) {
    serialise(ar);
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
  void serialise(Fwg::Utils::Serialisation::Archive &ar) override;
  void deserialise(Fwg::Utils::Serialisation::Archive &ar) override;
  uint32_t typeTag() const override;
};
} // namespace Arda