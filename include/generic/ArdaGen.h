#pragma once
#include "FastWorldGenerator.h"
#include "RandNum.h"
#include "areas/ArdaContinent.h"
#include "areas/ArdaProvince.h"
#include "areas/ArdaRegion.h"
#include "areas/SuperRegion.h"
#include "countries/Country.h"
#include "flags/Flag.h"
#include "civilisation/CivilizationGeneration.h"
#include "culture/NameUtils.h"
#include "utils/ArdaUtils.h"
#include <map>
namespace Arda {

class ArdaGen : public Fwg::FastWorldGenerator {

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
  float superRegionFactor = 1.0;
  // containers
  Arda::Names::NameData nData;
  std::vector<ArdaContinent> scenContinents;
  std::vector<std::shared_ptr<ArdaRegion>> ardaRegions;
  std::vector<std::shared_ptr<Arda::ArdaProvince>> ardaProvinces;
  // countries
  std::set<std::string> tags;
  Fwg::Utils::ColourTMap<std::string> countryColourMap;
  std::map<std::string, std::shared_ptr<Country>> countries;
  std::map<Rank, std::vector<std::shared_ptr<Country>>> countriesByRank;
  std::map<int, std::vector<std::shared_ptr<Country>>> countryImportanceScores;
  Civilization::CivilizationData civData;
  // images
  Fwg::Gfx::Bitmap countryMap;
  Fwg::Gfx::Bitmap superRegionMap;
  // constructors/destructors
  ArdaGen();
  ArdaGen(const std::string &configSubFolder);
  ArdaGen(Fwg::FastWorldGenerator &fwg);
  ~ArdaGen();
  /* member functions*/
  // print a map showing all countries for debug purposes
  Fwg::Gfx::Bitmap visualiseCountries(Fwg::Gfx::Bitmap &countryBmp,
                                      const int ID = -1);

  // map base continents to generic paradox compatible game continents
  void mapContinents();
  // map base regions to generic paradox compatible game regions
  virtual void mapRegions();
  // apply values read from a file to override generated data
  void applyRegionInput();
  void applyCountryInput();
  // map base provinces to generic game regions
  void mapProvinces();

  // ardaRegions are used for every single game,
  std::shared_ptr<ArdaRegion> &findStartRegion();

  // and countries are always created the same way
  template <typename T> void generateCountries() {
    countries.clear();
    countryMap.clear();
    for (auto &region : ardaRegions) {
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
                        const Arda::Utils::ResConfig &resourceConfig);
  // calculate how strong each country is
  virtual void evaluateCountries();
  virtual void printStatistics();

  virtual void writeTextFiles();
  virtual void writeLocalisation();
  virtual void writeImages();

}; // namespace Arda
} // namespace Arda
