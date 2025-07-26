#pragma once
#include "FastWorldGenerator.h"
#include "RandNum.h"
#include "areas/ArdaRegion.h"
#include "characters/Character.h"
#include "culture/Culture.h"
#include "flags/Flag.h"
#include <string>
#include <vector>
namespace Arda {

enum class Rank {
  GreatPower,
  SecondaryPower,
  RegionalPower,
  LocalPower,
  MinorPower,
  Unranked
};
class Country {

public:
  // member variables
  const int ID;
  std::string tag;
  std::string name;
  std::string adjective;
  int capitalRegionID = 0;
  int capitalProvinceID;
  double populationFactor;
  double averageDevelopment;
  double worldPopulationShare;
  double worldEconomicActivityShare;
  bool landlocked = true;

  Rank rank = Rank::Unranked;
  // total importance
  double importanceScore;
  // relative importance to most important country
  double relativeScore;
  // the gamemodule calculates the total population
  int pop;
  // the cultures and their populationFactor
  std::map<std::shared_ptr<Culture>, double> cultures;
  Gfx::Flag flag;
  Fwg::Gfx::Colour colour;
  std::vector<Arda::Character> characters;
  // constructors/destructors
  Country();
  Country(std::string tag, int ID, std::string name, std::string adjective,
          Gfx::Flag flag);
  virtual ~Country() = default;
  // containers
  std::vector<std::shared_ptr<ArdaRegion>> ownedRegions;
  std::vector<std::shared_ptr<Arda::ArdaProvince>> ownedProvinces;

  std::set<std::shared_ptr<Country>> neighbours;
  // member functions
  void assignRegions(
      int maxRegions, std::vector<std::shared_ptr<ArdaRegion>> &ardaRegions,
      std::shared_ptr<ArdaRegion> startRegion,
      std::vector<std::shared_ptr<Arda::ArdaProvince>> &ardaProvinces);
  void addRegion(std::shared_ptr<ArdaRegion> region);
  void removeRegion(std::shared_ptr<ArdaRegion> region);
  void selectCapital();
  // operators
  bool operator<(const Country &right) const { return ID < right.ID; };
  void evaluateProvinces();
  void evaluatePopulations(const double worldPopulationFactor);
  void evaluateDevelopment();
  void evaluateEconomicActivity(const double worldEconomicActivity);
  void evaluateProperties();

  void gatherCultureShares();
  virtual std::shared_ptr<Culture> getPrimaryCulture() const;
};
} // namespace Arda