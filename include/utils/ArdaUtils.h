#pragma once
#include "FastWorldGenerator.h"
#include <string>
#include <vector>
namespace Arda::Utils {

enum GenerationAge { Medieval, Renaissance, Victorian, WorldWar };

struct AgeConfig {
  double targetWorldPopulation = 0.0;
  double targetWorldGdp = 0.0;
};
static AgeConfig medievalConfig{300'000'000, 1'000'000};
static AgeConfig renaissanceConfig{500'000'000, 8'000'000};
static AgeConfig victorianConfig{1'000'000'000, 300'000'000};
static AgeConfig worldWarConfig{3'000'000'000, 8'000'000'000'000};



static std::map<GenerationAge, AgeConfig> ageConfigs{
    {GenerationAge::Medieval, medievalConfig},
    {GenerationAge::Renaissance, renaissanceConfig},
    {GenerationAge::Victorian, victorianConfig},
    {GenerationAge::WorldWar, worldWarConfig}};

struct NoiseConfig {
  double fractalFrequency;
  double tanFactor;
  double cutOff;
  double mountainBonus;
};
static NoiseConfig defaultNoise{0.02, 0.0, 0.8, 2.0};
static NoiseConfig semiRareNoise{0.015, 0.0, 0.85, 2.0};
static NoiseConfig rareLargePatch{0.005, 0.0, 0.7, 0.0};
static NoiseConfig rareNoise{0.01, 0.0, 0.9, 2.0};
static NoiseConfig agriNoise{0.24, 0.0, 0.0, 0.0};

struct ResConfig {
  std::string name;
  bool capped;
  double resourcePrevalence;
  bool random = false;
  NoiseConfig noiseConfig;
  bool considerClimate = false;
  std::map<Fwg::Climate::Detail::ClimateTypeIndex, double> climateEffects;
  bool considerTrees = false;
  std::map<Fwg::Climate::Detail::TreeTypeIndex, double> treeEffects;
  bool considerSea = false;
  double oceanFactor = 0.0;
  double lakeFactor = 0.0;
};

struct Resource {
  std::string name;
  bool capped;
  double amount;
};

struct Coordinate {
  int x, z;
  double y, rotation;
};

struct Building {
  std::string name;
  Coordinate position;
  // sometimes necessary for special building types
  int relativeID;
  int provinceID;
};

struct UnitStack {
  int type;
  Coordinate position;
};

struct WeatherPosition {
  std::string effectSize;
  Coordinate position;
};
Coordinate strToPos(const std::vector<std::string> &tokens,
                    const std::vector<int> positions);

}; // namespace Arda::Utils
