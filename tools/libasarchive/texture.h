#pragma once

#include "../../src/gfx/gfx.h"
#include "../../src/gfx/gfx_format.h"
#include "../../src/gfx/gfx_image.h"

#include "../../src/third_party/stb_image/stb_image.h"

#include "archive.h"

namespace as::archive {

GfxFormat textureFormatFromString(const std::string& str);

std::string textureFormatToString(GfxFormat format);


struct TextureDesc {
  GfxFormat format = GfxFormat::eUnknown;
  std::string name;
  bool enableMips = true;
  bool enableCube = false;
  bool enableLayers = false;
  bool allowCompression = true;
  bool allowBc7 = false;
};


struct TextureImageDesc {
  GfxFormat format = GfxFormat::eUnknown;
  uint32_t w = 0;
  uint32_t h = 0;
  uint32_t channels = 0;
  size_t dataSize = 0;
};


class TextureImage {

public:

  TextureImage() { }

  TextureImage(
    const TextureImageDesc&             desc,
          void*                         data)
  : m_desc(desc), m_data(data) { }

  TextureImage(TextureImage&& other)
  : m_desc(other.m_desc)
  , m_data(other.m_data) {
    other.m_desc = TextureImageDesc();
    other.m_data = nullptr;
  }

  TextureImage& operator = (TextureImage&& other) {
    m_desc = other.m_desc;
    m_data = other.m_data;

    other.m_desc = TextureImageDesc();
    other.m_data = nullptr;
    return *this;
  }

  ~TextureImage() {
    if (m_data)
      stbi_image_free(m_data);
  }

  TextureImageDesc getDesc() const {
    return m_desc;
  }

  Vector4D load(uint32_t x, uint32_t y) const;

  uint32_t loadRaw(uint32_t x, uint32_t y) const;

  void store(uint32_t x, uint32_t y, Vector4D color);

  uint8_t* at(size_t offset);

  const uint8_t* at(size_t offset) const;

  operator bool () const {
    return m_data != nullptr;
  }

private:

  TextureImageDesc  m_desc = { };
  void*             m_data = nullptr;

};


class TextureBuildJob : public BuildJob {

public:

  TextureBuildJob(
          Environment                   env,
    const TextureDesc&                  desc,
          std::vector<std::filesystem::path> inputs);

  ~TextureBuildJob();

  std::pair<BuildResult, ArchiveFile> build() override;

private:

  Environment               m_env;
  TextureDesc               m_desc;

  std::vector<std::filesystem::path> m_inputs;

  std::vector<TextureImage> m_subresourceImages;
  std::vector<TextureImage> m_encodedImages;
  std::vector<ArchiveData>  m_compressedData;
  std::vector<size_t>       m_rawSizes;

  void generateMip(
    const GfxFormatInfo&                formatInfo,
          TextureImage*                 dstImage,
    const TextureImage*                 srcImage,
          uint32_t                      row);

  void encodeBlocks(
    const GfxFormatInfo&                formatInfo,
          TextureImage*                 dstImage,
    const TextureImage*                 srcImage,
          uint32_t                      row);

  bool compressChunk(
    const GfxTextureDesc&               metadata,
          uint32_t                      dataIndex);

  TextureImage loadImage(
    const std::filesystem::path&        path);

  GfxFormat pickFormat(
    const TextureImage&                 texture) const;

  uint32_t computeSubresourceIndex(
    const GfxTextureDesc&               metadata,
          uint32_t                      mip,
          uint32_t                      layer) const;

  uint32_t computeDataIndex(
    const GfxTextureDesc&               metadata,
          uint32_t                      mip,
          uint32_t                      layer) const;

};

}
