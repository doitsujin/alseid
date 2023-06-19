#include <cmath>
#include <mutex>
#include <utility>

#include "../../src/third_party/bc7enc/bc7enc.h"
#include "../../src/third_party/bc7enc/rgbcx.h"

#include "../../src/third_party/stb_image/stb_image.h"

#include "../../src/util/util_log.h"
#include "../../src/util/util_math.h"
#include "../../src/util/util_types.h"

#include "texture.h"

std::mutex g_globalMutex;

std::atomic<bool> g_bc7encInitialized = { false };


void initBc7Enc() {
  if (g_bc7encInitialized.load())
    return;

  std::lock_guard lock(g_globalMutex);

  if (!g_bc7encInitialized.load()) {
    rgbcx::init();
    bc7enc_compress_block_init();

    g_bc7encInitialized.store(true, std::memory_order_release);
  }
}


float srgbToLinear(float s) {
  if (s <= 0.04045f)
    return s * (1.0f / 12.92f);

  return std::pow(((s + 0.055f) * (1.0 / 1.055f)), 2.4f);
}


Vector4D srgbToLinear(Vector4D s) {
  return Vector4D(
    srgbToLinear(s.at<0>()),
    srgbToLinear(s.at<1>()),
    srgbToLinear(s.at<2>()),
    s.at<3>());
}


float linearToSrgb(float l) {
  if (l <= 0.0031308f)
    return l * 12.92f;

  return 1.055f * std::pow(l, 1.0f / 2.4f) - 0.055f;
}


Vector4D linearToSrgb(Vector4D l) {
  return Vector4D(
    linearToSrgb(l.at<0>()),
    linearToSrgb(l.at<1>()),
    linearToSrgb(l.at<2>()),
    l.at<3>());
}


Texture::Texture(
        Io                            io,
        Jobs                          jobs,
        TextureArgs                   args)
: m_io    (std::move(io))
, m_jobs  (std::move(jobs))
, m_args  (std::move(args)) {

}


Texture::~Texture() {
  waitForCompletion();
}


bool Texture::process() {
  m_images.resize(m_args.files.size());

  for (size_t i = 0; i < m_args.files.size(); i++) {
    if (!readImage(m_args.files[i], m_images[i]))
      return false;
  }

  for (size_t i = 1; i < m_images.size(); i++) {
    if (m_images[i].w != m_images[0].w
     || m_images[i].h != m_images[0].h) {
      Log::err(m_args.files[i], " has different dimensions than ", m_args.files[0]);
      return false;
    }
  }

  // Pick format based on first layer, assume others are the same
  m_extent = Extent2D(m_images[0].w, m_images[0].h);

  m_format = pickFormat(m_images[0]);
  m_formatInfo = Gfx::getFormatInfo(m_format);

  m_arrayLayers = m_images.size();
  m_mipCount = 1;

  if (m_args.enableMips) {
    // If enabled, generate mip maps. This can be multithreaded nicely.
    m_mipCount = findmsb(std::max(m_extent.at<0>(), m_extent.at<1>())) + 1;
    m_images.resize(m_arrayLayers * m_mipCount);

    for (uint32_t i = 0; i < m_arrayLayers; i++) {
      for (uint32_t j = 1; j < m_mipCount; j++) {
        TextureImage* dstImage = &m_images[i + m_arrayLayers * (j - 0)];
        TextureImage* srcImage = &m_images[i + m_arrayLayers * (j - 1)];

        Extent2D mipExtent = gfxComputeMipExtent(m_extent, j);
        dstImage->w = mipExtent.at<0>();
        dstImage->h = mipExtent.at<1>();
        dstImage->channels = srcImage->channels;

        dstImage->rawData.resize(dstImage->w * dstImage->h * sizeof(uint32_t));

        dstImage->mipmapJob = m_jobs->dispatch(m_jobs->create<BatchJob>(
          [this, srcImage, dstImage] (uint32_t row) {
            generateMip(dstImage, srcImage, row);
          }, dstImage->h, 8), srcImage->mipmapJob);
      }
    }
  }

  // Dispatch a job for actually encoding the image, or in case
  // of uncompressed formats, rearrange the data accordingly.
  for (size_t i = 0; i < m_images.size(); i++) {
    TextureImage* dstImage = &m_images[i];

    Extent2D blockCount(dstImage->w, dstImage->h);
    blockCount += m_formatInfo.blockExtent - 1;
    blockCount >>= m_formatInfo.blockExtentLog2;

    dstImage->encodedData.resize(m_formatInfo.planes[0].elementSize * blockCount.at<0>() * blockCount.at<1>());

    if (m_formatInfo.flags & GfxFormatFlag::eCompressed) {
      initBc7Enc();

      dstImage->encodeJob = m_jobs->dispatch(m_jobs->create<BatchJob>(
        [this, dstImage] (uint32_t row) {
          encodeBlocks(dstImage, row);
        }, blockCount.at<1>(), 1), dstImage->mipmapJob);
    } else {
      // TODO implement
    }
  }

  // Find a mip level that's smaller than 64k and mark it as the start of
  // the mip tail. There is no point in splitting up mips even further.
  m_mipTail = m_mipCount;

  for (uint32_t i = 0; i < m_mipCount; i++) {
    Extent2D mipExtent = gfxComputeMipExtent(m_extent, i);
    Extent2D blockCount = (mipExtent + m_formatInfo.blockExtent - 1) >> m_formatInfo.blockExtentLog2;

    uint32_t mipSize = m_formatInfo.planes[0].elementSize * blockCount.at<0>() * blockCount.at<1>();

    if (mipSize < (1u << 16)) {
      m_mipTail = std::min(m_mipTail, i);
      m_mipTailSize += mipSize;
    }
  }

  // Generate inline blob for the texture
  GfxTextureDesc textureDesc;
  textureDesc.type = GfxImageType::e2D;
  textureDesc.format = m_format;
  textureDesc.extent = Extent3D(m_images[0].w, m_images[0].h, 1);
  textureDesc.mips = m_mipCount;
  textureDesc.layers = m_arrayLayers;
  textureDesc.mipTailStart = m_mipTail;
  textureDesc.flags = 0;

  if (m_args.enableCube)
    textureDesc.flags |= GfxTextureFlag::eCubeMap;

  return textureDesc.serialize(Lwrap<WrVectorStream>(m_inlineBlob));
}


IoArchiveFileDesc Texture::getFileDesc() {
  waitForCompletion();

  // Set up file description
  IoArchiveFileDesc desc;
  desc.type = FourCC('T', 'E', 'X', ' ');
  desc.name = m_args.name;

  if (desc.name.empty())
    desc.name = m_args.files[0].stem();

  desc.inlineDataSource.memory = m_inlineBlob.data();
  desc.inlineDataSource.size = m_inlineBlob.size();

  // Order subresources in such a way that mip levels
  // of a single array layer are kept together.
  desc.subFiles.reserve(m_arrayLayers * std::min(m_mipCount, m_mipTail + 1));
  m_mipTailData.resize(m_mipTailSize * m_arrayLayers);

  for (uint32_t i = 0; i < m_arrayLayers; i++) {
    for (uint32_t j = 0; j < m_mipTail; j++) {
      auto& image = m_images[i + m_arrayLayers * j];

      auto& subFile = desc.subFiles.emplace_back();
      subFile.dataSource.memory = image.encodedData.data();
      subFile.dataSource.size = image.encodedData.size();
      subFile.identifier = FourCC(strcat(std::setw(3), std::setfill('0'), std::hex, i, j));
      subFile.compression = IoArchiveCompression::eGDeflate;
    }

    if (m_mipTail < m_mipCount) {
      size_t offset = 0;

      for (uint32_t j = m_mipTail; j < m_mipCount; j++) {
        auto& image = m_images[i + m_arrayLayers * j];

        std::memcpy(&m_mipTailData[m_mipTailSize * i + offset],
          image.encodedData.data(), image.encodedData.size());
        offset += image.encodedData.size();
      }

      auto& subFile = desc.subFiles.emplace_back();
      subFile.dataSource.memory = &m_mipTailData[m_mipTailSize * i];
      subFile.dataSource.size = m_mipTailSize;
      subFile.identifier = FourCC(strcat(std::setw(3), std::setfill('0'), std::hex, i, 'T'));
      subFile.compression = IoArchiveCompression::eGDeflate;
    }
  }

  return desc;
}


bool Texture::readImage(
  const std::filesystem::path&        path,
        TextureImage&                 layer) {
  auto file = m_io->open(path, IoOpenMode::eRead);

  if (!file) {
    Log::err("Failed to open ", path);
    return false;
  }

  std::vector<unsigned char> data(file->getSize());

  if (file->read(0, data.size(), data.data()) != IoStatus::eSuccess) {
    Log::err("Failed to read ", path);
    return false;
  }

  int w = 0;
  int h = 0;
  int n = 0;

  auto imageData = stbi_load_from_memory(data.data(), data.size(), &w, &h, &n, 4);

  if (!imageData) {
    Log::err("Failed to decode ", path);
    return false;
  }

  layer.w = w;
  layer.h = h;
  layer.channels = 4;
  layer.rawData.resize(w * h * sizeof(uint32_t));
  std::memcpy(layer.rawData.data(), imageData, layer.rawData.size());

  stbi_image_free(imageData);
  return true;
}


GfxFormat Texture::pickFormat(
  const TextureImage&                 layer) const {
  if (m_args.format != GfxFormat::eUnknown)
    return m_args.format;

  if (layer.channels == 1) {
    return m_args.allowCompression
      ? GfxFormat::eBc4un
      : GfxFormat::eR8un;
  }

  // Generate mask of channels that are actually used
  uint32_t channelNonzeroMask = 0;

  bool alphaUsed = false;

  for (uint32_t y = 0; y < layer.h; y++) {
    for (uint32_t x = 0; x < layer.w; x++) {
      uint32_t pixel = layer.get(x, y);
      channelNonzeroMask |= pixel;

      uint32_t alpha = (channelNonzeroMask >> 24);
      alphaUsed |= alpha < 0xFF;
    }

    // We don't need to continue if we already know
    // that the "worst case" scenario occured
    if ((alphaUsed || layer.channels < 4)
     && ((channelNonzeroMask & 0xFF0000) || layer.channels < 3))
      break;
  }

  if (!alphaUsed) {
    if (!(channelNonzeroMask & 0xFFFF00)) {
      return m_args.allowCompression
        ? GfxFormat::eBc4un
        : GfxFormat::eR8un;
    } else if (!(channelNonzeroMask & 0xFF0000)) {
      return m_args.allowCompression
        ? GfxFormat::eBc5un
        : GfxFormat::eR8G8un;
    }
  }

  if (!m_args.allowCompression)
    return GfxFormat::eR8G8B8A8srgb;

  if (m_args.allowBc7)
    return GfxFormat::eBc7srgb;

  return alphaUsed
    ? GfxFormat::eBc3srgb
    : GfxFormat::eBc1srgb;
}


void Texture::waitForCompletion() {
  for (auto& i : m_images) {
    m_jobs->wait(i.encodeJob);
    m_jobs->wait(i.mipmapJob);
  }
}


void Texture::generateMip(
        TextureImage*                 dstImage,
  const TextureImage*                 srcImage,
        uint32_t                      row) {
  bool isSrgb = m_formatInfo.flags & GfxFormatFlag::eSrgb;

  Vector2D dstSize(dstImage->w, dstImage->h);
  Vector2D srcSize(srcImage->w, srcImage->h);

  Vector2D scale = srcSize / dstSize;

  float y = scale.at<1>() * (float(row) + 0.25f);
  uint32_t yBase = uint32_t(y);
  float yFract = y - float(yBase);

  for (uint32_t col = 0; col < dstImage->w; col++) {
    float x = scale.at<0>() * (float(col) + 0.25f);
    uint32_t xBase = uint32_t(x);
    float xFract = x - xBase;

    std::array<Vector4D, 4> px;
    px[0] = srcImage->getf(xBase,     yBase);
    px[1] = srcImage->getf(xBase + 1, yBase);
    px[2] = srcImage->getf(xBase,     yBase + 1);
    px[3] = srcImage->getf(xBase + 1, yBase + 1);

    if (isSrgb) {
      for (uint32_t i = 0; i < px.size(); i++)
        px[i] = srgbToLinear(px[i]);
    }

    Vector4D p0 = px[0] + (px[1] - px[0]) * xFract;
    Vector4D p1 = px[2] + (px[3] - px[2]) * xFract;
    Vector4D p = p0 + (p1 - p0) * yFract;

    if (isSrgb)
      p = linearToSrgb(p);

    dstImage->setf(col, row, p);
  }
}


void Texture::encodeBlocks(
        TextureImage*                 dstImage,
        uint32_t                      row) {
  std::array<uint8_t, 64> input = { };

  Extent2D blockCount(dstImage->w, dstImage->h);
  blockCount += m_formatInfo.blockExtent - 1;
  blockCount >>= m_formatInfo.blockExtentLog2;

  uint32_t blockSize = m_formatInfo.planes[0].elementSize;

  bc7enc_compress_block_params bc7args = { };
  bc7enc_compress_block_params_init(&bc7args);

  for (uint32_t i = 0; i < blockCount.at<0>(); i++) {
    // Fetch raw pixel data from source block.
    uint32_t maxX = std::min(m_formatInfo.blockExtent.at<0>(), dstImage->w - m_formatInfo.blockExtent.at<0>() * i);
    uint32_t maxY = std::min(m_formatInfo.blockExtent.at<1>(), dstImage->h - m_formatInfo.blockExtent.at<1>() * row);

    for (uint32_t y = 0; y < m_formatInfo.blockExtent.at<1>(); y++) {
      for (uint32_t x = 0; x < m_formatInfo.blockExtent.at<0>(); x++) {
        uint32_t index = m_formatInfo.blockExtent.at<0>() * y + x;
        uint32_t dword;

        if (x < maxX && y < maxY) {
          dword = dstImage->get(
            x + m_formatInfo.blockExtent.at<0>() * i,
            y + m_formatInfo.blockExtent.at<1>() * row);
        } else {
          // If the block is clipped, repeat other pixels in the block
          dword = input[m_formatInfo.blockExtent.at<0>() * (y % maxY) + (x % maxX)];
        }

        std::memcpy(&input[index * sizeof(uint32_t)], &dword, sizeof(uint32_t));
      }
    }

    // Compute byte offset of current block
    auto blockData = &dstImage->encodedData[blockSize * (blockCount.at<0>() * row + i)];

    switch (m_format) {
      case GfxFormat::eBc1un:
      case GfxFormat::eBc1srgb:
        rgbcx::encode_bc1(rgbcx::MAX_LEVEL, blockData, input.data(), true, true);
        break;

      case GfxFormat::eBc3un:
      case GfxFormat::eBc3srgb:
        rgbcx::encode_bc3_hq(rgbcx::MAX_LEVEL, blockData, input.data());
        break;

      case GfxFormat::eBc4un:
      case GfxFormat::eBc4sn:
        rgbcx::encode_bc4_hq(blockData, input.data(), sizeof(uint32_t));
        break;

      case GfxFormat::eBc5un:
      case GfxFormat::eBc5sn:
        rgbcx::encode_bc5_hq(blockData, input.data(), 0, 1, sizeof(uint32_t));
        break;

      case GfxFormat::eBc7un:
      case GfxFormat::eBc7srgb:
        bc7enc_compress_block(blockData, input.data(), &bc7args);
        break;

      default:
        Log::err("Unhanled format: ", uint32_t(m_format));
    }
  }
}

