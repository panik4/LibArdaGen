#pragma once
#include "DirectXTex.h"
#include "FastWorldGenerator.h"

namespace Arda::Gfx::Textures {
void writeDDS(const int width, const int height,
              std::vector<uint8_t> &pixelData, const DXGI_FORMAT format,
              const std::string &path);
void writeMipMapDDS(const int width, const int height,
              std::vector<uint8_t> &pixelData, const DXGI_FORMAT format,
                    const std::string &path, bool compress=false);
void writeTGA(const int width, const int height, std::vector<uint8_t> pixelData,
              const std::string &path);
std::vector<uint8_t> readTGA(const std::string &path);
std::vector<uint8_t> readDDS(const std::string &path);
}; // namespace Arda::Gfx::Textures
