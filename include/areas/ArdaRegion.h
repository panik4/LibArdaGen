#pragma once
#include "areas/ArdaProvince.h"
#include "areas/Region.h"
#include "culture/Culture.h"
#include "culture/Religion.h"
#include "utils/ArdaUtils.h"
#include <map>
namespace Arda {
class Country;
enum class LocatorType { NONE, CITY, FARM, MINE, PORT, WOOD };
class ArdaRegion : public Fwg::Areas::Region {
  std::vector<std::string> cores;

public:
  // member variables
  std::shared_ptr<Country> owner;
  int superRegionID = -1;
  // calculate this for the scenario in general
  double worldPopulationShare = 0.0;
  double averageDevelopment = 0.0;
  double importanceScore = 0.0;
  double relativeImportance = 0.0;
  // calculate this in every module
  double totalPopulation = 0;
  // this calculated in every module, usually by taking both population and
  // development into account
  double gdp = 0.0;
  double worldEconomicActivityShare = 0.0;
  std::map<std::string, Arda::Utils::Resource> resources;

  // other
  bool assigned = false;
  double snowChance, lightRainChance, heavyRainChance, blizzardChance,
      mudChance, sandStormChance;
  // containers
  std::vector<std::shared_ptr<Arda::ArdaProvince>> ardaProvinces;
  std::vector<double> temperatureRange;
  std::vector<double> dateRange;
  std::map<std::shared_ptr<Arda::Religion>, double> religions;
  // the sum here should ALWAYS be 1
  std::map<std::shared_ptr<Arda::Culture>, double> cultures;
  std::vector<std::shared_ptr<Fwg::Civilization::Location>>
      significantLocations;

  ArdaRegion();
  ArdaRegion(const std::shared_ptr<Fwg::Areas::Region> &baseRegion);
  virtual ~ArdaRegion();

  // member functions

  void findLocator(Fwg::Civilization::LocationType locationType, int maxAmount);

  void findPortLocator(int maxAmount = 1);
  void findCityLocator(int maxAmount = 1);
  void findMineLocator(int maxAmount = 1);
  void findFarmLocator(int maxAmount = 1);
  void findWoodLocator(int maxAmount = 1);
  void findWaterLocator(int maxAmount = 1);

  void findWaterPortLocator(int maxAmount = 1);

  std::shared_ptr<Fwg::Civilization::Location>
  getLocation(Fwg::Civilization::LocationType type);
  std::shared_ptr<Arda::Culture> getPrimaryCulture();
  std::string exportLine() const;
};
} // namespace Arda