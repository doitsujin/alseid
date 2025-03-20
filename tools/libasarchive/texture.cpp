#include <iomanip>

#include "../../src/third_party/bc7enc/bc7enc.h"
#include "../../src/third_party/bc7enc/rgbcx.h"

#include "../../src/util/util_deflate.h"

#include "texture.h"

namespace as::archive {

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


static const std::vector<std::pair<std::string, GfxFormat>> g_formatNames = {{
  { "auto",       GfxFormat::eUnknown },
  { "r8un",       GfxFormat::eR8un },
  { "rg8un",      GfxFormat::eR8G8un },
  { "rgba8un",    GfxFormat::eR8G8B8A8un },
  { "bc1un",      GfxFormat::eBc1un },
  { "bc1srgb",    GfxFormat::eBc1srgb },
  { "bc3un",      GfxFormat::eBc1un },
  { "bc3srgb",    GfxFormat::eBc1srgb },
  { "bc4un",      GfxFormat::eBc4un },
  { "bc4sn",      GfxFormat::eBc4sn },
  { "bc5un",      GfxFormat::eBc5un },
  { "bc5sn",      GfxFormat::eBc5sn },
  { "bc7un",      GfxFormat::eBc7un },
  { "bc7srgb",    GfxFormat::eBc7srgb },
}};



GfxFormat textureFormatFromString(const std::string& str) {
  for (const auto& pair : g_formatNames) {
    if (pair.first == str)
      return pair.second;
  }

  Log::err("Unknown format: ", str);
  return GfxFormat::eUnknown;
}


std::string textureFormatToString(GfxFormat format) {
  for (const auto& pair : g_formatNames) {
    if (pair.second == format)
      return pair.first;
  }

  return "unknown";
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




Vector4D TextureImage::load(uint32_t x, uint32_t y) const {
  constexpr float factor = 1.0f / 255.0f;

  // Assume that format is always RGBA8
  size_t offset = sizeof(uint32_t) * (m_desc.w * y + x);

  auto data = reinterpret_cast<const uint8_t*>(m_data);
  auto pixel = &data[offset];

  return Vector4D(pixel[0], pixel[1], pixel[2], pixel[3]) * factor;
}


uint32_t TextureImage::loadRaw(uint32_t x, uint32_t y) const {
  size_t offset = sizeof(uint32_t) * (m_desc.w * y + x);
  auto data = reinterpret_cast<const uint8_t*>(m_data);

  uint32_t result;
  std::memcpy(&result, &data[offset], sizeof(result));
  return result;
}


void TextureImage::store(uint32_t x, uint32_t y, Vector4D color) {
  // Assume that format is always RGBA8
  size_t offset = sizeof(uint32_t) * (m_desc.w * y + x);

  color *= 255.0f;
  color += 0.5f;

  auto data = reinterpret_cast<uint8_t*>(m_data);
  auto pixel = &data[offset];

  pixel[0] = uint8_t(color.at<0>());
  pixel[1] = uint8_t(color.at<1>());
  pixel[2] = uint8_t(color.at<2>());
  pixel[3] = uint8_t(color.at<3>());
}


uint8_t* TextureImage::at(size_t offset) {
  return reinterpret_cast<uint8_t*>(m_data) + offset;
}


const uint8_t* TextureImage::at(size_t offset) const {
  return reinterpret_cast<uint8_t*>(m_data) + offset;
}




TextureBuildJob::TextureBuildJob(
        Environment                   env,
  const TextureDesc&                  desc,
        std::vector<std::filesystem::path> inputs)
: m_env           (std::move(env))
, m_desc          (desc)
, m_inputs        (std::move(inputs)) {

}


TextureBuildJob::~TextureBuildJob() {

}


std::pair<BuildResult, ArchiveFile> TextureBuildJob::build() {
  initBc7Enc();

  std::pair<BuildResult, ArchiveFile> result;
  result.first = BuildResult::eSuccess;

  if (m_inputs.empty()) {
    Log::err("No inputs specified for texture");

    result.first = BuildResult::eInvalidArgument;
    return result;
  }

  if (m_desc.enableCube && (m_inputs.size() % 6)) {
    Log::err("Cube textures must use a multiple of 6 inputs");

    result.first = BuildResult::eInvalidArgument;
    return result;
  }

  if (!m_desc.enableLayers && m_inputs.size() > 1) {
    Log::err("Multiple inputs specified for single-layer texture");

    result.first = BuildResult::eInvalidArgument;
    return result;
  }

  if (m_desc.enableLayers && m_desc.format == GfxFormat::eUnknown) {
    Log::err("Format detection only supported for single-layer images");

    result.first = BuildResult::eInvalidArgument;
    return result;
  }

  // Load first input file and compute image metadata
  TextureImage texture = loadImage(m_inputs[0]);

  if (!texture) {
    result.first = BuildResult::eInvalidInput;
    return result;
  }

  TextureImageDesc textureDesc = texture.getDesc();

  // Set up basic texture metadata
  GfxTextureDesc metadata = { };
  metadata.type = m_desc.enableLayers
    ? GfxImageViewType::e2DArray
    : GfxImageViewType::e2D;
  metadata.format = pickFormat(texture);
  metadata.extent = Extent3D(textureDesc.w, textureDesc.h, 1);
  metadata.mips = m_desc.enableMips
    ? findmsb(std::max(textureDesc.w, textureDesc.h)) + 1
    : 1;
  metadata.mipTailStart = metadata.mips;
  metadata.layers = m_inputs.size();

  if (m_desc.enableCube) {
    metadata.type = m_desc.enableLayers
      ? GfxImageViewType::eCubeArray
      : GfxImageViewType::eCube;
  }

  // Determine format based on image properties and contents
  GfxFormatInfo formatInfo = Gfx::getFormatInfo(metadata.format);

  // Set up arrays and allocate memory for encoded subresources
  uint32_t subresourceCount = metadata.layers * metadata.mips;
  m_subresourceImages.resize(subresourceCount);
  m_encodedImages.resize(subresourceCount);

  for (uint32_t l = 0; l < metadata.layers; l++) {
    for (uint32_t m = 0; m < metadata.mips; m++) {
      uint32_t index = computeSubresourceIndex(metadata, m, l);

      Extent2D mipExtent = gfxComputeMipExtent(Extent2D(textureDesc.w, textureDesc.h), m);
      Extent2D blockCount = (mipExtent + formatInfo.blockExtent - 1) >> formatInfo.blockExtentLog2;

      if (m) {
        // Allocate raw mipmap image
        TextureImageDesc imageDesc = { };
        imageDesc.format = textureDesc.format;
        imageDesc.w = mipExtent.at<0>();
        imageDesc.h = mipExtent.at<1>();
        imageDesc.channels = textureDesc.channels;
        imageDesc.dataSize = sizeof(uint32_t) * imageDesc.w * imageDesc.h;

        void* imageData = std::calloc(imageDesc.dataSize, 1);
        m_subresourceImages[index] = TextureImage(imageDesc, imageData);
      }

      // Allocate encoded image
      TextureImageDesc encodedDesc = { };
      encodedDesc.format = metadata.format;
      encodedDesc.w = mipExtent.at<0>();
      encodedDesc.h = mipExtent.at<1>();
      encodedDesc.channels = textureDesc.channels;
      encodedDesc.dataSize = formatInfo.planes[0].elementSize * blockCount.at<0>() * blockCount.at<1>();

      void* encodedData = std::calloc(encodedDesc.dataSize, 1);
      m_encodedImages[index] = TextureImage(encodedDesc, encodedData);

      // If the mip level is less than 64k in size when encoded,
      // let this be the start of our mip tail. There is no point
      // in subdividing mip levels further when using GDeflate.
      if (metadata.mipTailStart == metadata.mips && encodedDesc.dataSize < 0x10000)
        metadata.mipTailStart = m;
    }
  }

  // Set up arrays for compression
  bool hasMipTail = metadata.mipTailStart < metadata.mips;

  uint32_t chunkCount = hasMipTail
    ? metadata.layers * (metadata.mipTailStart + 1)
    : metadata.layers * (metadata.mips);

  m_compressedData.resize(chunkCount);
  m_rawSizes.resize(chunkCount);

  // Move first image to its appropriate place, then load all
  // other input images and verify that their parameters match.
  m_subresourceImages.at(0) = std::move(texture);

  for (uint32_t l = 1; l < metadata.layers; l++) {
    TextureImage current = loadImage(m_inputs[l]);

    if (!current) {
      result.first = BuildResult::eInvalidInput;
      return result;
    }

    TextureImageDesc currentDesc = current.getDesc();

    if (currentDesc.w != textureDesc.w || currentDesc.h != textureDesc.h) {
      Log::err("Mismatched dimensions in image ", m_inputs[l]);

      result.first = BuildResult::eInvalidInput;
      return result;
    }

    m_subresourceImages.at(computeSubresourceIndex(metadata, 0, l)) = std::move(current);
  }

  // Now that all the source images are set up, we can start dispatching
  // jobs. Launch one job per mip for mip generation purposes, and one
  // to encode that mip level in the desired format.
  std::vector<Job> deps;

  for (uint32_t l = 0; l < metadata.layers; l++) {
    for (uint32_t m = 0; m < metadata.mips; m++) {
      uint32_t index = computeSubresourceIndex(metadata, m, l);

      if (m) {
        auto currMip = &m_subresourceImages.at(index);
        auto prevMip = &m_subresourceImages.at(index - 1);

        m_env.jobs->execute<BatchJob>(
          [this, &formatInfo, currMip, prevMip] (uint32_t n) {
            generateMip(formatInfo, currMip, prevMip, n);
          }, currMip->getDesc().h, 8);
      }

      auto inputImage = &m_subresourceImages.at(index);
      auto encodedImage = &m_encodedImages.at(index);

      uint32_t blockCount = encodedImage->getDesc().h;
      blockCount += formatInfo.blockExtent.at<1>() - 1;
      blockCount >>= formatInfo.blockExtentLog2.at<1>();

      deps.push_back(m_env.jobs->dispatch<BatchJob>(
        [this, &formatInfo, inputImage, encodedImage] (uint32_t n) {
          encodeBlocks(formatInfo, encodedImage, inputImage, n);
        }, blockCount, 1));
    }
  }

  m_env.jobs->wait(deps.begin(), deps.end());

  // Finally, compress all subresources with GDeflate.
  m_env.jobs->execute<BatchJob>([this, &metadata, &result] (uint32_t n) {
    if (!compressChunk(metadata, n))
      result.first = BuildResult::eIoError;
  }, chunkCount, 1);

  if (result.first != BuildResult::eSuccess) {
    Log::err("Failed to compress texture subresource");
    return result;
  }

  // Generate metadata blob for the texture
  ArchiveData metadataBlob;

  if (!metadata.serialize(Lwrap<WrVectorStream>(metadataBlob))) {
    Log::err("Failed to serialize texture metadata");

    result.first = BuildResult::eInvalidInput;
    return result;
  }

  // Create actual file data
  result.second = ArchiveFile(FourCC('T', 'E', 'X', ' '), m_desc.name);
  result.second.setInlineData(std::move(metadataBlob));

  for (uint32_t l = 0; l < metadata.layers; l++) {
    for (uint32_t m = 0; m < std::min(metadata.mips, metadata.mipTailStart + 1); m++) {
      uint32_t index = computeDataIndex(metadata, m, l);

      FourCC ident = m < metadata.mipTailStart
        ? FourCC(strcat(std::setw(3), std::setfill('0'), std::hex, l, m))
        : FourCC(strcat(std::setw(3), std::setfill('0'), std::hex, l, 'T'));

      result.second.addSubFile(ident, IoArchiveCompression::eGDeflate,
        m_rawSizes[index], std::move(m_compressedData[index]));
    }
  }

  return result;
}


void TextureBuildJob::generateMip(
  const GfxFormatInfo&                formatInfo,
        TextureImage*                 dstImage,
  const TextureImage*                 srcImage,
        uint32_t                      row) {
  bool isSrgb = formatInfo.flags & GfxFormatFlag::eSrgb;

  TextureImageDesc dstDesc = dstImage->getDesc();
  TextureImageDesc srcDesc = srcImage->getDesc();

  Vector2D dstSize(dstDesc.w, dstDesc.h);
  Vector2D srcSize(srcDesc.w, srcDesc.h);

  Vector2D scale = srcSize / dstSize;

  float y = scale.at<1>() * (float(row) + 0.25f);
  uint32_t yBase = uint32_t(y);
  float yFract = y - float(yBase);

  for (uint32_t col = 0; col < dstDesc.w; col++) {
    float x = scale.at<0>() * (float(col) + 0.25f);
    uint32_t xBase = uint32_t(x);
    float xFract = x - xBase;

    std::array<Vector4D, 4> px;
    px[0] = srcImage->load(xBase,     yBase);
    px[1] = srcImage->load(xBase + 1, yBase);
    px[2] = srcImage->load(xBase,     yBase + 1);
    px[3] = srcImage->load(xBase + 1, yBase + 1);

    if (isSrgb) {
      for (uint32_t i = 0; i < px.size(); i++)
        px[i] = srgbToLinear(px[i]);
    }

    Vector4D p0 = px[0] + (px[1] - px[0]) * xFract;
    Vector4D p1 = px[2] + (px[3] - px[2]) * xFract;
    Vector4D p = p0 + (p1 - p0) * yFract;

    if (isSrgb)
      p = linearToSrgb(p);

    dstImage->store(col, row, p);
  }
}


void TextureBuildJob::encodeBlocks(
  const GfxFormatInfo&                formatInfo,
        TextureImage*                 dstImage,
  const TextureImage*                 srcImage,
        uint32_t                      row) {
  std::array<uint8_t, 64> input = { };

  TextureImageDesc srcDesc = srcImage->getDesc();
  TextureImageDesc dstDesc = dstImage->getDesc();

  Extent2D blockCount(srcDesc.w, srcDesc.h);
  blockCount += formatInfo.blockExtent - 1;
  blockCount >>= formatInfo.blockExtentLog2;

  uint32_t blockSize = formatInfo.planes[0].elementSize;

  bc7enc_compress_block_params bc7args = { };
  bc7enc_compress_block_params_init(&bc7args);

  for (uint32_t i = 0; i < blockCount.at<0>(); i++) {
    // Fetch raw pixel data from source block.
    uint32_t maxX = std::min(formatInfo.blockExtent.at<0>(), srcDesc.w - formatInfo.blockExtent.at<0>() * i);
    uint32_t maxY = std::min(formatInfo.blockExtent.at<1>(), srcDesc.h - formatInfo.blockExtent.at<1>() * row);

    for (uint32_t y = 0; y < formatInfo.blockExtent.at<1>(); y++) {
      for (uint32_t x = 0; x < formatInfo.blockExtent.at<0>(); x++) {
        uint32_t index = formatInfo.blockExtent.at<0>() * y + x;
        uint32_t dword;

        if (x < maxX && y < maxY) {
          dword = srcImage->loadRaw(
            x + formatInfo.blockExtent.at<0>() * i,
            y + formatInfo.blockExtent.at<1>() * row);
        } else {
          // If the block is clipped, repeat other pixels in the block
          dword = input[formatInfo.blockExtent.at<0>() * (y % maxY) + (x % maxX)];
        }

        std::memcpy(&input[index * sizeof(uint32_t)], &dword, sizeof(uint32_t));
      }
    }

    // Compute byte offset of current block
    auto blockData = reinterpret_cast<uint8_t*>(dstImage->at(blockSize * (blockCount.at<0>() * row + i)));

    switch (dstDesc.format) {
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
        Log::err("Unhanled format: ", uint32_t(dstDesc.format));
    }
  }
}


bool TextureBuildJob::compressChunk(
  const GfxTextureDesc&               metadata,
        uint32_t                      dataIndex) {
  uint32_t mip = 0;
  uint32_t layer = 0;

  bool hasMipTail = metadata.mipTailStart < metadata.mips;

  if (hasMipTail) {
    mip   = dataIndex % (metadata.mipTailStart + 1);
    layer = dataIndex / (metadata.mipTailStart + 1);
  } else {
    mip   = dataIndex % metadata.mips;
    layer = dataIndex / metadata.mips;
  }

  ArchiveData subresourceData;

  uint32_t subresourceIndex = computeSubresourceIndex(metadata, mip, layer);
  uint32_t subresourceCount = 1;

  if (mip >= metadata.mipTailStart)
    subresourceCount = metadata.mips - metadata.mipTailStart;

  size_t dataSize = 0;

  for (uint32_t i = 0; i < subresourceCount; i++)
    dataSize += m_encodedImages.at(subresourceIndex + i).getDesc().dataSize;

  subresourceData.resize(dataSize);

  size_t dataOffset = 0;

  for (uint32_t i = 0; i < subresourceCount; i++) {
    size_t subresourceSize = m_encodedImages.at(subresourceIndex + i).getDesc().dataSize;
    std::memcpy(&subresourceData[dataOffset], m_encodedImages.at(subresourceIndex + i).at(0), subresourceSize);
    dataOffset += subresourceSize;
  }

  m_rawSizes[dataIndex] = subresourceData.size();

  return gdeflateEncode(Lwrap<WrVectorStream>(m_compressedData.at(dataIndex)), subresourceData);
}


TextureImage TextureBuildJob::loadImage(
  const std::filesystem::path&        path) {
  auto file = m_env.io->open(path, IoOpenMode::eRead);

  if (!file) {
    Log::err("Failed to open ", path);
    return TextureImage();
  }

  std::vector<unsigned char> data(file->getSize());

  if (file->read(0, data.size(), data.data()) != IoStatus::eSuccess) {
    Log::err("Failed to read ", path);
    return TextureImage();
  }

  int w = 0;
  int h = 0;
  int n = 0;

  auto imageData = stbi_load_from_memory(data.data(), data.size(), &w, &h, &n, 4);

  if (!imageData) {
    Log::err("Failed to decode ", path);
    return TextureImage();
  }

  TextureImageDesc desc = { };
  desc.format = GfxFormat::eR8G8B8A8srgb;
  desc.w = w;
  desc.h = h;
  desc.channels = n;
  desc.dataSize = w * h * sizeof(uint32_t);

  return TextureImage(desc, imageData);
}


GfxFormat TextureBuildJob::pickFormat(
  const TextureImage&                 texture) const {
  if (m_desc.format != GfxFormat::eUnknown)
    return m_desc.format;

  TextureImageDesc textureDesc = texture.getDesc();

  // Generate mask of channels that are actually used
  uint32_t channelNonzeroMask = 0;

  bool alphaUsed = false;

  for (uint32_t y = 0; y < textureDesc.h; y++) {
    for (uint32_t x = 0; x < textureDesc.w; x++) {
      uint32_t pixel = texture.loadRaw(x, y);
      channelNonzeroMask |= pixel;

      uint32_t alpha = (channelNonzeroMask >> 24);
      alphaUsed |= alpha < 0xFF;
    }

    // We don't need to continue if we already know
    // that the "worst case" scenario occured
    if ((alphaUsed || textureDesc.channels < 4)
     && ((channelNonzeroMask & 0xFF0000) || textureDesc.channels < 3))
      break;
  }

  if (!alphaUsed) {
    if (!(channelNonzeroMask & 0xFFFF00)) {
      return m_desc.allowCompression
        ? GfxFormat::eBc4un
        : GfxFormat::eR8un;
    } else if (!(channelNonzeroMask & 0xFF0000)) {
      return m_desc.allowCompression
        ? GfxFormat::eBc5un
        : GfxFormat::eR8G8un;
    }
  }

  if (!m_desc.allowCompression)
    return GfxFormat::eR8G8B8A8srgb;

  if (m_desc.allowBc7)
    return GfxFormat::eBc7srgb;

  return alphaUsed
    ? GfxFormat::eBc3srgb
    : GfxFormat::eBc1srgb;
}


uint32_t TextureBuildJob::computeSubresourceIndex(
  const GfxTextureDesc&               metadata,
        uint32_t                      mip,
        uint32_t                      layer) const {
  return metadata.mips * layer + mip;
}


uint32_t TextureBuildJob::computeDataIndex(
  const GfxTextureDesc&               metadata,
        uint32_t                      mip,
        uint32_t                      layer) const {
  if (metadata.mipTailStart < metadata.mips) {
    return mip < metadata.mipTailStart
      ? (metadata.mipTailStart + 1) * layer + mip
      : (metadata.mipTailStart + 1) * layer + metadata.mipTailStart;
  } else {
    return metadata.mips * layer + mip;
  }
}

}
