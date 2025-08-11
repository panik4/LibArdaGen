#include "flags/Flag.h"

namespace Arda::Gfx {
using namespace Fwg;
namespace PU = Fwg::Parsing;
std::map<std::string, std::vector<Fwg::Gfx::Colour>> Flag::colourGroups;
std::vector<std::vector<std::vector<int>>> Flag::flagTypes(7);
std::vector<std::vector<std::vector<std::string>>> Flag::flagTypeColours(7);
std::vector<std::vector<uint8_t>> Flag::flagTemplates;
std::vector<std::vector<uint8_t>> Flag::symbolTemplates;
std::vector<FlagInfo> Flag::flagMetadata;
std::vector<SymbolInfo> Flag::symbolMetadata;
Flag::Flag() {}

Fwg::Gfx::Colour
pickDistinctColour(const std::vector<Fwg::Gfx::Colour> &pool,
                   const std::vector<Fwg::Gfx::Colour> &existing,
                   int minDistance, int maxAttempts = 50) {
  if (pool.empty()) {
    throw std::runtime_error("pickDistinctColour: colour pool is empty");
  }

  Fwg::Gfx::Colour chosen;
  bool valid = false;
  int attempts = 0;

  do {
    chosen = pool[RandNum::getRandom(pool.size())];
    valid = true;
    for (const auto &e : existing) {
      if (chosen.distance(e) < minDistance) {
        valid = false;
        break;
      }
    }
  } while (!valid && ++attempts < maxAttempts);

  return chosen;
}

Flag::Flag(const int width, const int height) : width(width), height(height) {
  image = std::vector<unsigned char>(width * height * 4, 0);
  auto randomIndex = RandNum::getRandom(flagTemplates.size());
  image = flagTemplates[randomIndex];
  const auto &flagInfo = flagMetadata[randomIndex];
  // get the template and map all colours to indices
  std::map<Fwg::Gfx::Colour, std::vector<int>> colourMapping;
  for (auto i = 0; i < image.size(); i += 4) {
    Fwg::Gfx::Colour temp(image[i + 2], image[i + 1], image[i]);
    colourMapping[temp].push_back(i);
  }
  // determine replacements for the colours in the template.
  // pool of colours is taken from colour groups defined in metadata files
  std::vector<Fwg::Gfx::Colour> replacementColours;
  const int minDistance = 90;

  for (auto &colGroup : flagInfo.flagColourGroups) {
    const auto &pool = colourGroups[colGroup];
    auto chosen = pickDistinctColour(pool, replacementColours, minDistance);
    replacementColours.push_back(chosen);
  }

  // now convert the old colours to the replacement colours
  // alpha values stay the same
  int colIndex = 0;
  for (const auto &mapping : colourMapping) {
    for (auto index : mapping.second)
      setPixel(replacementColours[colIndex], index);
    colIndex++;
  }

  if (flagInfo.applySymbol) {
    // now load symbol templates
    randomIndex = RandNum::getRandom(symbolTemplates.size());
    auto symbol{symbolTemplates[randomIndex]};
    auto symbolInfo{symbolMetadata[randomIndex]};

    // now resize symbol
    int newSize = static_cast<int>(52.0 * flagInfo.reductionFactor);
    symbol = Flag::resize(newSize, newSize, symbol, 52, 52);

    replacementColours.clear();
    // 1. Build colour mapping from resized symbol
    colourMapping.clear();
    for (auto i = 0; i < symbol.size(); i += 4) {
      if (symbol[i + 3] > 0) { // only non-transparent pixels
        Fwg::Gfx::Colour temp(symbol[i + 2], symbol[i + 1], symbol[i]);
        colourMapping[temp].push_back(i);
      }
    }

    // 2. Determine final colours for each mapping
    std::vector<Fwg::Gfx::Colour> finalColours;
    if (symbolInfo.replaceColour) {
      // Build avoid list from current flag pixels
      std::vector<Fwg::Gfx::Colour> avoidColours;
      for (size_t i = 0; i < image.size(); i += 4) {
        if (image[i + 3] > 0) { // only non-transparent
          Fwg::Gfx::Colour c(image[i + 2], image[i + 1], image[i]);
          bool tooClose = false;
          for (const auto &ec : avoidColours) {
            if (c.distance(ec) <
                minDistance) { // same threshold as pickDistinctColour
              tooClose = true;
              break;
            }
          }
          if (!tooClose)
            avoidColours.push_back(c);
        }
      }

      // Now pick symbol colours ensuring they are distinct
      for (const auto &colGroup : flagInfo.symbolColourGroups) {
        const auto &pool = colourGroups[colGroup];
        auto chosen = pickDistinctColour(pool, avoidColours, minDistance);
        finalColours.push_back(chosen);
        avoidColours.push_back(chosen); // also avoid same symbol colour twice
      }
    } else {
      // Use original colours from symbol (BGR ordering)
      for (const auto &mapping : colourMapping) {
        int index = mapping.second.front();
        Fwg::Gfx::Colour original(symbol[index + 2], // R
                                  symbol[index + 1], // G
                                  symbol[index]);    // B
        finalColours.push_back(original);
      }
    }

    // 3. Helper: blend pixel onto the flag
    auto blendPixel = [&](const Fwg::Gfx::Colour &src, unsigned char alpha,
                          int targetIndex) {
      if (alpha > 127) { // or alpha > 0 if you want any visibility
        image[targetIndex] = src.getBlue();
        image[targetIndex + 1] = src.getGreen();
        image[targetIndex + 2] = src.getRed();
        image[targetIndex + 3] = 255;
      } else {
      }
    };

    // 4. Apply the symbol onto the flag
    colIndex = 0;
    const int lineSize = 328;
    const int symbolLineSize = 52 * 4;

for (const auto &mapping : colourMapping) {
      const auto &colourToUse = finalColours[colIndex];
      for (auto byteIndex : mapping.second) {
        int pixelIndexInSymbol = byteIndex / 4;
        int symbolWidthPixels = static_cast<int>(52 * flagInfo.reductionFactor);

        int symY = pixelIndexInSymbol / symbolWidthPixels;
        int symX = pixelIndexInSymbol % symbolWidthPixels;

        int dstY = static_cast<int>(flagInfo.symbolHeightOffset * 52) + symY;
        int dstX =
            static_cast<int>(flagInfo.symbolWidthOffset * this->width) + symX;

        if (dstY < 0 || dstY >= this->height)
          continue;
        if (dstX < 0 || dstX >= this->width)
          continue;

        int dstByteIndex = (dstY * this->width + dstX) * 4;

        uint8_t alpha = symbol[byteIndex + 3];
        blendPixel(colourToUse, alpha, dstByteIndex);
      }
      colIndex++;
    }

  }
  return;
}

Flag::~Flag() {}

void Flag::setPixel(const Fwg::Gfx::Colour colour, const int x, const int y) {
  if (Utils::inRange(0, width * height * 4 + 3, (x * width + y) * 4 + 3)) {
    for (auto i = 0; i < 3; i++)
      image[(x * width + y) * 4 + i] = colour.getBGR()[i];
    image[(x * width + y) * 4 + 3] = 255;
  }
}

void Flag::setPixel(const Fwg::Gfx::Colour colour, const int index) {
  if (Utils::inRange(0, width * height * 4 + 3, index)) {
    for (auto i = 0; i < 3; i++)
      image[index + i] = colour.getBGR()[i];
    image[index + 3] = 255;
  }
}

std::vector<unsigned char> Flag::getFlag() const { return image; }

std::vector<uint8_t> Flag::resize(const int width, const int height) const {
  auto resized = std::vector<unsigned char>(width * height * 4, 0);
  const auto factor = this->width / width;
  for (auto h = 0; h < height; h++) {
    for (auto w = 0; w < width; w++) {
      auto colourmapIndex = factor * h * this->width + factor * w;
      colourmapIndex *= 4;
      resized[(h * width + w) * 4] = image[colourmapIndex];
      resized[(h * width + w) * 4 + 1] = image[colourmapIndex + 1];
      resized[(h * width + w) * 4 + 2] = image[colourmapIndex + 2];
      resized[(h * width + w) * 4 + 3] = image[colourmapIndex + 3];
    }
  }
  return resized;
}

void Flag::flip() {
  for (auto i = 0; i < height / 2; i++) {
    for (auto w = 0; w < width; w++) {
      auto abseIndex = 4 * (i * width + w);
      auto otherIndex = image.size() - abseIndex - 4 * (width - w) + 4 * w;
      for (int x = 0; x < 4; x++) {

        auto temp = image[abseIndex + x];
        image[abseIndex + x] = image[otherIndex + x];
        image[otherIndex + x] = temp;
      }
    }
  }
}

std::vector<uint8_t> Flag::resize(const int width, const int height,
                                  const std::vector<unsigned char> &tImage,
                                  const int inWidth, const int inHeight) {
  const int stepSize = 4; // RGBA
  double xFactor = static_cast<double>(inWidth) / width;
  double yFactor = static_cast<double>(inHeight) / height;

  std::vector<uint8_t> resized(width * height * stepSize);

  for (int y = 0; y < height; ++y) {
    int srcY = std::min<int>(inHeight - 1, static_cast<int>(y * yFactor));
    for (int x = 0; x < width; ++x) {
      int srcX = std::min<int>(inWidth - 1, static_cast<int>(x * xFactor));

      int srcIndex = (srcY * inWidth + srcX) * stepSize;
      int dstIndex = (y * width + x) * stepSize;

      // Copy RGBA directly
      for (int i = 0; i < stepSize; ++i) {
        resized[dstIndex + i] = tImage[srcIndex + i];
      }
    }
  }
  return resized;
}

void Flag::readColourGroups() {
  auto lines = PU::getLines(Fwg::Cfg::Values().resourcePath +
                            "flags//colour_groups.txt");
  for (const auto &line : lines) {
    if (!line.size())
      continue;
    auto tokens = PU::getTokens(line, ';');
    for (auto i = 1; i < tokens.size(); i++) {
      const auto nums =
          Arda::Parsing::getNumbers(tokens[i], ',', std::set<int>{});
      Fwg::Gfx::Colour c{(unsigned char)nums[0], (unsigned char)nums[1],
                         (unsigned char)nums[2]};
      colourGroups[tokens[0]].push_back(c);
    }
  }
}

void Flag::readFlagTypes() {
  auto lines =
      PU::getLines(Fwg::Cfg::Values().resourcePath + "flags//flag_types.txt");
  for (const auto &line : lines) {
    if (!line.size())
      continue;
    auto tokens = PU::getTokens(line, ';');
    const auto flagType = stoi(tokens[0]);
    const auto flagTypeID = flagTypes[flagType].size();
    const auto symbols = PU::getTokens(tokens[1], ',');
    const auto colourGroupStrings = PU::getTokens(tokens[2], ',');
    flagTypes[flagType].push_back(std::vector<int>{});
    flagTypeColours[flagType].push_back(std::vector<std::string>{});
    for (const auto &symbolRange : symbols) {
      const auto &rangeTokens =
          Arda::Parsing::getNumbers(symbolRange, '-', std::set<int>{});
      for (auto x = rangeTokens[0]; x <= rangeTokens[1]; x++)
        flagTypes[flagType][flagTypeID].push_back(x);
    }
    for (const auto &cGroup : colourGroupStrings)
      flagTypeColours[flagType][flagTypeID].push_back(cGroup);
  }
}

void Flag::readFlagTemplates() {
  for (auto i = 0; i < 100; i++) {
    if (std::filesystem::exists(Fwg::Cfg::Values().resourcePath +
                                "flags//flag_presets//" + std::to_string(i) +
                                ".tga")) {
      flagTemplates.push_back(Gfx::Textures::readTGA(
          Fwg::Cfg::Values().resourcePath + "flags//flag_presets//" +
          std::to_string(i) + ".tga"));
      // get line and immediately tokenize it
      auto tokens = PU::getTokens(PU::getLines(Fwg::Cfg::Values().resourcePath +
                                               "flags//flag_presets//" +
                                               std::to_string(i) + ".txt")[0],
                                  ';');
      bool applySymbol = tokens[2] == "true";
      flagMetadata.push_back(
          {PU::getTokens(tokens[0], ','), PU::getTokens(tokens[1], ','),
           applySymbol, stod(tokens[3]), stod(tokens[4]), stod(tokens[5])});
    }
  }
}
void Flag::readSymbolTemplates() {
  for (int i = 0; i < 100; i++) {
    if (std::filesystem::exists(Fwg::Cfg::Values().resourcePath +
                                "flags//symbol_presets//" + std::to_string(i) +
                                ".tga")) {
      symbolTemplates.push_back(Gfx::Textures::readTGA(
          Fwg::Cfg::Values().resourcePath + "flags//symbol_presets//" +
          std::to_string(i) + ".tga"));
      // get line and immediately tokenize it
      auto tokens = PU::getTokens(PU::getLines(Fwg::Cfg::Values().resourcePath +
                                               "flags//symbol_presets//" +
                                               std::to_string(i) + ".txt")[0],
                                  ';');
      symbolMetadata.push_back({tokens[0] == "true"});
    }
  }
}
} // namespace Arda::Gfx