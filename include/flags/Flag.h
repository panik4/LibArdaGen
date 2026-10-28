#pragma once
#include "FastWorldGenerator.h"
#include "utils/ArdaUtils.h"
#include <set>
#include "io/Textures.h"
#include "parsing/ArdaParserUtils.h"
#include "utils/SerialisationFwd.h"

namespace Arda::Gfx {
struct FlagInfo {
  std::map<Fwg::Gfx::Colour, std::string> flagColourGroups;
  std::vector<std::string> symbolColourGroups;
  bool applySymbol;
  double symbolWidthOffset;
  double symbolHeightOffset;
  double reductionFactor;
};

struct SymbolInfo {
  bool replaceColour;
  std::set<Arda::Utils::Ideology> allowedIdeologies;
};

class Flag {
  struct BackgroundTemplate {
    std::vector<uint8_t> image;
    FlagInfo info;
  };
  struct SymbolTemplate {
    std::vector<uint8_t> image;
    SymbolInfo info;
  };
  static bool resourcesLoaded;
  static std::map<std::string, std::vector<Fwg::Gfx::Colour>> colourGroups;
  static std::vector<BackgroundTemplate> flagTemplates;
  static std::vector<SymbolTemplate> symbolTemplates;
  void recolourBackground(const FlagInfo &info);
  void compositeSymbol(const FlagInfo &info, Arda::Utils::Ideology ideology);
  // containers
  std::vector<Fwg::Gfx::Colour> colours;
  std::vector<unsigned char> image;

public:
  // vars
  int width = 0;
  int height = 0;
  // constructors/destructors
  Flag() = default;
  Flag(const int width, const int height, Arda::Utils::Ideology ideology);

  // methods - image read/write
  void setPixel(const Fwg::Gfx::Colour colour, const int x, const int y);
  void setPixel(const Fwg::Gfx::Colour colour, const int index);
  std::vector<unsigned char> getFlag() const;
  // methods - utils
  std::vector<unsigned char> resize(const int width, const int height) const;
  void flip();
  static std::vector<unsigned char>
  resize(const int width, const int height,
         const std::vector<unsigned char> &tImage, const int inWidth,
         const int inHeight);
  static void loadResources(const std::string &resourcePath);

  template<class Archive>
  void serialize(Archive &ar, const unsigned int /*version*/) {
    ar & width & height & image & colours;
  }
};
} // namespace Arda::Gfx
