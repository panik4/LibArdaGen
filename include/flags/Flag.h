#pragma once
#include "FastWorldGenerator.h"
#include "parsing/ArdaParserUtils.h"
#include "io/Textures.h"

namespace Arda::Gfx {
struct FlagInfo {
  std::vector<std::string> flagColourGroups;
  std::vector<std::string> symbolColourGroups;
  double symbolWidthOffset;
  double symbolHeightOffset;
  double reductionFactor;
};

struct SymbolInfo {
  bool replaceColour;
};

class Flag {
  // keep code shorter
  // static cause we only want to read them from file once
  static std::map<std::string, std::vector<Fwg::Gfx::Colour>> colourGroups;
  static std::vector<std::vector<std::vector<int>>> flagTypes;
  static std::vector<std::vector<std::vector<std::string>>> flagTypeColours;
  static std::vector<std::vector<uint8_t>> flagTemplates;
  static std::vector<FlagInfo> flagMetadata;
  static std::vector<SymbolInfo> symbolMetadata;
  static std::vector<std::vector<uint8_t>> symbolTemplates;
  // containers
  std::vector<Fwg::Gfx::Colour> colours;
  std::vector<unsigned char> image;

public:
  // vars
  int width;
  int height;
  // constructors/destructors
  Flag();
  Flag(const int width, const int height);
  ~Flag();
  // methods - image read/write
  void setPixel(const Fwg::Gfx::Colour colour, const int x, const int y);
  void setPixel(const Fwg::Gfx::Colour colour, const int index);
  std::vector<unsigned char> getFlag() const;
  // methods - utils
  std::vector<unsigned char> resize(const int width, const int height) const;
  void flip();
  static std::vector<unsigned char>
  resize(const int width, const int height,
         const std::vector<unsigned char> tImage, const int inWidth,
         const int inHeight);
  // methods - read in configs
  static void readColourGroups();
  static void readFlagTypes();
  static void readFlagTemplates();
  static void readSymbolTemplates();
  
};
} // namespace Arda::Gfx