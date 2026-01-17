#pragma once
#include "FastWorldGenerator.h"
#include "RandNum.h"
#include "areas/ArdaContinent.h"
#include "areas/ArdaProvince.h"
#include "areas/ArdaRegion.h"
#include "areas/AreaGen.h"
#include "areas/SuperRegion.h"
#include "civilisation/CivilizationGeneration.h"
#include "civilisation/NaturalFeatures.h"
#include "civilization/Location.h"
#include "civilization/NavmeshGeneration.h"
#include "countries/Country.h"
#include "countries/CountryGen.h"
#include "culture/NameUtils.h"
#include "flags/Flag.h"
#include "resources/ResourceGeneration.h"
#include "utils/ArdaUtils.h"
#include <map>
namespace Arda {

struct ArdaConfig {
  // vars - config options
  Utils::GenerationAge generationAge = Utils::GenerationAge::Renaissance;
  int numCountries = 100;
  double worldPopulationFactor = 1.0;
  double worldIndustryFactor = 1.0;
  double resourceFactor = 1.0;
  float superRegionFactor = 1.0;
  float superRegionMinDistanceFactor = 1.0;
  double targetWorldPopulation = 3'000'000'000;
  double targetWorldGdp = 8'000'000'000'000;
  Fwg::Civilization::LocationConfig locationConfig;
  void calculateTargetWorldPopulation() {
    targetWorldPopulation =
        Arda::Utils::ageConfigs[generationAge].targetWorldPopulation *
        worldPopulationFactor;
  }
  void calculateTargetWorldGdp() {
    targetWorldGdp = Arda::Utils::ageConfigs[generationAge].targetWorldGdp *
                     worldIndustryFactor;
  }
};

struct ArdaStats {
  double totalWorldPopulation = 0;
  double totalWorldGdp = 0;
  int totalCountries = 0;
  int totalColonialCountries = 0;
  int totalRegions = 0;
  int totalProvinces = 0;
  int totalLandProvinces = 0;
  int totalCoastalProvinces = 0;
  int totalOceanProvinces = 0;
  int totalColonialRegions = 0;
  std::map<std::string, int> totalResources;
};

struct ArdaData {
  double worldEconomicActivity = 0;
  Civilization::CivilizationLayer civLayer;
};

class ArdaGen : public Fwg::FastWorldGenerator {

protected:
  Fwg::Gfx::Image typeMap;

public:
  struct FactoryRegistry {
    std::function<std::shared_ptr<Arda::SuperRegion>()> superRegionFactory;
    std::function<std::shared_ptr<Arda::Country>()> countryFactory;
  } ardaFactories;

  // to allow text inputs as addition to country image input
  std::string countryMappingPath = "";
  // to allow text inputs as addition to region image input
  std::string regionMappingPath = "";
  ArdaConfig ardaConfig;
  ArdaStats ardaStats;
  ArdaData ardaData;

  // containers
  Arda::Names::NameData nData;
  std::vector<std::shared_ptr<ArdaContinent>> ardaContinents;
  std::vector<std::shared_ptr<ArdaRegion>> ardaRegions;
  std::vector<std::shared_ptr<Arda::ArdaProvince>> ardaProvinces;
  std::vector<std::shared_ptr<SuperRegion>> superRegions;
  // countries
  std::map<std::string, std::shared_ptr<Country>> countries;
  std::map<Rank, std::vector<std::shared_ptr<Country>>> countriesByRank;
  std::map<int, std::vector<std::shared_ptr<Country>>> countryImportanceScores;
  Civilization::CivilizationData civData;
  // images
  Fwg::Gfx::Image countryMap;
  Fwg::Gfx::Image superRegionMap;
  // constructors/destructors
  ArdaGen();
  ArdaGen(const std::string &configSubFolder);
  ArdaGen(Fwg::FastWorldGenerator &fwg);
  ~ArdaGen();
  /* member functions*/

  void genNaturalFeatures();
  bool loadNaturalFeatures(Fwg::Cfg &config,
                           const Fwg::Gfx::Image &inputFeatures);
  bool loadDevelopment(Fwg::Cfg &config, const std::string &path);
  bool genDevelopment(Fwg::Cfg &config);
  bool loadPopulation(Fwg::Cfg &config, const Fwg::Gfx::Image &inputPop);
  bool genPopulation(Fwg::Cfg &config);
  void genEconomyData();
  void genCultureData();
  void genCivilisationData();
  void clearLocations();
  void genLocationType(const Fwg::Civilization::LocationType &type);
  void genLocations();
  void detectLocationType(const Fwg::Civilization::LocationType &type);
  void genNavmesh(const std::vector<Fwg::Civilization::Locations::AreaLocationSet>
                 &inputSet);

  bool genWastelands(Fwg::Cfg &config);
  void generateStrategicRegions(
      std::function<std::shared_ptr<SuperRegion>()> factory);
  void
  loadStrategicRegions(std::function<std::shared_ptr<SuperRegion>()> factory,
                       const Fwg::Gfx::Image &inputImage);
  virtual void generateStateSpecifics();
  void generateCountries(std::function<std::shared_ptr<Country>()> factory);
  void loadCountries(std::function<std::shared_ptr<Country>()> factory,
                     const Fwg::Gfx::Image &inputImage);
  // print a map showing all countries for debug purposes
  Fwg::Gfx::Image visualiseCountries(Fwg::Gfx::Image &countryBmp,
                                     const int ID = -1);

  // mapping terrain types of FastWorldGen to module
  // compatible terrains
  virtual Fwg::Gfx::Image mapTerrain();
  //  map base continents to generic paradox compatible game continents
  void mapContinents();
  // map base regions to generic paradox compatible game regions
  virtual void mapRegions();
  virtual void mapCountries();
  // apply values read from a file to override generated data
  void applyRegionInput();
  void applyCountryInput();
  // map base provinces to generic game regions
  void mapProvinces();
  void totalResourceVal(const std::vector<float> &resPrev,
                        float resourceModifier,
                        const Arda::Utils::ResConfig &resourceConfig);
  virtual void gatherStatistics();
  // calculate how strong each country is
  virtual void evaluateCountries();
  virtual void printStatistics();

  virtual void writeTextFiles();
  virtual void writeLocalisation();
  virtual void writeImages();

}; // namespace Arda
} // namespace Arda
