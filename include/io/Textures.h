#pragma once
#include "FastWorldGenerator.h"
#include "gli/gli.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace Arda::Gfx::Textures {
void writeDDS(int width, int height, const std::vector<uint8_t> &pixelData,
              const std::string &path, gli::format format);
void writeMipMapDDS(
    int width, int height, const std::vector<uint8_t> &pixelData,
    const std::string &path, gli::format format = gli::FORMAT_BGR8_UNORM_PACK8,
    bool compress = false,
    gli::format compressedFormat = gli::FORMAT_RGBA_DXT1_UNORM_BLOCK8);
void writeTGA(const int width, const int height, std::vector<uint8_t> pixelData,
              const std::string &path, bool flipVertically = true);
std::vector<uint8_t> readTGA(const std::string &path);
std::vector<uint8_t> readDDS(const std::string &path);
}; // namespace Arda::Gfx::Textures
