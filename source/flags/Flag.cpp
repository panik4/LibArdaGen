#include "flags/Flag.h"
#include <cmath>
#include <limits>

namespace Arda::Gfx {
namespace {
using Colour = Fwg::Gfx::Colour;
namespace PU = Fwg::Parsing;
Colour pixelColour(const std::vector<uint8_t> &image, size_t i) {
  return Colour(image[i + 2], image[i + 1], image[i]);
}
size_t imageSize(int width, int height) {
  if (width <= 0 || height <= 0 ||
      static_cast<size_t>(width) > std::numeric_limits<size_t>::max() / 4 / height)
    throw std::runtime_error("Flag: invalid image dimensions");
  return static_cast<size_t>(width) * height * 4;
}
Colour pickDistinctColour(const std::vector<Colour> &pool,
                          const std::vector<Colour> &existing) {
  if (pool.empty()) throw std::runtime_error("Flag: empty colour pool");
  std::vector<Colour> valid, best;
  int bestDistance = -1;
  for (const auto &c : pool) {
    int distance = 765;
    for (const auto &e : existing) distance = std::min(distance, c.distance(e));
    if (distance >= 90) valid.push_back(c);
    if (distance > bestDistance) { best.clear(); bestDistance = distance; }
    if (distance == bestDistance) best.push_back(c);
  }
  const auto &choices = valid.empty() ? best : valid;
  return choices[RandNum::getRandom(choices.size())];
}
std::string trim(std::string s) {
  const std::string whitespace = std::string(" ") + char(9) + char(13) + char(10);
  const auto start = s.find_first_not_of(whitespace);
  return start == std::string::npos ? "" :
      s.substr(start, s.find_last_not_of(whitespace) - start + 1);
}
double number(const std::string &s) {
  size_t end;
  const auto value = std::stod(trim(s), &end);
  if (end != trim(s).size() || !std::isfinite(value))
    throw std::runtime_error("invalid finite number: " + s);
  return value;
}
Colour parseColour(const std::string &s) {
  auto fields = PU::getTokens(s, ',');
  if (fields.size() != 3) throw std::runtime_error("RGB requires three channels: " + s);
  int rgb[3];
  for (int i = 0; i < 3; ++i) {
    auto n = number(fields[i]);
    if (n < 0 || n > 255 || n != std::floor(n))
      throw std::runtime_error("invalid RGB channel: " + fields[i]);
    rgb[i] = static_cast<int>(n);
  }
  return Colour(rgb[0], rgb[1], rgb[2]);
}
bool boolean(const std::string &s) {
  if (trim(s) == "true") return true;
  if (trim(s) == "false") return false;
  throw std::runtime_error("invalid boolean: " + s);
}
std::vector<std::string> metadata(const std::filesystem::path &path, size_t count) {
  auto lines = PU::getLines(path.string());
  std::vector<std::string> data;
  for (const auto &line : lines)
    if (!trim(line).empty()) data.push_back(line);
  if (data.size() != 1) throw std::runtime_error("expected one metadata line");
  auto fields = PU::getTokens(data[0], ';');
  for (auto &f : fields) f = trim(f);
  if (fields.size() != count) throw std::runtime_error("incorrect metadata field count");
  return fields;
}
std::vector<uint8_t> readTemplate(const std::filesystem::path &path, int w, int h) {
  // readTGA returns pixels only, so verify dimensions from the TGA header too.
  std::ifstream file(path, std::ios::binary);
  unsigned char header[18]{};
  if (!file.read(reinterpret_cast<char *>(header), 18) ||
      (header[12] + 256 * header[13]) != w ||
      (header[14] + 256 * header[15]) != h)
    throw std::runtime_error("incorrect TGA dimensions: " + path.string());
  auto pixels = Textures::readTGA(path.string());
  if (pixels.size() != imageSize(w, h)) throw std::runtime_error("incorrect pixel buffer");
  return pixels;
}
}
bool Flag::resourcesLoaded = false;
std::map<std::string, std::vector<Fwg::Gfx::Colour>> Flag::colourGroups;
std::vector<Flag::BackgroundTemplate> Flag::flagTemplates;
std::vector<Flag::SymbolTemplate> Flag::symbolTemplates;

Flag::Flag(int width, int height, Arda::Utils::Ideology ideology) : width(width), height(height) {
  if (width != 82 || height != 52) throw std::runtime_error("Flag generation requires 82x52");
  if (!resourcesLoaded) throw std::runtime_error("Flag resources have not been initialized");
  const auto &background = flagTemplates[RandNum::getRandom(flagTemplates.size())];
  image = background.image;
  recolourBackground(background.info);
  if (background.info.applySymbol) compositeSymbol(background.info, ideology);
}
void Flag::recolourBackground(const FlagInfo &info) {
  std::map<Colour, Colour> replacements;
  std::vector<Colour> selected;
  for (const auto &[source, group] : info.flagColourGroups) {
    auto chosen = pickDistinctColour(colourGroups.at(group), selected);
    replacements.emplace(source, chosen);
    selected.push_back(chosen);
  }
  for (size_t i = 0; i < image.size(); i += 4) {
    setPixel(replacements.at(pixelColour(image, i)), static_cast<int>(i));
    image[i + 3] = 255;
  }
}
void Flag::compositeSymbol(const FlagInfo &info, Arda::Utils::Ideology ideology) {
  std::vector<const SymbolTemplate *> eligible;
  for (const auto &symbol : symbolTemplates)
    if (symbol.info.allowedIdeologies.contains(ideology))
      eligible.push_back(&symbol);
  if (eligible.empty()) return;
  const auto &entry = *eligible[RandNum::getRandom(eligible.size())];
  const int size = static_cast<int>(52 * info.reductionFactor);
  const auto symbol = resize(size, size, entry.image, 52, 52);
  Colour replacement;
  if (entry.info.replaceColour) {
    std::vector<Colour> existing;
    for (size_t i = 0; i < image.size(); i += 4) {
      auto c = pixelColour(image, i);
      if (std::find(existing.begin(), existing.end(), c) == existing.end())
        existing.push_back(c);
    }
    replacement = pickDistinctColour(colourGroups.at(info.symbolColourGroups.front()), existing);
  }
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      int dx = static_cast<int>(info.symbolWidthOffset * width) + x;
      int dy = static_cast<int>(info.symbolHeightOffset * height) + y;
      if (dx < 0 || dy < 0 || dx >= width || dy >= height) continue;
      const size_t src = (static_cast<size_t>(y) * size + x) * 4;
      const size_t dst = (static_cast<size_t>(dy) * width + dx) * 4;
      const auto colour = entry.info.replaceColour ? replacement : pixelColour(symbol, src);
      const int alpha = symbol[src + 3];
      for (int channel = 0; channel < 3; ++channel)
        image[dst + channel] = static_cast<uint8_t>(
            (colour.getBGR()[channel] * alpha + image[dst + channel] * (255 - alpha) + 127) / 255);
      image[dst + 3] = 255;
    }
  }
}
void Flag::setPixel(Colour colour, int x, int y) {
  if (x < 0 || y < 0 || x >= width || y >= height) return;
  const size_t index = (static_cast<size_t>(y) * width + x) * 4;
  if (index > static_cast<size_t>(std::numeric_limits<int>::max())) return;
  setPixel(colour, static_cast<int>(index));
}
void Flag::setPixel(Colour colour, int index) {
  if (index < 0 || index % 4 || image.size() < 4 ||
      static_cast<size_t>(index) > image.size() - 4) return;
  for (int c = 0; c < 3; ++c) image[index + c] = colour.getBGR()[c];
}
std::vector<uint8_t> Flag::getFlag() const { return image; }
std::vector<uint8_t> Flag::resize(int width, int height) const {
  return resize(width, height, image, this->width, this->height);
}
std::vector<uint8_t> Flag::resize(int width, int height,
                                const std::vector<uint8_t> &input, int inWidth, int inHeight) {
  const size_t outputSize = imageSize(width, height);
  if (input.size() != imageSize(inWidth, inHeight))
    throw std::runtime_error("Flag resize: source buffer size mismatch");
  std::vector<uint8_t> output(outputSize);
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x) {
      const auto sy = static_cast<size_t>(y) * inHeight / height;
      const auto sx = static_cast<size_t>(x) * inWidth / width;
      const auto src = (sy * inWidth + sx) * 4;
      const auto dst = (static_cast<size_t>(y) * width + x) * 4;
      std::copy_n(input.begin() + src, 4, output.begin() + dst);
    }
  return output;
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


void Flag::loadResources(const std::string &resourcePath) {
  if (resourcesLoaded) return;
  decltype(colourGroups) groups;
  decltype(flagTemplates) backgrounds;
  decltype(symbolTemplates) symbols;
  const auto root = std::filesystem::path(resourcePath) / "flags";
  auto path = root / "colour_groups.txt";
  try {
    for (const auto &line : PU::getLines(path.string())) {
      if (trim(line).empty()) continue;
      auto fields = PU::getTokens(line, ';');
      if (fields.size() < 2 || trim(fields[0]).empty())
        throw std::runtime_error("palette requires a name and colours");
      auto name = trim(fields[0]);
      if (groups.count(name)) throw std::runtime_error("duplicate palette: " + name);
      for (size_t i = 1; i < fields.size(); ++i)
        groups[name].push_back(parseColour(fields[i]));
    }
    auto requireGroup = [&](const std::string &name) {
      if (!groups.count(name) || groups.at(name).empty())
        throw std::runtime_error("unknown or empty palette: " + name);
    };
    for (int i = 0; i < 100; ++i) {
      path = root / "flag_presets" / (std::to_string(i) + ".tga");
      if (!std::filesystem::exists(path)) continue;
      auto pixels = readTemplate(path, 82, 52);
      path.replace_extension(".txt");
      auto fields = metadata(path, 6);
      FlagInfo info{};
      for (const auto &mapping : PU::getTokens(fields[0], '|')) {
        auto pair = PU::getTokens(mapping, '=');
        if (pair.size() != 2) throw std::runtime_error("expected explicit RGB=palette mapping");
        auto group = trim(pair[1]);
        requireGroup(group);
        if (!info.flagColourGroups.emplace(parseColour(pair[0]), group).second)
          throw std::runtime_error("duplicate RGB mapping");
      }
      for (size_t p = 0; p < pixels.size(); p += 4)
        if (!info.flagColourGroups.count(pixelColour(pixels, p)))
          throw std::runtime_error("missing RGB mapping for " + pixelColour(pixels, p).toString());
      info.applySymbol = boolean(fields[2]);
      info.symbolColourGroups = {fields[1]};
      requireGroup(fields[1]);
      info.symbolWidthOffset = number(fields[3]);
      info.symbolHeightOffset = number(fields[4]);
      info.reductionFactor = number(fields[5]);
      if (info.symbolWidthOffset < 0 || info.symbolWidthOffset > 1 ||
          info.symbolHeightOffset < 0 || info.symbolHeightOffset > 1 ||
          info.reductionFactor < 1.0 / 52 || info.reductionFactor > 1)
        throw std::runtime_error("symbol offsets must be in [0,1], size in [1/52,1]");
      backgrounds.push_back({std::move(pixels), std::move(info)});
    }
    for (int i = 0; i < 100; ++i) {
      path = root / "symbol_presets" / (std::to_string(i) + ".tga");
      if (!std::filesystem::exists(path)) continue;
      auto pixels = readTemplate(path, 52, 52);
      path.replace_extension(".txt");
      auto fields = metadata(path, 2);
      SymbolInfo info{boolean(fields[0]), {}};
      for (const auto &token : PU::getTokens(fields[1], ',')) {
        const auto name = trim(token);
        const auto found = Arda::Utils::stringToIdeology.find(name);
        if (found == Arda::Utils::stringToIdeology.end())
          throw std::runtime_error("unknown allowed ideology: " + name);
        info.allowedIdeologies.insert(found->second);
      }
      if (info.allowedIdeologies.empty())
        throw std::runtime_error("allowed ideologies must not be empty");
      symbols.push_back({std::move(pixels), std::move(info)});
    }
    if (backgrounds.empty()) throw std::runtime_error("no backgrounds found");
  } catch (const std::exception &e) {
    throw std::runtime_error("Flag resource " + path.string() + ": " + e.what());
  }
  colourGroups = std::move(groups);
  flagTemplates = std::move(backgrounds);
  symbolTemplates = std::move(symbols);
  resourcesLoaded = true;
}
} // namespace Arda::Gfx
