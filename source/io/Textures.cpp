#include "io/Textures.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

#define HRESULT_E_NOT_SUPPORTED static_cast<HRESULT>(0x80070032L)
namespace Arda::Gfx::Textures {
void writeDDS(int width, int height, const std::vector<uint8_t> &pixelData,
              const std::string &path, gli::format format) {
  // Create a 2D texture with a single mip level
  gli::texture2d tex(format, gli::extent2d(width, height), 1);

  // Copy pixel data into level 0
  std::memcpy(tex[0].data(), pixelData.data(), pixelData.size());

  // Save the texture as DDS
  gli::save_dds(tex, path);
}

void writeMipMapDDS(int width, int height,
                    const std::vector<uint8_t> &pixelData,
                    const std::string &path, gli::format format, bool compress,
                    gli::format compressedFormat) {
  // Create texture
  const int levels =
      1 + static_cast<int>(std::floor(std::log2(std::max(width, height))));
  gli::texture2d tex(format, gli::extent2d(width, height), levels);

  std::memcpy(tex[0].data(), pixelData.data(), pixelData.size());

  // Generate mipmaps on CPU (simple box filter)
  for (int level = 1; level < levels; ++level) {
    auto prevExtent = tex.extent(level - 1);
    auto currExtent = tex.extent(level);
    const uint8_t *src = static_cast<const uint8_t *>(tex[level - 1].data());
    uint8_t *dst = static_cast<uint8_t *>(tex[level].data());

    for (int y = 0; y < currExtent.y; ++y) {
      for (int x = 0; x < currExtent.x; ++x) {
        int dstIdx = (y * currExtent.x + x) * 4;
        int sx = x * 2;
        int sy = y * 2;
        for (int c = 0; c < 4; ++c) {
          int sum = 0;
          for (int dy = 0; dy < 2; ++dy)
            for (int dx = 0; dx < 2; ++dx)
              sum += src[((sy + dy) * prevExtent.x + (sx + dx)) * 4 + c];
          dst[dstIdx + c] = static_cast<uint8_t>(sum / 4);
        }
      }
    }
  }

  // Compress if requested
  if (compress) {
    tex = gli::convert(tex, compressedFormat);
  }

  // Save DDS
  gli::save_dds(tex, path);
}

void flipVertically(std::vector<uint8_t> &pixels, int width, int height) {
  const int rowSize = width * 4;
  std::vector<uint8_t> temp(rowSize);

  for (int y = 0; y < height / 2; ++y) {
    uint8_t *top = pixels.data() + y * rowSize;
    uint8_t *bottom = pixels.data() + (height - y - 1) * rowSize;

    std::memcpy(temp.data(), top, rowSize);
    std::memcpy(top, bottom, rowSize);
    std::memcpy(bottom, temp.data(), rowSize);
  }
}

std::vector<uint8_t> readDDS(const std::string &path) {
  gli::texture tex = gli::load_dds(path);

  if (tex.empty())
    return {};

  gli::texture2d tex2d(tex); // View as 2D

  const auto extent = tex2d.extent(0);
  const size_t size = extent.x * extent.y * 4;

  std::vector<uint8_t> pixels(size);
  std::memcpy(pixels.data(), tex2d[0].data(), size);

  return pixels;
}

void fix_tga_origin_top_left(const std::string &path) {
  std::fstream f(path, std::ios::in | std::ios::out | std::ios::binary);
  if (!f)
    return;

  f.seekp(17); // image descriptor byte
  unsigned char b;
  f.read((char *)&b, 1);

  b |= 0x20; // set top-left origin bit
  f.seekp(17);
  f.write((char *)&b, 1);
}
void writeTGA_Paradox_B32(const std::string &path, int width, int height,
                          std::vector<uint8_t> &bgra) {
  if (bgra.size() != width * height * 4) {
    throw std::runtime_error("writeTGA_Paradox_B32: pixel data size mismatch");
  }

  std::ofstream out(path, std::ios::binary);
  if (!out) {
    throw std::runtime_error("writeTGA_Paradox_B32: cannot open output file");
  }
  flipVertically(bgra, width, height);

  uint8_t header[18] = {};
  header[2] = 2; // uncompressed true-color
  header[12] = width & 0xFF;
  header[13] = (width >> 8) & 0xFF;
  header[14] = height & 0xFF;
  header[15] = (height >> 8) & 0xFF;
  header[16] = 32;   // 32 bits per pixel
  header[17] = 0x20; // top-left origin (bit 5 = 1)

  out.write(reinterpret_cast<char *>(header), 18);
  out.write(reinterpret_cast<const char *>(bgra.data()), bgra.size());
}

void writeTGA(const int width, const int height, std::vector<uint8_t> pixelData,
              const std::string &path, bool flipVertically) {
  // if (pixelData.size() != width * height * 4)
  //   throw std::runtime_error("Invalid pixelData size");

  //// Paradox expects BGRA
  // for (size_t i = 0; i < pixelData.size(); i += 4)
  //   std::swap(pixelData[i], pixelData[i + 2]);

  //// stb vertical flip only affects pixel order, not header
  // stbi_flip_vertically_on_write(flipVertically ? 1 : 0);

  // if (!stbi_write_tga(path.c_str(), width, height, 4, pixelData.data()))
  //   throw std::runtime_error("Failed to write tga: " + path);

  //// Fix header: origin = top-left
  // fix_tga_origin_top_left(path);
  writeTGA_Paradox_B32(path, width, height, pixelData);
}

std::vector<uint8_t> readTGA(const std::string &path) {
  int width = 0, height = 0, n = 0;
  unsigned char *data = stbi_load(path.c_str(), &width, &height, &n, 4);

  if (!data) {
    throw std::runtime_error(std::string("Failed to read TGA: ") +
                             stbi_failure_reason());
  }

  std::vector<uint8_t> out(data, data + width * height * 4);

  // swap R/B to match old DXGI_FORMAT_B8G8R8A8_UNORM
  for (size_t i = 0; i < out.size(); i += 4)
    std::swap(out[i], out[i + 2]);

  stbi_image_free(data);
  return out;
}

} // namespace Arda::Gfx::Textures