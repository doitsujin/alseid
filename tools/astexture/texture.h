#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../../src/gfx/gfx.h"
#include "../../src/gfx/gfx_format.h"

#include "../../src/io/io.h"
#include "../../src/io/io_archive.h"

#include "../../src/job/job.h"

using namespace as;

/**
 * \brief Texture arguments
 */
struct TextureArgs {
  std::string name;
  std::vector<std::filesystem::path> files;
  bool enableMips = true;
  bool enableCube = false;
  bool allowCompression = true;
  bool allowBc7 = false;
  GfxFormat format = GfxFormat::eUnknown;
};


/**
 * \brief Texture image data
 */
struct TextureImage {
  /** Image dimensions */
  uint32_t w = 0;
  uint32_t h = 0;
  /** Original channel count, stored data is always RGBA */
  uint32_t channels = 0;
  std::vector<unsigned char> rawData;
  std::vector<unsigned char> encodedData;

  Job mipmapJob;
  Job encodeJob;

  uint32_t get(uint32_t x, uint32_t y) const {
    uint32_t result;
    std::memcpy(&result, &rawData[sizeof(uint32_t) * (w * y + x)], sizeof(uint32_t));
    return result;
  }

  Vector4D getf(uint32_t x, uint32_t y) const {
    constexpr float factor = 1.0f / 255.0f;
    uint32_t offset = sizeof(uint32_t) * (w * y + x);

    return Vector4D(
      rawData[offset + 0], rawData[offset + 1],
      rawData[offset + 2], rawData[offset + 3]) * factor;
  }

  void set(uint32_t x, uint32_t y, uint32_t d) {
    std::memcpy(&rawData[sizeof(uint32_t) * (w * y + x)], &d, sizeof(uint32_t));
  }


  void setf(uint32_t x, uint32_t y, Vector4D f) {
    constexpr float factor = 255.0f;
    uint32_t offset = sizeof(uint32_t) * (w * y + x);

    f *= factor;
    f += 0.5f;

    rawData[offset + 0] = uint8_t(f.at<0>());
    rawData[offset + 1] = uint8_t(f.at<1>());
    rawData[offset + 2] = uint8_t(f.at<2>());
    rawData[offset + 3] = uint8_t(f.at<3>());
  }
};


/**
 * \brief Texture object
 */
class Texture {

public:

  Texture(
          Io                            io,
          Jobs                          jobs,
          TextureArgs                   args);

  ~Texture();

  /**
   * \brief Begins processing the image
   *
   * Loads the image file and dispatches background processing
   * jobs. Fails if the file could not be read.
   * \returns \c false if an error occured
   */
  bool process();

  /**
   * \brief Generates file description
   *
   * Waits for background processing for this texture to
   * complete. Returns \c nullopt if an error occured.
   * \returns File description
   */
  IoArchiveFileDesc getFileDesc();

private:

  Io            m_io;
  Jobs          m_jobs;

  TextureArgs   m_args;

  std::vector<TextureImage> m_images;
  std::vector<char> m_inlineBlob;
  std::vector<char> m_mipTailData;

  GfxFormat     m_format      = GfxFormat::eUnknown;
  GfxFormatInfo m_formatInfo  = GfxFormatInfo();
  Extent2D      m_extent      = Extent2D(0, 0);

  uint32_t      m_arrayLayers = 0;
  uint32_t      m_mipCount    = 0;
  uint32_t      m_mipTail     = 0;
  uint32_t      m_mipTailSize = 0;

  bool readImage(
    const std::filesystem::path&        path,
          TextureImage&                 layer);

  bool readJpg(
    const std::filesystem::path&        path,
          TextureImage&                 layer,
    const void*                         data,
          size_t                        size);

  bool readPng(
    const std::filesystem::path&        path,
          TextureImage&                 layer,
    const void*                         data,
          size_t                        size);

  GfxFormat pickFormat(
    const TextureImage&                 layer) const;

  void waitForCompletion();

  void generateMip(
          TextureImage*                 dstImage,
    const TextureImage*                 srcImage,
          uint32_t                      row);

  void encodeBlocks(
          TextureImage*                 dstImage,
          uint32_t                      row);

};
