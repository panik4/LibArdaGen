#pragma once
#include "FastWorldGenerator.h"
#include "RandNum.h"
#include "areas/ArdaRegion.h"
#include "characters/Character.h"
#include "culture/Culture.h"
#include "flags/Flag.h"
#include "utils/ArdaUtils.h"
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
class Country : public Fwg::Areas::Area {

public:
  // member variables
  std::string tag;
  std::string name;
  std::string adjective;
  int capitalRegionID = 0;
  int capitalProvinceID = 0;
  double technologyLevel = 0.0;
  double gdp = 0.0;
  bool landlocked = true;
  Arda::Utils::Ideology ideology = Arda::Utils::Ideology::NONE;

  Rank rank = Rank::Unranked;
  // total importance
  double importanceScore;
  // relative importance to most important country
  double relativeScore;

  // the cultures and their population
  std::map<std::shared_ptr<Culture>, double> cultures;
  Gfx::Flag flag;
  Fwg::Gfx::Colour colour;
  std::vector<Arda::Character> characters;

  // military
  double navalFocus;
  double airFocus;
  double landFocus;

  // constructors/destructors
  Country();
  Country(std::string tag, int ID, std::string name, std::string adjective,
          Gfx::Flag flag);
  virtual ~Country() = default;
  // containers
  std::vector<std::shared_ptr<ArdaRegion>> ownedRegions;
  std::vector<std::shared_ptr<Arda::ArdaProvince>> ownedProvinces;

  std::set<std::shared_ptr<Country>> neighbourCountries;
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
  int getTotalPopulation() const;
  void evaluateTechnologyLevel();
  void evaluateProperties();

  void gatherCultureShares();
  mutable std::vector<int> pixelCache;

  std::span<const int> getNonOwningPixelView() const override {
    pixelCache.clear();
    for (const auto region : ownedRegions) {
      auto p = region->getNonOwningPixelView();
      pixelCache.insert(pixelCache.end(), p.begin(), p.end());
    }
    return pixelCache;
  }
  virtual std::shared_ptr<Culture> getPrimaryCulture() const;

  virtual std::string exportLine() const;
};
} // namespace Arda