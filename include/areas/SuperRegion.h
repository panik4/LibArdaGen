#pragma once
#include "FastWorldGenerator.h"
#include "areas/ArdaRegion.h"
#include <map>
namespace Arda {
struct Cluster : Fwg::Areas::Area {
  std::vector<std::shared_ptr<ArdaRegion>> regions;
};
class SuperRegion : public Fwg::Areas::Area {

public:
  SuperRegion();

  // member variables
  int ID;
  std::string name;
  Fwg::Gfx::Colour colour;
  // containers
  std::vector<std::shared_ptr<ArdaRegion>> ardaRegions;
  std::vector<Cluster> regionClusters;

  bool centerOutsidePixels = false;

  void addRegion(std::shared_ptr<ArdaRegion> region);
  void removeRegion(std::shared_ptr<ArdaRegion> region);
  void setType();
  void checkPosition(const std::vector<SuperRegion> &superRegions);
  std::vector<Cluster>
  getClusters(std::vector<std::shared_ptr<ArdaRegion>> &regions);
};
} // namespace Arda