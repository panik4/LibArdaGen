#pragma once
#include "CivilizationGeneration.h"
#include "countries/Country.h"
#include "FastWorldGenerator.h"
#include "flags/Flag.h"
#include "areas/GameProvince.h"
#include "areas/GameRegion.h"
#include "RandNum.h"
#include "areas/ScenarioContinent.h"
#include "utils/ArdaUtils.h"
#include "areas/SuperRegion.h"
#include <map>
namespace Scenario {

class Generator : public Fwg::FastWorldGenerator {

protected:
  Fwg::Gfx::Bitmap typeMap;

public:
  int numCountries;
  std::string countryMappingPath = "";
  std::string regionMappingPath = "";
  // vars - track civil statistics
  long long worldPop = 0;
  double worldEconomicActivity = 0;
  // vars - config options
  double worldPopulationFactor = 1.0;
  double worldIndustryFactor = 1.0;
  double resourceFactor = 1.0;
  float strategicRegionFactor = 1.0;
  // containers - used for every game
  std::vector<ScenarioContinent> scenContinents;
  std::vector<std::shared_ptr<Region>> gameRegions;
  std::vector<std::shared_ptr<GameProvince>> gameProvinces;
  std::set<std::string> tags;
  Fwg::Utils::ColourTMap<std::string> countryColourMap;
  std::map<std::string, std::shared_ptr<Country>> countries;
  std::map<Rank, std::vector<std::shared_ptr<Country>>> countriesByRank;
  Fwg::Gfx::Bitmap countryMap;
  Fwg::Gfx::Bitmap stratRegionMap;
  std::map<int, std::vector<std::shared_ptr<Country>>> countryImportanceScores;
  Civilization::CivilizationData civData;
  // constructors/destructors
  Generator();
  Generator(const std::string &configSubFolder);
  Generator(Fwg::FastWorldGenerator &fwg);
  ~Generator();
  /* member functions*/
  // print a map showing all countries for debug purposes
  Fwg::Gfx::Bitmap visualiseCountries(Fwg::Gfx::Bitmap &countryBmp,
                                      const int ID = -1);
  // specific preparations. Used by each game, BUT to create game scenario
  void loadRequiredResources(const std::string &gamePath);

  // map base continents to generic paradox compatible game continents
  void mapContinents();
  // map base regions to generic paradox compatible game regions
  virtual void mapRegions();
  // apply values read from a file to override generated data
  void applyRegionInput();
  void applyCountryInput();
  // map base provinces to generic game regions
  void mapProvinces();

  virtual void cutFromFiles(const std::string &gamePath);
  // initialize states
  virtual void initializeStates();
  // initialize countries
  virtual void mapCountries();
  // mapping terrain types of FastWorldGen to module compatible terrains, only
  // implemented for some modules
  virtual Fwg::Gfx::Bitmap mapTerrain();
  // GameRegions are used for every single game,
  std::shared_ptr<Region> &findStartRegion();


  // and countries are always created the same way
  template <typename T> void generateCountries() {
    countries.clear();
    countryMap.clear();
    for (auto &region : gameRegions) {
      region->assigned = false;
      region->owner = nullptr;
    }
    auto &config = Fwg::Cfg::Values();
    Fwg::Utils::Logging::logLine("Generating Countries");

    for (auto i = 0; i < numCountries; i++) {

      T country(std::to_string(i), i, "DUMMY", "", Gfx::Flag(82, 52));

      countries.emplace(country.tag, std::make_shared<T>(country));
    }
    distributeCountries();
  }

  void distributeCountries();
  // see which country neighbours which
  void evaluateCountryNeighbours();
  virtual void generateCountrySpecifics();
  void totalResourceVal(const std::vector<float> &resPrev,
                        float resourceModifier,
                        const Scenario::Utils::ResConfig &resourceConfig);
  // calculate how strong each country is
  virtual void evaluateCountries();
  virtual void printStatistics();

  virtual void writeTextFiles();
  virtual void writeLocalisation();
  virtual void writeImages();

}; // namespace Scenario
} // namespace Scenario
