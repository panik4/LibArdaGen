#include "areas/ArdaRegion.h"
namespace Arda {
ArdaRegion::ArdaRegion() {}

ArdaRegion::ArdaRegion(const Fwg::Areas::Region &baseRegion)
    : Fwg::Areas::Region(baseRegion), assigned(false), totalPopulation{-1} {}

ArdaRegion::~ArdaRegion() {}
void ArdaRegion::sumPopulations() {}
void ArdaRegion::findLocator(Fwg::Civilization::LocationType locationType,
                         int maxAmount) {
  std::shared_ptr<Fwg::Civilization::Location> addLocation;
  int maxPixels = 0;
  for (auto &location : locations) {
    if (location->type == locationType) {
      if (location->pixels.size() > maxPixels) {
        // locator.xPos = location->position.widthCenter;
        // locator.yPos = location->position.heightCenter;
        maxPixels = location->pixels.size();
        addLocation = location;
      }
    }
  }
  // add to significant locations
  if (maxPixels > 0) {
    significantLocations.push_back(addLocation);
  }
}

void ArdaRegion::findPortLocator(int maxAmount) {
  findLocator(Fwg::Civilization::LocationType::Port, maxAmount);
}

void ArdaRegion::findCityLocator(int maxAmount) {
  findLocator(Fwg::Civilization::LocationType::City, maxAmount);
}
void ArdaRegion::findMineLocator(int maxAmount) {
  findLocator(Fwg::Civilization::LocationType::Mine, maxAmount);
}

void ArdaRegion::findFarmLocator(int maxAmount) {
  findLocator(Fwg::Civilization::LocationType::Farm, maxAmount);
}

void ArdaRegion::findWoodLocator(int maxAmount) {
  findLocator(Fwg::Civilization::LocationType::Forest, maxAmount);
}
void ArdaRegion::findWaterLocator(int maxAmount) {
  findLocator(Fwg::Civilization::LocationType::WaterNode, maxAmount);
}
void ArdaRegion::findWaterPortLocator(int maxAmount) {
  findLocator(Fwg::Civilization::LocationType::WaterPort, maxAmount);
}
std::shared_ptr<Fwg::Civilization::Location>
ArdaRegion::getLocation(Fwg::Civilization::LocationType type) {
  for (auto &location : locations) {
    if (location->type == type) {
      return location;
    }
  }
  return nullptr;
}
std::shared_ptr<Arda::Culture> Arda::ArdaRegion::getPrimaryCulture() {
  if (cultureShares.empty()) {
    return nullptr;
  }

  auto primaryCulture = std::max_element(
      cultureShares.begin(), cultureShares.end(),
      [](const auto &a, const auto &b) { return a.second < b.second; });

  return primaryCulture->first;
}
} // namespace Arda