#pragma once
#include "FastWorldGenerator.h"
#include "RandNum.h"
#include "areas/ArdaContinent.h"
#include "areas/ArdaProvince.h"
#include "areas/ArdaRegion.h"
#include "areas/AreaGen.h"
#include "areas/SuperRegion.h"
#include "civilisation/CivilizationGeneration.h"
#include "countries/Country.h"
#include "countries/CountryGen.h"
#include "culture/NameUtils.h"
#include "flags/Flag.h"
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
};

struct ArdaStats {
  int totalWorldPopulation = 0;
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
  // vars - track civil statistics
  long long worldPop = 0;
  double worldEconomicActivity = 0;
};

class ArdaGen : public Fwg::FastWorldGenerator {

protected:
  Fwg::Gfx::Bitmap typeMap;

public:
  // to allow text inputs as addition to country image input
  std::string countryMappingPath = "";
  // to allow text inputs as addition to region image input
  std::string regionMappingPath = "";
  ArdaConfig ardaConfig;
  ArdaStats ardaStats;
  ArdaData ardaData;

  // containers
  Arda::Names::NameData nData;
  std::vector<ArdaContinent> ardaContinents;
  std::vector<std::shared_ptr<ArdaRegion>> ardaRegions;
  std::vector<std::shared_ptr<Arda::ArdaProvince>> ardaProvinces;
  std::vector<std::shared_ptr<SuperRegion>> superRegions;
  // countries
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
  void generateStrategicRegions(
      std::function<std::shared_ptr<SuperRegion>()> factory);
  void
  loadStrategicRegions(std::function<std::shared_ptr<SuperRegion>()> factory,
                       const Fwg::Gfx::Bitmap &inputImage);
  virtual void generateStateSpecifics();
  void generateCountries(std::function<std::shared_ptr<Country>()> factory);
  void loadCountries(std::function<std::shared_ptr<Country>()> factory,
                     const Fwg::Gfx::Bitmap &inputImage);
  // print a map showing all countries for debug purposes
  Fwg::Gfx::Bitmap visualiseCountries(Fwg::Gfx::Bitmap &countryBmp,
                                      const int ID = -1);

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
