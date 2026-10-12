#include "ArdaGen.h"
#include "simulation/SimulationArtifactWriter.h"
#include "simulation/SimulationExporter.h"
#include <algorithm>
#include <fstream>
#include <sstream>
namespace Logging = Fwg::Utils::Logging;
namespace Arda {
using namespace Fwg::Gfx;

namespace {
std::string encodeSidecarField(const std::string &value) {
  std::string encoded;
  encoded.reserve(value.size());
  for (const unsigned char character : value) {
    if (character == '%' || character == '\t' || character == '\n' ||
        character == '\r') {
      constexpr char hex[] = "0123456789ABCDEF";
      encoded.push_back('%');
      encoded.push_back(hex[character >> 4]);
      encoded.push_back(hex[character & 0x0F]);
    } else {
      encoded.push_back(static_cast<char>(character));
    }
  }
  return encoded;
}

bool decodeSidecarField(const std::string &value, std::string &decoded) {
  decoded.clear();
  for (std::size_t i = 0; i < value.size(); ++i) {
    if (value[i] != '%') {
      decoded.push_back(value[i]);
      continue;
    }
    if (i + 2 >= value.size())
      return false;
    const auto hexValue = [](char character) -> int {
      if (character >= '0' && character <= '9')
        return character - '0';
      if (character >= 'A' && character <= 'F')
        return character - 'A' + 10;
      if (character >= 'a' && character <= 'f')
        return character - 'a' + 10;
      return -1;
    };
    const int high = hexValue(value[i + 1]);
    const int low = hexValue(value[i + 2]);
    if (high < 0 || low < 0)
      return false;
    decoded.push_back(static_cast<char>((high << 4) | low));
    i += 2;
  }
  return true;
}

std::vector<std::string> splitSidecarFields(const std::string &line) {
  std::vector<std::string> fields;
  std::istringstream stream(line);
  std::string field;
  while (std::getline(stream, field, '\t'))
    fields.push_back(std::move(field));
  if (!line.empty() && line.back() == '\t')
    fields.emplace_back();
  return fields;
}

void resetGeneratedLanguageWords(const Civilization::CivilizationData &civData) {
  for (const auto &culture : civData.cultures) {
    if (culture && culture->language)
      culture->language->resetGeneratedWords();
  }
}
} // namespace

bool ArdaGen::saveCultureGroupState(const std::string &path) const {
  std::ofstream output(path, std::ios::trunc);
  if (!output)
    return false;

  output << "ARDA_CULTURE_GROUPS\t1\n";
  for (std::size_t groupIndex = 0;
       groupIndex < civData.cultureGroups.size(); ++groupIndex) {
    const auto &group = civData.cultureGroups[groupIndex];
    if (!group)
      continue;
    const auto colour = group->getColour();
    const auto centre = group->getCenter();
    std::vector<std::string> datasets;
    const auto languageGroup = group->getLanguageGroup();
    if (languageGroup)
      datasets = languageGroup->mergedDataset.influences;

    output << "GROUP\t" << groupIndex << '\t'
           << encodeSidecarField(group->getName()) << '\t'
           << (centre ? centre->ID : -1) << '\t'
           << static_cast<int>(colour.getRed()) << '\t'
           << static_cast<int>(colour.getGreen()) << '\t'
           << static_cast<int>(colour.getBlue()) << '\t' << datasets.size();
    for (const auto &dataset : datasets)
      output << '\t' << encodeSidecarField(dataset);
    output << '\n';
  }
  return output.good();
}

bool ArdaGen::loadCultureGroupState(const std::string &path) {
  std::ifstream input(path);
  if (!input)
    return false;

  LanguageGenerator sidecarLanguageGenerator(
      Fwg::Cfg::Values().resourcePath + "/names/languageGroups/");

  std::string line;
  if (!std::getline(input, line) || line != "ARDA_CULTURE_GROUPS\t1")
    return false;

  struct CultureGroupState {
    std::size_t groupIndex;
    std::string name;
    int centreProvinceId;
    Fwg::Gfx::Colour colour;
    std::vector<std::string> datasets;
  };
  std::vector<CultureGroupState> states;
  while (std::getline(input, line)) {
    if (line.empty())
      continue;
    const auto fields = splitSidecarFields(line);
    if (fields.size() < 8 || fields[0] != "GROUP")
      return false;

    CultureGroupState state;
    int red = 0;
    int green = 0;
    int blue = 0;
    std::size_t datasetCount = 0;
    try {
      state.groupIndex = std::stoul(fields[1]);
      if (!decodeSidecarField(fields[2], state.name))
        return false;
      state.centreProvinceId = std::stoi(fields[3]);
      red = std::stoi(fields[4]);
      green = std::stoi(fields[5]);
      blue = std::stoi(fields[6]);
      datasetCount = std::stoul(fields[7]);
    } catch (const std::exception &) {
      return false;
    }
    if (red < 0 || red > 255 || green < 0 || green > 255 || blue < 0 ||
        blue > 255 || fields.size() != datasetCount + 8 ||
        state.groupIndex >= civData.cultureGroups.size())
      return false;
    state.colour = Fwg::Gfx::Colour(red, green, blue);
    for (std::size_t i = 0; i < datasetCount; ++i) {
      std::string dataset;
      if (!decodeSidecarField(fields[i + 8], dataset) || dataset.empty())
        return false;
      state.datasets.push_back(std::move(dataset));
    }
    states.push_back(std::move(state));
  }
  //if (states.size() != civData.cultureGroups.size())
  //  return false;

  std::vector<std::shared_ptr<LanguageGroup>> languageGroups;
  languageGroups.resize(states.size());
  std::vector<bool> groupSeen(civData.cultureGroups.size(), false);
  for (const auto &state : states) {
    if (state.groupIndex >= groupSeen.size() || groupSeen[state.groupIndex])
      return false;
    groupSeen[state.groupIndex] = true;
  }
  if (std::find(groupSeen.begin(), groupSeen.end(), false) != groupSeen.end())
    return false;

  std::fill(groupSeen.begin(), groupSeen.end(), false);
  for (const auto &state : states) {
    const auto &group = civData.cultureGroups[state.groupIndex];
    if (!group || groupSeen[state.groupIndex])
      return false;
    groupSeen[state.groupIndex] = true;
    const auto centre = group->getCenter();
    //if ((centre ? centre->ID : -1) != state.centreProvinceId)
    //  return false;
    for (const auto &dataset : state.datasets) {
      if (!sidecarLanguageGenerator.datasetsByLanguage.contains(dataset))
        return false;
    }
    std::vector<std::string> datasets = state.datasets;
    auto languageGroup = std::make_shared<LanguageGroup>(
        sidecarLanguageGenerator.generateLanguageGroup(
            civData.cultureGroups[state.groupIndex]->getCultures().size(),
            datasets, Fwg::Cfg::Values().mapSeed));
    if (languageGroup->name.empty())
      return false;
    languageGroups[state.groupIndex] = std::move(languageGroup);
  }

  civData.languageGroups.clear();
  for (const auto &state : states) {
    auto &group = civData.cultureGroups[state.groupIndex];
    group->setColour(state.colour);
    group->setLanguageGroup(languageGroups[state.groupIndex]);
    civData.languageGroups.push_back(languageGroups[state.groupIndex]);
  }

  nameCultureDependentAreas();
  return true;
}

ArdaGen::ArdaGen() {
  factories.provinceFactory = []() {
    return std::make_shared<Arda::ArdaProvince>();
  };
  factories.regionFactory = []() {
    return std::make_shared<Arda::ArdaRegion>();
  };
  factories.continentFactory = [](const std::vector<int> &pixels) {
    return std::make_shared<Arda::ArdaContinent>(pixels);
  };
  ardaFactories.superRegionFactory =
      []() -> std::shared_ptr<Arda::SuperRegion> {
    return std::make_shared<Arda::SuperRegion>();
  };
  ardaFactories.countryFactory = []() -> std::shared_ptr<Arda::Country> {
    return std::make_shared<Arda::Country>();
  };
}

ArdaGen::ArdaGen(const std::string &configSubFolder)
    : FastWorldGenerator(configSubFolder) {
  Fwg::Utils::Logging::logLine("ArdaGen::ArdaGen");
  Gfx::Flag::readColourGroups();
  Gfx::Flag::readFlagTypes();
  Gfx::Flag::readFlagTemplates();
  Gfx::Flag::readSymbolTemplates();
  superRegionMap = Image(0, 0, 24);
  factories.provinceFactory = []() {
    return std::make_shared<Arda::ArdaProvince>();
  };
  factories.regionFactory = []() {
    return std::make_shared<Arda::ArdaRegion>();
  };
  factories.continentFactory = [](const std::vector<int> &pixels) {
    return std::make_shared<Arda::ArdaContinent>(pixels);
  };
  ardaFactories.superRegionFactory =
      []() -> std::shared_ptr<Arda::SuperRegion> {
    return std::make_shared<Arda::SuperRegion>();
  };
  ardaFactories.countryFactory = []() -> std::shared_ptr<Arda::Country> {
    return std::make_shared<Arda::Country>();
  };
  Fwg::Utils::Logging::logLine("ArdaGen::ArdaGen Done");
}

ArdaGen::ArdaGen(Fwg::FastWorldGenerator &fwg) : FastWorldGenerator(fwg) {
  factories.provinceFactory = []() {
    return std::make_shared<Arda::ArdaProvince>();
  };
  factories.regionFactory = []() {
    return std::make_shared<Arda::ArdaRegion>();
  };
  factories.continentFactory = [](const std::vector<int> &pixels) {
    return std::make_shared<Arda::ArdaContinent>(pixels);
  };
  ardaFactories.superRegionFactory =
      []() -> std::shared_ptr<Arda::SuperRegion> {
    return std::make_shared<Arda::SuperRegion>();
  };
  ardaFactories.countryFactory = []() -> std::shared_ptr<Arda::Country> {
    return std::make_shared<Arda::Country>();
  };
}

ArdaGen::~ArdaGen() {}

void ArdaGen::mapContinents() {
  Logging::logLine("Mapping Continents");
  ardaContinents.clear();
  for (const auto &continent : this->areaData.continents) {
    // reinterpret FastWorldGen continent as ArdaContinent
    auto ardaContinent = std::dynamic_pointer_cast<ArdaContinent>(continent);
    ardaContinent->ardaProvinces.clear();
    ardaContinent->ardaRegions.clear();
    for (const auto &prov : ardaContinent->provinces) {
      ardaContinent->ardaProvinces.push_back(
          std::dynamic_pointer_cast<Arda::ArdaProvince>(prov));
    }
    for (const auto &region : ardaContinent->regions) {
      ardaContinent->ardaRegions.push_back(
          std::dynamic_pointer_cast<Arda::ArdaRegion>(region));
    }
    ardaContinents.push_back(ardaContinent);
  }
}

void ArdaGen::mapRegions() {
  Logging::logLine("Mapping Regions");
  ardaRegions.clear();

  for (auto &region : this->areaData.regions) {
    std::sort(region->provinces.begin(), region->provinces.end(),
              [](const std::shared_ptr<Fwg::Areas::Province> a,
                 const std::shared_ptr<Fwg::Areas::Province> b) {
                return (*a < *b);
              });
    auto ardaRegion = std::dynamic_pointer_cast<Arda::ArdaRegion>(region);
    ardaRegion->ardaProvinces.clear();

    for (auto &province : ardaRegion->provinces) {
      ardaRegion->ardaProvinces.push_back(ardaProvinces[province->ID]);
    }
    // save game region
    ardaRegions.push_back(ardaRegion);
  }
  // sort by Arda::ArdaProvince ID
  // std::sort(ardaRegions.begin(), ardaRegions.end(),
  //          [](auto l, auto r) { return *l < *r; });
  // check if we have the same amount of ardaProvinces as FastWorldGen provinces
  if (ardaProvinces.size() != this->areaData.provinces.size())
    throw(std::runtime_error("Fatal: Lost provinces, terminating"));
  if (ardaRegions.size() != this->areaData.regions.size())
    throw(std::runtime_error("Fatal: Lost regions, terminating"));
  for (const auto &ardaRegion : ardaRegions) {
    if (ardaRegion->ID > ardaRegions.size()) {
      throw(std::runtime_error("Fatal: Invalid region IDs, terminating"));
    }
  }
  applyRegionInput();
}

void ArdaGen::mapCountries() {}

void ArdaGen::applyRegionInput() {
  Fwg::Utils::ColourTMap<std::vector<std::string>> regionInputMap;
  if (regionMappingPath.size() && std::filesystem::exists(regionMappingPath)) {
    auto mappingFileLines = Fwg::Parsing::getLines(regionMappingPath);
    for (auto &line : mappingFileLines) {
      if (line.size()) {
        auto tokens = Fwg::Parsing::getTokens(line, ';');
        if (tokens.size() > 3) {
          auto colour = Fwg::Gfx::Colour(
              std::stoi(tokens[0]), std::stoi(tokens[1]), std::stoi(tokens[2]));
          regionInputMap.setValue(colour, tokens);
        }
      }
    }
  }
  for (auto &ardaRegion : this->ardaRegions) {
    if (regionInputMap.contains(ardaRegion->colour)) {
      if (regionInputMap[ardaRegion->colour].size() > 3 &&
          regionInputMap[ardaRegion->colour][3].size()) {
        // get the predefined name
        ardaRegion->name = regionInputMap[ardaRegion->colour][3];
      }
      if (regionInputMap[ardaRegion->colour].size() > 4 &&
          regionInputMap[ardaRegion->colour][4].size()) {
        try {

          // get the predefined population
          ardaRegion->totalPopulation =
              stoi(regionInputMap[ardaRegion->colour][4]);
        } catch (std::exception &e) {
          Fwg::Utils::Logging::logLine(
              "ERROR: Some of the tokens can't be turned into a population "
              "number. The faulty token is ",
              regionInputMap[ardaRegion->colour][4]);
        }
      }
      ardaRegion->importLine(regionInputMap[ardaRegion->colour]);
    }
  }
  // debug visualisation of all regions, if coastal they are yellow, if sea they
  // are blue, if non-coastal they are green
  Image regionMap(Fwg::Cfg::Values().width, Fwg::Cfg::Values().height, 24);
  for (auto &ardaRegion : this->ardaRegions) {
    for (auto &gameProv : ardaRegion->ardaProvinces) {
      for (auto &pix : gameProv->pixels) {
        if (ardaRegion->isSea()) {
          regionMap.setColourAtIndex(pix, Fwg::Cfg::Values().colours.at("sea"));

        } else if (ardaRegion->isCoastalToOcean() && !ardaRegion->isSea()) {
          regionMap.setColourAtIndex(pix,
                                     Fwg::Cfg::Values().colours.at("ores"));

        } else if (ardaRegion->isLake()) {
          regionMap.setColourAtIndex(pix,
                                     Fwg::Cfg::Values().colours.at("lake"));
        }

        else {
          regionMap.setColourAtIndex(pix,
                                     Fwg::Cfg::Values().colours.at("land"));
          if (gameProv->isCoastalToOcean()) {
            regionMap.setColourAtIndex(
                pix, Fwg::Cfg::Values().colours.at("autumnForest"));
          }
        }
      }
    }
  }
  Png::save(regionMap,
            Fwg::Cfg::Values().mapsPath + "debug/ardaRegionTypes.png", false);
}

void ArdaGen::applyCountryInput() {}

void ArdaGen::mapProvinces() {
  ardaProvinces.clear();
  for (auto &prov : this->areaData.provinces) {
    // now create ardaProvinces from FastWorldGen provinces
    auto gP = std::dynamic_pointer_cast<Arda::ArdaProvince>(prov);
    ardaProvinces.push_back(gP);
  }
}

Fwg::Gfx::Image ArdaGen::mapTerrain() {
  using namespace Arda::Civilization;
  auto &civLayer = this->ardaData.civLayer;
  auto &overlayColours = Fwg::Cfg::Values().topographyOverlayColours;
  auto topoMap =
      Fwg::Gfx::Image(Fwg::Cfg::Values().width, Fwg::Cfg::Values().height, 24);

  // Define the topography types we care about and their thresholds
  struct TopoDef {
    TopographyType type;
    double provinceThreshold; // fraction of province pixels
    double regionThreshold;   // fraction of region pixels
  };

  const std::vector<TopoDef> topoDefs = {
      {TopographyType::CITY, 0.25, 0.25},
      {TopographyType::PORTCITY, 0.00, 0.00}, // any pixel triggers inclusion
      {TopographyType::MARSH, 0.50, 0.50},
      {TopographyType::WASTELAND, 0.50, 0.50},
      {TopographyType::FARMLAND, 0.50, 0.50},
      {TopographyType::MINE, 0.00, 0.00},
      {TopographyType::FORESTRY, 0.00, 0.00},
  };

  for (const auto &region : ardaRegions) {
    for (const auto &topoDef : topoDefs)
      region->topographyTypes.erase(topoDef.type);
    for (const auto &province : region->ardaProvinces) {
      for (const auto &topoDef : topoDefs)
        province->topographyTypes.erase(topoDef.type);
    }
  }

  // Process all regions
  for (auto &region : ardaRegions) {
    std::unordered_map<TopographyType, int> regionTotals;

    const auto regionSize = region->getNonOwningPixelView().size();

    // Process each province
    for (auto &province : region->ardaProvinces) {
      if (!province->isLand())
        continue;

      const int provSize = static_cast<int>(province->pixels.size());

      for (const auto &[type, provThresh, _] : topoDefs) {
        const int count = civLayer.countOfTypeInRange(province->pixels, type);
        regionTotals[type] += count;

        // Province classification
        const bool meetsThreshold =
            provThresh == 0.0 ? (count > 0) : (count > provSize * provThresh);

        if (meetsThreshold)
          province->topographyTypes.insert(type);
      }

      // City special case: combine with portcity pixels for density
      const int cityPixels =
          civLayer.countOfTypeInRange(province->pixels, TopographyType::CITY);
      const int portPixels = civLayer.countOfTypeInRange(
          province->pixels, TopographyType::PORTCITY);
      if (cityPixels + portPixels > provSize / 4) {
        province->topographyTypes.insert(TopographyType::CITY);
      }
    }

    // Region-level classification
    for (const auto &[type, _, regionThresh] : topoDefs) {
      const int total = regionTotals[type];
      const bool meetsThreshold = regionThresh == 0.0
                                      ? (total > 0)
                                      : (total > regionSize * regionThresh);

      if (meetsThreshold)
        region->topographyTypes.insert(type);
    }
  }

  return topoMap;
}

void ArdaGen::genNaturalFeatures() {
  Fwg::Utils::Randomisation::resetRandomisation();
  // detect marshes
  // Arda::NaturalFeatures::detectMarshes(terrainData, climateData,
  //                                     ardaData.civLayer,
  //                                     Fwg::Cfg::Values());
  clearLocations();
  ardaData.civLayer.clear();
  // genWastelands(Fwg::Cfg::Values());
  mapTerrain();
}

bool ArdaGen::loadNaturalFeatures(Fwg::Cfg &config,
                                  const Fwg::Gfx::Image &inputFeatures) {
  // clearLocations();
  ardaData.civLayer.clear();
  Arda::NaturalFeatures::loadNaturalFeatures(config, inputFeatures,
                                             ardaData.civLayer);
  mapTerrain();
  return true;
}

bool ArdaGen::loadDevelopment(Fwg::Cfg &config, const std::string &path) {
  Civilization::loadDevelopment(Fwg::IO::Reader::readGenericImage(path, config),
                                ardaProvinces, ardaRegions, ardaContinents);

  generateEconomyData(civData, ardaProvinces, ardaRegions, ardaContinents,
                      ardaConfig.targetWorldPopulation);
  gatherStatistics();
  return true;
}
bool ArdaGen::genDevelopment(Fwg::Cfg &config) {
  Fwg::Utils::Randomisation::resetRandomisation();
  Civilization::generateDevelopment(ardaProvinces, ardaRegions, ardaContinents);
  gatherStatistics();
  return true;
}
bool ArdaGen::deriveDevelopment(Fwg::Cfg &config) {
  Fwg::Utils::Randomisation::resetRandomisation();
  Civilization::deriveDevelopment(ardaData.simulationExport, ardaProvinces,
                                  ardaRegions, ardaContinents);
  gatherStatistics();
  return true;
}

bool ArdaGen::loadPopulation(Fwg::Cfg &config,
                             const Fwg::Gfx::Image &inputPop) {
  ardaConfig.calculateTargetWorldPopulation();
  Civilization::loadPopulation(inputPop, ardaProvinces, ardaRegions,
                               ardaContinents,
                               ardaConfig.targetWorldPopulation);
  generateEconomyData(civData, ardaProvinces, ardaRegions, ardaContinents,
                      ardaConfig.targetWorldPopulation);
  gatherStatistics();

  return true;
}
bool ArdaGen::genPopulation(Fwg::Cfg &config) {
  Fwg::Utils::Randomisation::resetRandomisation();
  ardaConfig.calculateTargetWorldPopulation();
  Civilization::generatePopulation(civData, ardaProvinces, ardaRegions,
                                   ardaContinents,
                                   ardaConfig.targetWorldPopulation);
  gatherStatistics();

  return true;
}
bool ArdaGen::derivePopulation(Fwg::Cfg &config) {
  Fwg::Utils::Randomisation::resetRandomisation();
  Civilization::derivePopulation(ardaData.simulationExport, civData,
                                 ardaProvinces, ardaRegions, ardaContinents,
                                 ardaConfig.targetWorldPopulation);
  gatherStatistics();
  return true;
}

void ArdaGen::genEconomyData() {
  Fwg::Utils::Randomisation::resetRandomisation();
  ardaConfig.calculateTargetWorldGdp();
  Arda::Civilization::generateEconomyData(civData, ardaProvinces, ardaRegions,
                                          ardaContinents,
                                          ardaConfig.targetWorldGdp);
  gatherStatistics();
}

void ArdaGen::genCultureData() {
  Fwg::Utils::Randomisation::resetRandomisation();
  Arda::Civilization::generateCultureData(civData, ardaProvinces, ardaRegions,
                                          ardaContinents, superRegions);
  gatherStatistics();
}

bool ArdaGen::loadCultures(const Fwg::Gfx::Image &inputImage) {
  const auto &config = Fwg::Cfg::Values();
  if (!config.validateResolution(inputImage.width(), inputImage.height()) ||
      inputImage.size() != config.processingArea) {
    Fwg::Utils::Logging::logLine(
        "Resolution of culture map does not have correct resolution");
    return false;
  }
  if (!Arda::Civilization::loadCulturesFromImage(civData, ardaProvinces,
                                                 ardaRegions, inputImage))
    return false;

  Arda::Gfx::displayCultureGroups(ardaProvinces);
  Arda::Gfx::displayCultures(ardaProvinces);
  nameCultureDependentAreas();
  Arda::Areas::saveRegions(ardaRegions, config.mapsPath + "areas/",
                           Arda::Gfx::visualiseRegions(ardaRegions));
  gatherStatistics();
  return true;
}

void ArdaGen::deriveCultureData() {
  Fwg::Utils::Randomisation::resetRandomisation();
  Arda::Civilization::deriveCultureData(ardaData.simulationExport, civData,
                                        ardaProvinces, ardaRegions,
                                        ardaContinents, superRegions);
  gatherStatistics();
}

void ArdaGen::genCivilisationData() {
  Fwg::Utils::Randomisation::resetRandomisation();
  genNaturalFeatures();
  ardaConfig.calculateTargetWorldPopulation();
  ardaConfig.calculateTargetWorldGdp();

  Arda::Simulation::HistorySimulation simulation;
  const Arda::Simulation::Input input{ardaContinents, &climateData,
                                      &terrainData};
  const auto result = simulation.runSimulation(input);
  std::vector<Arda::Simulation::ValidationError> exportErrors;
  simulation.writeArtifacts(input, result, worldMap,
                            Fwg::Cfg::Values().mapsPath + "simulation",
                            exportErrors);
  std::map<Arda::Simulation::ProvinceId, size_t> provinceAreas;
  for (const auto &province : ardaProvinces)
    provinceAreas.emplace(province->ID, province->pixels.size());
  ardaData.simulationExport =
      Arda::Simulation::SimulationExporter::exportFinalState(
          result.finalState, simulation.getProvinceRegions(),
          result.polityHistory, result.events, result.wars, provinceAreas,
          simulation.getConfiguration());
  Arda::Simulation::artifacts::writePolityStatisticsCsv(
      ardaData.simulationExport,
      Fwg::Cfg::Values().mapsPath + "simulation/polity_statistics.csv",
      simulation.getConfiguration().startYear, exportErrors);

  Arda::Civilization::generateFullCivilisationData(
      ardaData.simulationExport, ardaRegions, ardaProvinces, civData,
      ardaContinents, superRegions, ardaConfig.targetWorldPopulation,
      ardaConfig.targetWorldGdp);
  genLocations();
  gatherStatistics();
}

void ArdaGen::clearLocations() {
  Fwg::Civilization::Locations::clearLocations(areaData.regions);
  locationMap = Arda::Gfx::displayLocations(areaData.regions, worldMap);
}

void ArdaGen::genLocations() {
  Fwg::Utils::Randomisation::resetRandomisation();
  locationMap.clear();
  Fwg::Civilization::Locations::generateLocations(
      areaData.regions, terrainData, climateData, provinceMap, areaData,
      ardaConfig.locationConfig);
  nameGeneratedLocations();
  locationMap = Arda::Gfx::displayLocations(areaData.regions, worldMap);
  Fwg::Gfx::Png::save(locationMap,
                      Fwg::Cfg::Values().mapsPath + "world/locations.png");
  Arda::Civilization::applyCivilisationTopography(ardaData.civLayer,
                                                  ardaProvinces);
  mapTerrain();
}

void ArdaGen::genLocationType(const Fwg::Civilization::LocationType &type) {
  Fwg::Utils::Randomisation::resetRandomisation();
  using namespace Arda::Civilization;
  using namespace Fwg::Civilization;
  // Mapping from location types to topography types
  static const std::unordered_map<LocationType, TopographyType> typeMap = {
      {LocationType::City, TopographyType::CITY},
      {LocationType::Port, TopographyType::PORTCITY},
      {LocationType::Farm, TopographyType::FARMLAND},
      {LocationType::Mine, TopographyType::MINE},
      {LocationType::Forest, TopographyType::FORESTRY},
  };
  ardaData.civLayer.clear(typeMap.at(type));
  Fwg::Civilization::Locations::generateLocationCategory(
      areaData.regions, terrainData, climateData, provinceMap, areaData,
      ardaConfig.locationConfig, type);
  nameGeneratedLocations();
  locationMap = Arda::Gfx::displayLocations(areaData.regions, worldMap);
  Fwg::Gfx::Png::save(locationMap,
                      Fwg::Cfg::Values().mapsPath + "world/locations.png");
  Arda::Civilization::applyCivilisationTopography(ardaData.civLayer,
                                                  ardaProvinces);
  // TODO Is this required mapTerrain();
}

void ArdaGen::detectLocationType(const Fwg::Civilization::LocationType &type) {
  detectLocationTypeInternal(type);
  nameGeneratedLocations();
}

void ArdaGen::detectLocationTypeInternal(
    const Fwg::Civilization::LocationType &type) {
  auto topotype = Arda::Civilization::TopographyType::CITY;
  if (type == Fwg::Civilization::LocationType::Farm) {
    topotype = Arda::Civilization::TopographyType::FARMLAND;
  } else if (type == Fwg::Civilization::LocationType::Forest) {
    topotype = Arda::Civilization::TopographyType::FORESTRY;
  } else if (type == Fwg::Civilization::LocationType::Mine) {
    topotype = Arda::Civilization::TopographyType::MINE;
  } else if (type == Fwg::Civilization::LocationType::Port) {
    topotype = Arda::Civilization::TopographyType::PORTCITY;
  } else if (type == Fwg::Civilization::LocationType::City) {
    topotype = Arda::Civilization::TopographyType::CITY;
  }

  Fwg::Civilization::Locations::detectLocationsFromPixels(
      ardaData.civLayer.getAll(topotype), provinceMap, areaData,
      areaData.regions, type);
  // Arda::Civilization::applyCivilisationTopography(ardaData.civLayer,
  //                                                 ardaProvinces);
  mapTerrain();
}

void ArdaGen::nameGeneratedLocations() {
  resetGeneratedLanguageWords(civData);
  for (const auto &region : ardaRegions) {
    if (!region)
      continue;

    const auto culture = region->getPrimaryCulture();
    const auto language = culture ? culture->language : nullptr;
    for (std::size_t i = 0; i < region->locations.size(); ++i) {
      const auto &location = region->locations[i];
      if (!location)
        continue;

      std::string name;
      if (language)
        name = language->generateAreaName("");
      if (name.empty())
        name = "region_" + std::to_string(region->ID) + "_location_" +
               std::to_string(i + 1);
      location->name = std::move(name);
    }
  }
}

void ArdaGen::loadLocations(const Fwg::Gfx::Image &inputImage) {
  auto &cfg = Fwg::Cfg::Values();
  clearLocations();
  loadNaturalFeatures(cfg, inputImage);
  detectLocationTypeInternal(Fwg::Civilization::LocationType::Farm);
  detectLocationTypeInternal(Fwg::Civilization::LocationType::City);
  detectLocationTypeInternal(Fwg::Civilization::LocationType::Port);
  nameGeneratedLocations();
  // detectLocationType(Fwg::Civilization::LocationType::Mine);
  // detectLocationType(Fwg::Civilization::LocationType::Forest);
  Arda::Civilization::applyCivilisationTopography(ardaData.civLayer,
                                                  ardaProvinces);
  locationMap = Arda::Gfx::displayLocations(areaData.regions, worldMap);
  mapTerrain();
}

void ArdaGen::generateCountries(
    std::function<std::shared_ptr<Country>()> factory) {
  Fwg::Utils::Randomisation::resetRandomisation();

  std::filesystem::create_directory(Fwg::Cfg::Values().mapsPath + "countries");
  // generate country data
  if (factory != nullptr) {
    // Arda::Countries::generateCountries(
    //     ardaConfig.generationAge, factory, ardaConfig.numCountries,
    //     ardaRegions, countries, ardaProvinces, civData, nData);

    Arda::Countries::deriveCountries(ardaData.simulationExport,
                                     ardaConfig.generationAge, factory,
                                     ardaConfig.numCountries, ardaRegions,
                                     countries, ardaProvinces, civData, nData);
  }
  //  first gather generic neighbours, they will be mapped to hoi4 countries
  //  in mapCountries
  Arda::Countries::evaluateCountryNeighbours(areaData.regions, ardaRegions,
                                             countries);
  Fwg::Utils::Logging::logLine("Visualising Countries");
  visualiseCountries(countryMap, worldMap);
  Fwg::Gfx::Png::save(countryMap,
                      Fwg::Cfg::Values().mapsPath + "countries/countries.png");
  mapCountries();
  Arda::Countries::saveCountries(countries,
                                 Fwg::Cfg::Values().mapsPath + "countries/");
}

void ArdaGen::loadCountries(std::function<std::shared_ptr<Country>()> factory,
                            const std::string &path) {
  // generate country data
  if (factory != nullptr) {
    Arda::Countries::loadCountriesFromText(ardaConfig.generationAge, factory,
                                           ardaRegions, countries, civData,
                                           nData, Fwg::Parsing::readFile(path));
  }
  Arda::Countries::evaluateCountryNeighbours(areaData.regions, ardaRegions,
                                             countries);
  mapCountries();
  nameCountries();
}

void ArdaGen::loadCountries(std::function<std::shared_ptr<Country>()> factory,
                            const Fwg::Gfx::Image &inputImage) {
  // generate country data
  if (factory != nullptr) {
    Arda::Countries::loadCountries(ardaConfig.generationAge, factory,
                                   ardaRegions, countries, civData, nData,
                                   inputImage);
  }
  Arda::Countries::evaluateCountryNeighbours(areaData.regions, ardaRegions,
                                             countries);
  mapCountries();
  nameCountries();
}

void ArdaGen::nameCultureDependentAreas() {
  resetGeneratedLanguageWords(civData);
  Civilization::nameRegions(ardaRegions);
  Civilization::nameContinents(ardaContinents, ardaRegions);
  Civilization::nameSuperRegions(superRegions, ardaRegions);
  // nameCountries();
}

void ArdaGen::nameCountries() {
  resetGeneratedLanguageWords(civData);
  for (auto &[tag, country] : countries) {
    if (!country)
      continue;
    country->gatherCultureShares();
    const auto culture = country->getPrimaryCulture();
    if (!culture || !culture->language)
      continue;
    country->name = culture->language->generateGenericCapitalizedWord();
    country->adjective = culture->language->getAdjectiveForm(country->name);
  }
}

void ArdaGen::deriveCountries(
    std::function<std::shared_ptr<Country>()> factory) {
  // generate country data
  if (factory != nullptr) {
    Arda::Countries::deriveCountries(ardaData.simulationExport,
                                     ardaConfig.generationAge, factory,
                                     ardaConfig.numCountries, ardaRegions,
                                     countries, ardaProvinces, civData, nData);
  }
  Arda::Countries::evaluateCountryNeighbours(areaData.regions, ardaRegions,
                                             countries);
  mapCountries();
}

void ArdaGen::genNavmesh(
    const std::vector<Fwg::Civilization::Locations::AreaLocationSet> &inputSet,
    const std::vector<std::shared_ptr<Fwg::Areas::Area>>
        &inputNavigationAreas) {
  Fwg::Utils::Randomisation::resetRandomisation();
  // no input means we default to all locations in a region
  if (inputSet.size() == 0) {
    std::vector<Fwg::Civilization::Locations::AreaLocationSet> defaultInputSet;
    for (auto &region : areaData.regions) {
      Fwg::Civilization::Locations::AreaLocationSet regionLocationSet;
      regionLocationSet.area = region;
      for (auto &location : region->locations) {
        regionLocationSet.locations.push_back(location);
      }
      defaultInputSet.push_back(regionLocationSet);
    }
    Fwg::Civilization::Locations::generateConnections(
        defaultInputSet, terrainData, climateData, inputNavigationAreas);
  } else {
    Fwg::Civilization::Locations::generateConnections(
        inputSet, terrainData, climateData, inputNavigationAreas);
  }
  // navmeshMap = Fwg::Gfx::displayConnections(areaData.regions, locationMap);
}

bool ArdaGen::genWastelands(Fwg::Cfg &config) {
  Arda::NaturalFeatures::detectWastelands(terrainData, climateData, ardaRegions,
                                          ardaData.civLayer, config);
  auto worldOverlayMap = Arda::Gfx::displayWorldOverlayMap(
      climateData, worldMap, ardaData.civLayer);
  Fwg::Gfx::Png::save(worldOverlayMap, config.mapsPath + "worldOverlayMap.png",
                      true);

  return true;
}

bool ArdaGen::generateStrategicRegions(
    std::function<std::shared_ptr<SuperRegion>()> factory) {
  Fwg::Utils::Randomisation::resetRandomisation();
  if (!Arda::Areas::generateStrategicRegions(factory, superRegions, ardaRegions,
                                             ardaConfig.superRegionFactor)) {
    superRegions.clear();
    return false;
  }
  resetGeneratedLanguageWords(civData);
  Civilization::nameSuperRegions(superRegions, ardaRegions);
  return true;
}

bool ArdaGen::loadStrategicRegions(
    std::function<std::shared_ptr<SuperRegion>()> factory,
    const Fwg::Gfx::Image &inputImage) {
  Fwg::Utils::Logging::logLine("Loading Strategic Regions from Image");
  if (!Arda::Areas::loadStrategicRegions(inputImage, factory, superRegions,
                                         ardaRegions, terrainData)) {
    superRegions.clear();
    return false;
  }
  resetGeneratedLanguageWords(civData);
  Civilization::nameSuperRegions(superRegions, ardaRegions);
  return true;
}

void ArdaGen::generateStateSpecifics() {
  Fwg::Utils::Randomisation::resetRandomisation();
  Arda::Areas::saveRegions(ardaRegions, Fwg::Cfg::Values().mapsPath + "areas/",
                           Arda::Gfx::visualiseRegions(ardaRegions));
}

void ArdaGen::finaliseData() {}

Image ArdaGen::visualiseCountries(Fwg::Gfx::Image &countryBmp,
                                  const Fwg::Gfx::Image &worldMap,
                                  const int ID) {
  Logging::logLine("Drawing borders");
  auto &config = Fwg::Cfg::Values();
  const auto stateBorderColour = Fwg::Gfx::Colour(0, 0, 0);
  if (!countryBmp.initialised() || countryBmp.size() != config.processingArea) {
    countryBmp = Image(config.width, config.height, 24);
  }
  if (ID > -1) {
    const auto &region = ardaRegions[ID];
    Fwg::Gfx::Colour countryColour(0, 0, 0);

    if (region->owner) {
      countryColour = region->owner->colour;

      for (const auto &prov : region->provinces) {
        for (const auto pix : prov->getNonOwningPixelView()) {
          // do not overwrite state borders
          if (countryBmp.imageData[pix] != stateBorderColour) {
            countryBmp.imageData[pix] = countryColour;
          }
        }
      }
    } else {
      for (const auto &prov : region->provinces) {
        for (const auto pix : prov->getNonOwningPixelView()) {
          countryBmp.imageData[pix] = worldMap[pix];
        }
      }
    }
  } else {
    // Parallel loop over regions
    std::for_each(std::execution::par, ardaRegions.begin(), ardaRegions.end(),
                  [&](const auto &region) {
                    auto countryColour = Fwg::Gfx::Colour(0, 0, 0);

                    if (region->owner) {
                      countryColour = region->owner->colour;

                      // Fill provinces
                      for (const auto &prov : region->provinces) {
                        for (const auto pix : prov->getNonOwningPixelView()) {
                          countryBmp.imageData[pix] = countryColour;
                        }
                      }
                    } else {
                      for (const auto &prov : region->provinces) {
                        for (const auto pix : prov->getNonOwningPixelView()) {
                          countryBmp.imageData[pix] = worldMap[pix];
                        }
                      }
                    }
                  });

    Png::save(countryBmp, Fwg::Cfg::Values().mapsPath +
                              "countries/countries_no_borders.png");
    std::vector<std::shared_ptr<Country>> countriesVector;
    for (auto const &country : countries) {
      countriesVector.push_back(country.second);
    }
    Fwg::Gfx::applyAreaBorders(countryBmp, countriesVector);
    Png::save(countryBmp,
              Fwg::Cfg::Values().mapsPath + "countries/countries_borders.png");
    Fwg::Gfx::applyAreaBorders(countryBmp, ardaRegions);
    Png::save(countryBmp, Fwg::Cfg::Values().mapsPath +
                              "countries/countries_state_borders.png");
  }
  return countryBmp;
}

void ArdaGen::totalResourceVal(const std::vector<float> &resPrev,
                               float resourceModifier,
                               const Arda::Utils::ResConfig &resourceConfig) {
  const auto baseResourceAmount = resourceModifier;
  auto totalRes = 0.0;
  for (auto &val : resPrev) {
    totalRes += val;
  }
  for (auto &reg : ardaRegions) {
    auto resShare = 0.0;
    for (const auto &prov : reg->provinces) {
      for (const auto pix : prov->getNonOwningPixelView()) {
        resShare += resPrev[pix];
      }
    }
    // basically fictive value from given input of how often this resource
    // appears
    auto stateRes = baseResourceAmount * (resShare / totalRes);
    // round to whole number
    stateRes = std::round(stateRes);
    reg->resources.insert(
        {resourceConfig.name,
         {resourceConfig.name, resourceConfig.capped, stateRes}});
  }
}

void ArdaGen::gatherStatistics() {
  // first gather all resources
  ardaStats.totalResources.clear();
  ardaStats.totalWorldPopulation = 0;
  ardaStats.totalWorldGdp = 0;
  for (const auto &reg : ardaRegions) {
    ardaStats.totalWorldPopulation += reg->totalPopulation;
    ardaStats.totalWorldGdp += reg->gdp;
    for (const auto &res : reg->resources) {
      if (ardaStats.totalResources.find(res.first) ==
          ardaStats.totalResources.end()) {
        ardaStats.totalResources[res.first] = 0;
      }
      ardaStats.totalResources[res.first] += res.second.amount;
    }
  }
}

void ArdaGen::printStatistics() {
  Logging::logLine("Printing Statistics");
  gatherStatistics();
  std::map<std::string, int> countryPop;
  for (auto &c : countries) {
    countryPop[c.first] = 0;
    for (auto &gR : c.second->ownedRegions) {
      countryPop[c.first] += gR->totalPopulation;
    }
  }
  for (auto &c : countries) {
    Logging::logLine("Country: ", c.first,
                     " Population: ", countryPop[c.first]);
  }
}
void ArdaGen::writeTextFiles() {}
void ArdaGen::writeLocalisation() {}
void ArdaGen::writeImages() {}

void ArdaGen::save(const std::string &path) {
  std::ofstream file(path, std::ios::binary);
  boost::archive::binary_oarchive ar(file);
  ar << areaData << terrainData << climateData;
  ar << climateMap << worldMap << segmentMap << provinceMap << regionMap;
  ar << locationMap << navmeshMap << errorMap;
  ar << preModifyHeightMap << preModifyHumidityMap;
  ar << ardaContinents << ardaRegions << ardaProvinces << superRegions;
  ar << countries << civData << nData;
  ar << typeMap << countryMap << superRegionMap;
  ar << ardaData.civLayer;
}

void ArdaGen::load(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  boost::archive::binary_iarchive ar(file);
  resetData();
  ar >> areaData >> terrainData >> climateData;
  ar >> climateMap >> worldMap >> segmentMap >> provinceMap >> regionMap;
  ar >> locationMap >> navmeshMap >> errorMap;
  ar >> preModifyHeightMap >> preModifyHumidityMap;
  ar >> ardaContinents >> ardaRegions >> ardaProvinces >> superRegions;
  ar >> countries >> civData >> nData;
  ar >> typeMap >> countryMap >> superRegionMap;
  ar >> ardaData.civLayer;
  mapProvinces();
  mapRegions();
  mapContinents();
}

} // namespace Arda