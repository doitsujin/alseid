#include <unordered_map>
#include <unordered_set>

#include "gltf_import.h"

namespace as::gltf {

GltfVertexDataReader::GltfVertexDataReader(
        std::shared_ptr<GltfMeshPrimitive>  primitive)
: m_primitive (std::move(primitive)) {
  uint32_t layoutOffset = 0;

  auto attributes = m_primitive->getAttributes();

  for (auto a = attributes.first; a != attributes.second; a++) {
    GltfDataType type = a->second->getDataType();

    GltfVertexAttribute attribute = { };
    attribute.name = a->first;
    attribute.components = type.rows * type.cols;
    attribute.offset = layoutOffset;

    switch (type.componentType) {
      case GltfComponentType::eS8:
      case GltfComponentType::eS16:
      case GltfComponentType::eS32:
        attribute.type.componentType = type.normalized
          ? GltfComponentType::eF32
          : GltfComponentType::eS32;
        break;

      case GltfComponentType::eU8:
      case GltfComponentType::eU16:
      case GltfComponentType::eU32:
        attribute.type.componentType = type.normalized
          ? GltfComponentType::eF32
          : GltfComponentType::eU32;
        break;

      case GltfComponentType::eF32:
        attribute.type.componentType = type.componentType;
        break;
    }

    attribute.type.rows = type.rows;
    attribute.type.cols = type.cols;
    attribute.type.normalized = false;

    layoutOffset += attribute.components;

    m_layout.attributes.push_back(std::move(attribute));
  }
}


GltfVertexDataReader::~GltfVertexDataReader() {

}


uint32_t GltfVertexDataReader::countPrimitives() const {
  std::shared_ptr<GltfAccessor> indices = m_primitive->getIndices();

  uint32_t indexCount = indices != nullptr
    ? indices->getElementCount()
    : countVertices();

  switch (m_primitive->getTopology()) {
    case GltfPrimitiveTopology::ePointList:
      return indexCount;

    case GltfPrimitiveTopology::eLineList:
      return indexCount / 2;

    case GltfPrimitiveTopology::eLineStrip:
      return std::max(indexCount, 1u) - 1;

    case GltfPrimitiveTopology::eLineLoop:
      return indexCount;

    case GltfPrimitiveTopology::eTriangleList:
      return indexCount / 3;

    case GltfPrimitiveTopology::eTriangleStrip:
    case GltfPrimitiveTopology::eTriangleFan:
      return std::max(indexCount, 2u) - 2;
  }

  return 0;
}


uint32_t GltfVertexDataReader::countIndices() const {
  uint32_t primitiveCount = countPrimitives();

  GltfPrimitiveTopology topology = getTopology();

  if (topology == GltfPrimitiveTopology::eTriangleList)
    return 3 * primitiveCount;

  if (topology == GltfPrimitiveTopology::eLineList)
    return 2 * primitiveCount;

  return primitiveCount;
}


uint32_t GltfVertexDataReader::countVertices() const {
  auto attributes = m_primitive->getAttributes();

  if (attributes.first == attributes.second)
    return 0;

  // Assume that all attribute accessors have the same vertex count
  return attributes.first->second->getElementCount();
}


GltfPrimitiveTopology GltfVertexDataReader::getTopology() const {
  switch (m_primitive->getTopology()) {
    case GltfPrimitiveTopology::ePointList:
      return GltfPrimitiveTopology::ePointList;

    case GltfPrimitiveTopology::eLineList:
    case GltfPrimitiveTopology::eLineStrip:
    case GltfPrimitiveTopology::eLineLoop:
      return GltfPrimitiveTopology::eLineList;

    case GltfPrimitiveTopology::eTriangleList:
    case GltfPrimitiveTopology::eTriangleStrip:
    case GltfPrimitiveTopology::eTriangleFan:
      return GltfPrimitiveTopology::eTriangleList;
  }

  return GltfPrimitiveTopology::eTriangleList;
}


void GltfVertexDataReader::readIndices(
        uint32_t*                     dst) const {
  std::shared_ptr<GltfAccessor> indices = m_primitive->getIndices();

  uint32_t primitiveCount = countPrimitives();

  if (!primitiveCount)
    return;

  switch (m_primitive->getTopology()) {
    case GltfPrimitiveTopology::ePointList:
    case GltfPrimitiveTopology::eLineList:
    case GltfPrimitiveTopology::eTriangleList: {
      uint32_t indexCount = countIndices();

      for (uint32_t i = 0; i < indexCount; i++)
        dst[i] = readIndex(indices, i);
    } break;

    case GltfPrimitiveTopology::eLineStrip: {
      for (uint32_t i = 0; i < primitiveCount; i++) {
        dst[2 * i + 0] = readIndex(indices, i + 0);
        dst[2 * i + 1] = readIndex(indices, i + 1);
      }
    } break;

    case GltfPrimitiveTopology::eLineLoop: {
      for (uint32_t i = 0; i < primitiveCount - 1; i++) {
        dst[2 * i + 0] = readIndex(indices, i + 0);
        dst[2 * i + 1] = readIndex(indices, i + 1);
      }

      dst[2 * primitiveCount - 2] = readIndex(indices, primitiveCount - 1);
      dst[2 * primitiveCount - 1] = readIndex(indices, 0);
    } break;

    case GltfPrimitiveTopology::eTriangleStrip: {
      for (uint32_t i = 0; i < primitiveCount - 1; i++) {
        dst[3 * i + 0] = readIndex(indices, i + 0);
        dst[3 * i + 1] = readIndex(indices, i + 1 + (i & 1));
        dst[3 * i + 2] = readIndex(indices, i + 2 - (i & 1));
      }
    } break;

    case GltfPrimitiveTopology::eTriangleFan: {
      uint32_t firstIndex = readIndex(indices, 0);

      for (uint32_t i = 0; i < primitiveCount - 1; i++) {
        dst[3 * i + 0] = readIndex(indices, i + 1);
        dst[3 * i + 1] = readIndex(indices, i + 2);
        dst[3 * i + 2] = firstIndex;
      }
    } break;
  }
}


void GltfVertexDataReader::readVertices(
        GltfVertex*                   dst) const {
  uint32_t vertexCount = countVertices();

  // While performance isn't critical here, we should at least try to be
  // somewhat efficient: Processing attributes one by one rather than
  // iterating over each attribute per vertex greatly reduces overhead,
  // and processing vertices in chunks of 256 is useful to keep vertex
  // data in L2.
  for (uint32_t vFirst = 0; vFirst < vertexCount; vFirst += 256) {
    uint32_t vLast = std::min(vFirst + 256, vertexCount);

    for (const auto& a : m_layout.attributes) {
      std::shared_ptr<GltfAccessor> accessor = m_primitive->findAttribute(a.name.c_str());
      readVertexRange(a, accessor, vFirst, vLast, nullptr, dst);
    }
  }
}


void GltfVertexDataReader::readMorphedVertices(
  const std::shared_ptr<GltfMorphTarget>& target,
        uint32_t                      vertexCount,
  const uint32_t*                     vertexIndices,
        GltfVertex*                   dst) const {
  for (const auto& a : m_layout.attributes) {
    std::shared_ptr<GltfAccessor> accessor = target->findAttribute(a.name.c_str());

    if (accessor != nullptr)
      readVertexRange(a, accessor, 0, vertexCount, vertexIndices, dst);
  }
}


uint32_t GltfVertexDataReader::readIndex(
  const std::shared_ptr<GltfAccessor>& accessor,
        uint32_t                      index) const {
  if (accessor == nullptr)
    return index;

  union {
    uint8_t   u8;
    uint16_t  u16;
    uint32_t  u32;
  } srcData;

  if (accessor->getElementData(index, &srcData)) {
    GltfComponentType type = accessor->getDataType().componentType;

    if (type == GltfComponentType::eU32)
      return srcData.u32;

    if (type == GltfComponentType::eU16)
      return uint32_t(srcData.u16);

    if (type == GltfComponentType::eU8)
      return uint32_t(srcData.u8);

    Log::err("Unknown index type ", uint32_t(type));
    return 0u;
  } else {
    Log::err("Failed to read index ", index);
    return 0u;
  }
}


void GltfVertexDataReader::readVertexRange(
  const GltfVertexAttribute&          attribute,
  const std::shared_ptr<GltfAccessor>& accessor,
        uint32_t                      vFirst,
        uint32_t                      vLast,
  const uint32_t*                     indices,
        GltfVertex*                   dst) const {
  GltfDataType srcType = accessor->getDataType();

  uint32_t offset = attribute.offset;

  switch (srcType.componentType) {
    case GltfComponentType::eU8: {
      std::array<uint8_t, 4> data = { };

      for (uint32_t i = vFirst; i < vLast; i++) {
        uint32_t v = indices ? indices[i] : i;
        accessor->getElementData(v, data.data());

        if (srcType.normalized) {
          const float scale = 1.0f / 255.0f;

          for (uint32_t c = 0; c < attribute.components; c++)
            dst[i].f32[offset + c] = std::min(float(data[c]) * scale, 1.0f);
        } else {
          for (uint32_t c = 0; c < attribute.components; c++)
            dst[i].u32[offset + c] = uint32_t(data[c]);
        }
      }
    } break;

    case GltfComponentType::eS8: {
      std::array<int8_t, 4> data = { };

      for (uint32_t i = vFirst; i < vLast; i++) {
        uint32_t v = indices ? indices[i] : i;
        accessor->getElementData(v, data.data());

        if (srcType.normalized) {
          const float scale = 1.0f / 127.0f;

          for (uint32_t c = 0; c < attribute.components; c++)
            dst[i].f32[offset + c] = std::max(std::min(float(data[c]) * scale, 1.0f), -1.0f);
        } else {
          for (uint32_t c = 0; c < attribute.components; c++)
            dst[i].i32[offset + c] = int32_t(data[c]);
        }
      }
    } break;

    case GltfComponentType::eU16: {
      std::array<uint16_t, 4> data = { };

      for (uint32_t i = vFirst; i < vLast; i++) {
        uint32_t v = indices ? indices[i] : i;
        accessor->getElementData(v, data.data());

        if (srcType.normalized) {
          const float scale = 1.0f / 65535.0f;

          for (uint32_t c = 0; c < attribute.components; c++)
            dst[i].f32[offset + c] = std::min(float(data[c]) * scale, 1.0f);
        } else {
          for (uint32_t c = 0; c < attribute.components; c++)
            dst[i].u32[offset + c] = uint32_t(data[c]);
        }
      }
    } break;

    case GltfComponentType::eS16: {
      std::array<int16_t, 4> data = { };

      for (uint32_t i = vFirst; i < vLast; i++) {
        uint32_t v = indices ? indices[i] : i;
        accessor->getElementData(v, data.data());

        if (srcType.normalized) {
          const float scale = 1.0f / 32767.0f;

          for (uint32_t c = 0; c < attribute.components; c++)
            dst[i].f32[offset + c] = std::max(std::min(float(data[c]) * scale, 1.0f), -1.0f);
        } else {
          for (uint32_t c = 0; c < attribute.components; c++)
            dst[i].i32[offset + c] = int32_t(data[c]);
        }
      }
    } break;

    case GltfComponentType::eU32:
    case GltfComponentType::eS32: {
      // Trivial case that does not require conversion
      for (uint32_t i = vFirst; i < vLast; i++) {
        uint32_t v = indices ? indices[i] : i;
        accessor->getElementData(v, &dst[i].u32[offset]);
      }
    } break;

    case GltfComponentType::eF32: {
      // Replace denorms, negative zero etc with zero
      std::array<float, 4> data = { };

      for (uint32_t i = vFirst; i < vLast; i++) {
        uint32_t v = indices ? indices[i] : i;
        accessor->getElementData(v, data.data());

        for (uint32_t c = 0; c < attribute.components; c++)
          dst[i].f32[offset + c] = std::isnormal(data[c]) ? data[c] : 0.0f;
      }
    } break;
  }
}


GfxFormat formatFromString(const std::string& name) {
  static const std::unordered_map<std::string, GfxFormat> s_formats = {{
    { "rgba4un",    GfxFormat::eR4G4B4A4un      },
    { "r8un",       GfxFormat::eR8un            },
    { "r8sn",       GfxFormat::eR8sn            },
    { "r8ui",       GfxFormat::eR8ui            },
    { "r8si",       GfxFormat::eR8si            },
    { "rg8un",      GfxFormat::eR8G8un          },
    { "rg8sn",      GfxFormat::eR8G8sn          },
    { "rg8ui",      GfxFormat::eR8G8ui          },
    { "rg8si",      GfxFormat::eR8G8si          },
    { "rgb8un",     GfxFormat::eR8G8B8un        },
    { "rgb8sn",     GfxFormat::eR8G8B8sn        },
    { "rgb8ui",     GfxFormat::eR8G8B8ui        },
    { "rgb8si",     GfxFormat::eR8G8B8si        },
    { "rgba8un",    GfxFormat::eR8G8B8A8un      },
    { "rgba8sn",    GfxFormat::eR8G8B8A8sn      },
    { "rgba8ui",    GfxFormat::eR8G8B8A8ui      },
    { "rgba8si",    GfxFormat::eR8G8B8A8si      },
    { "rgb10a2un",  GfxFormat::eR10G10B10A2un   },
    { "rgb10a2sn",  GfxFormat::eR10G10B10A2sn   },
    { "rgb10a2ui",  GfxFormat::eR10G10B10A2ui   },
    { "r16un",      GfxFormat::eR16un           },
    { "r16sn",      GfxFormat::eR16sn           },
    { "r16ui",      GfxFormat::eR16ui           },
    { "r16si",      GfxFormat::eR16si           },
    { "r16f",       GfxFormat::eR16f            },
    { "rg16un",     GfxFormat::eR16G16un        },
    { "rg16sn",     GfxFormat::eR16G16sn        },
    { "rg16ui",     GfxFormat::eR16G16ui        },
    { "rg16si",     GfxFormat::eR16G16si        },
    { "rg16f",      GfxFormat::eR16G16f         },
    { "rgb16un",    GfxFormat::eR16G16B16un     },
    { "rgb16sn",    GfxFormat::eR16G16B16sn     },
    { "rgb16ui",    GfxFormat::eR16G16B16ui     },
    { "rgb16si",    GfxFormat::eR16G16B16si     },
    { "rgb16f",     GfxFormat::eR16G16B16f      },
    { "rgba16un",   GfxFormat::eR16G16B16A16un  },
    { "rgba16sn",   GfxFormat::eR16G16B16A16sn  },
    { "rgba16ui",   GfxFormat::eR16G16B16A16ui  },
    { "rgba16si",   GfxFormat::eR16G16B16A16si  },
    { "rgba16f",    GfxFormat::eR16G16B16A16f   },
    { "r32ui",      GfxFormat::eR32ui           },
    { "r32si",      GfxFormat::eR32si           },
    { "r32f",       GfxFormat::eR32f            },
    { "rg32ui",     GfxFormat::eR32G32ui        },
    { "rg32si",     GfxFormat::eR32G32si        },
    { "rg32f",      GfxFormat::eR32G32f         },
    { "rgb32ui",    GfxFormat::eR32G32B32ui     },
    { "rgb32si",    GfxFormat::eR32G32B32si     },
    { "rgb32f",     GfxFormat::eR32G32B32f      },
    { "rgba32ui",   GfxFormat::eR32G32B32A32ui  },
    { "rgba32si",   GfxFormat::eR32G32B32A32si  },
    { "rgba32f",    GfxFormat::eR32G32B32A32f   },
  }};

  auto e = s_formats.find(name);

  return (e != s_formats.end())
    ? e->second : GfxFormat::eUnknown;
}


void from_json(const json& j, GltfPackedVertexAttributeDesc& desc) {
  j.at("name").get_to(desc.name);

  std::string format;
  j.at("format").get_to(format);
  desc.format = formatFromString(format);

  if (j.count("stream")) {
    std::string stream;
    j.at("stream").get_to(stream);

    desc.stream = (stream == "shading")
      ? GfxMeshletAttributeStream::eShadingData
      : GfxMeshletAttributeStream::eVertexData;
  }

  if (j.count("morph"))
    j.at("morph").get_to(desc.morph);
}


void from_json(const json& j, GltfPackedVertexLayoutDesc& desc) {
  j.at("name").get_to(desc.name);
  j.at("attributes").get_to(desc.attributes);
}


GltfPackedVertexLayout::GltfPackedVertexLayout(
  const GltfPackedVertexLayoutDesc&   desc) {
  // Initialize general attribute metadata before
  // computing the data layout
  m_metadata.name = desc.name;
  m_metadata.attributeCount = desc.attributes.size();

  m_attributes.resize(m_metadata.attributeCount);

  for (size_t i = 0; i < m_metadata.attributeCount; i++) {
    m_attributes[i].name = desc.attributes[i].name;
    m_attributes[i].dataFormat = desc.attributes[i].format;
    m_attributes[i].stream = desc.attributes[i].stream;
    m_attributes[i].morph = desc.attributes[i].morph;

    std::tie(m_attributes[i].semantic, m_attributes[i].semanticIndex) =
      parseSemantic(desc.attributes[i].name.c_str());
  }

  // Compute the data layout for each stream
  m_metadata.vertexDataStride = computeDataLayout(
    GltfPackedVertexStream::eVertexData);
  m_metadata.shadingDataStride = computeDataLayout(
    GltfPackedVertexStream::eShadingData);
  m_metadata.morphDataStride = computeDataLayout(
    GltfPackedVertexStream::eMorphData);
}


uint32_t GltfPackedVertexLayout::getStreamDataStride(
        GltfPackedVertexStream        streamType) const {
  switch (streamType) {
    case GltfPackedVertexStream::eVertexData:
      return m_metadata.vertexDataStride;
    case GltfPackedVertexStream::eShadingData:
      return m_metadata.shadingDataStride;
    case GltfPackedVertexStream::eMorphData:
      return m_metadata.morphDataStride;
  }

  return 0;
}


void GltfPackedVertexLayout::processVertices(
  const GltfVertexLayout&             inputLayout,
        uint32_t                      vertexCount,
  const GltfVertex*                   vertexData,
  const uint32_t*                     indexData,
        GltfPackedVertexStream        outputType,
        void*                         data) const {
  // Don't bother trying to be cache friendly for large inputs.
  // Ideally, this gets called for meshlet data anyway.
  uint32_t stride = getStreamDataStride(outputType);

  for (const auto& a : m_attributes) {
    if (!testAttributeStream(a, outputType))
      continue;

    // Not finding the attribute in the source data is fine,
    // we'll just assume all data to be zero in that case.
    const GltfVertexAttribute* inputAttribute = inputLayout.findAttribute(a.name.c_str());

    if (!inputAttribute)
      continue;

    // Find properties of the output data format
    const GfxFormatAspectInfo& formatInfo = Gfx::getFormatInfo(a.dataFormat).planes[0];

    std::array<uint8_t, 4> bitCounts = { formatInfo.rBits, formatInfo.gBits, formatInfo.bBits, formatInfo.aBits };
    std::array<float,   4> normScale = { 1.0f, 1.0f, 1.0f, 1.0f };

    for (uint32_t i = 0; i < 4; i++) {
      uint32_t shift = formatInfo.type == GfxFormatType::eUnorm ? 0 : 1;
      normScale[i] = float(((1u << bitCounts[i]) >> shift) - 1u);
    }

    // Get output attribute byte offset for the given stream
    uint32_t offset = outputType == GltfPackedVertexStream::eMorphData
      ? a.morphOffset
      : a.dataOffset;

    for (uint32_t i = 0; i < vertexCount; i++) {
      const GltfVertex* v = indexData
        ? &vertexData[indexData[i]]
        : &vertexData[i];

      // Read input data as both floats and integers. This way we
      // won't have to worry about the source data format later.
      std::array<uint32_t, 4> u32 = { };
      std::array<int32_t,  4> i32 = { };
      std::array<float,    4> f32 = { };

      if (inputAttribute->type.componentType == GltfComponentType::eU32) {
        for (uint32_t j = 0; j < inputAttribute->components; j++) {
          u32[j] = v->u32[inputAttribute->offset + j];
          i32[j] = int32_t(u32[j]);
          f32[j] = float(u32[j]);
        }
      } else if (inputAttribute->type.componentType == GltfComponentType::eS32) {
        for (uint32_t j = 0; j < inputAttribute->components; j++) {
          i32[j] = v->i32[inputAttribute->offset + j];
          u32[j] = uint32_t(i32[i]);
          f32[j] = float(i32[i]);
        }
      } else {
        for (uint32_t j = 0; j < inputAttribute->components; j++) {
          f32[j] = v->f32[inputAttribute->offset + j];
          u32[j] = uint32_t(f32[j]);
          i32[j] = int32_t(f32[j]);
        }
      }

      // Perform final format conversion for output data
      std::array<uint32_t, 4> dwords = { };

      // Output bit index, used to determine the DWORD to write to as
      // well as the bit shift to apply. None of the supported formats
      // have components straddling DWORD boundaries, so ignore that.
      uint32_t outputBit = 0;

      for (uint32_t j = 0; j < 4 && bitCounts[j]; j++) {
        uint32_t dword = outputBit >> 5;
        uint32_t shift = outputBit & 31;
        uint32_t count = bitCounts[j];
        uint32_t mask = (2u << (count - 1u)) - 1u;

        switch (formatInfo.type) {
          case GfxFormatType::eFloat: {
            if (count == 32)
              std::memcpy(&dwords[j], &f32[j], 4);
            else if (count == 16)
              dwords[dword] |= uint32_t(f32tof16(f32[j])) << shift;
          } break;

          case GfxFormatType::eUint: {
            dwords[dword] |= (u32[j] & mask) << shift;
          } break;

          case GfxFormatType::eSint: {
            dwords[dword] |= (uint32_t(i32[j]) & mask) << shift;
          } break;

          case GfxFormatType::eUnorm:
          case GfxFormatType::eSnorm: {
            uint32_t u = uint32_t(std::lroundf(f32[j] * normScale[j]));
            dwords[dword] |= (u & mask) << shift;
          } break;
        }

        outputBit += count;
      }

      // Copy formatted vertex to the output array
      void* dst = reinterpret_cast<char*>(data) + i * stride + offset;
      std::memcpy(dst, dwords.data(), formatInfo.elementSize);
    }
  }
}


uint32_t GltfPackedVertexLayout::computeDataLayout(
        GltfPackedVertexStream        streamType) {
  uint32_t dataOffset = 0;
  uint32_t dataAlignment = 0;

  for (auto& a : m_attributes) {
    if (!testAttributeStream(a, streamType))
      continue;

    uint32_t formatSize;
    uint32_t formatAlignment;

    std::tie(formatSize, formatAlignment) = computeFormatSize(a.dataFormat);

    dataOffset = align(dataOffset, formatAlignment);
    dataAlignment = std::max(dataAlignment, formatAlignment);

    if (streamType == GltfPackedVertexStream::eMorphData)
      a.morphOffset = dataOffset;
    else
      a.dataOffset = dataOffset;

    dataOffset += formatSize;
  }

  // Align entire vertex to the maximum member alignment
  return align(dataOffset, dataAlignment);
}


bool GltfPackedVertexLayout::testAttributeStream(
  const GfxMeshletAttributeMetadata&  attribute,
        GltfPackedVertexStream        streamType) {
  switch (streamType) {
    case GltfPackedVertexStream::eVertexData:
      return attribute.stream == GfxMeshletAttributeStream::eVertexData;
    case GltfPackedVertexStream::eShadingData:
      return attribute.stream == GfxMeshletAttributeStream::eShadingData;
    case GltfPackedVertexStream::eMorphData:
      return attribute.morph;
  }

  return false;
}


std::pair<uint32_t, uint32_t> GltfPackedVertexLayout::computeFormatSize(
        GfxFormat                     format) {
  const GfxFormatAspectInfo& info = Gfx::getFormatInfo(format).planes[0];

  // If components don't naturally fill whole bytes, align the
  // format to the element size. This means that packed formats
  // must use plain integers of the correct size, e.t. uint32_t
  // for RGB10A2 or uint16_t for RGBA4.
  if ((info.rBits | info.gBits | info.bBits | info.aBits) & 0x7)
    return std::make_pair(info.elementSize, info.elementSize);

  // Otherwise, find the size of the largest component, in bytes,
  // and use that byte size as the alignment. All supported fomats
  // will have powers of two here.
  uint32_t bitCount = std::max(
    std::max(info.rBits, info.gBits),
    std::max(info.bBits, info.aBits));

  return std::make_pair(info.elementSize, bitCount >> 3);
}


std::pair<GfxMeshletAttributeSemantic, uint32_t> GltfPackedVertexLayout::parseSemantic(
  const char*                         name) {
  static const std::array<std::pair<const char*, GfxMeshletAttributeSemantic>, 7> s_semantics = {{
    { "POSITION",     GfxMeshletAttributeSemantic::ePosition    },
    { "NORMAL",       GfxMeshletAttributeSemantic::eNormal      },
    { "TANGENT",      GfxMeshletAttributeSemantic::eTangent     },
    { "TEXCOORD",     GfxMeshletAttributeSemantic::eTexCoord    },
    { "COLOR",        GfxMeshletAttributeSemantic::eColor       },
    { "JOINTS",       GfxMeshletAttributeSemantic::eJointIndex  },
    { "WEIGHTS",      GfxMeshletAttributeSemantic::eJointWeight },
  }};

  if (!name || !name[0] || name[0] == '_')
    return std::make_pair(GfxMeshletAttributeSemantic::eNone, 0u);

  // Find first occurence of underscore. The name up to that
  // point defines the semantic, the part after is the index.
  size_t p = 0;

  while (name[p] && name[p] != '_')
    p += 1;

  // Scan known semantic name array
  GfxMeshletAttributeSemantic semantic = GfxMeshletAttributeSemantic::eNone;

  for (const auto& s : s_semantics) {
    if (!std::strncmp(name, s.first, p))
      semantic = s.second;
  }

  // Don't bother parsing the index if we don't know the semantic
  if (semantic == GfxMeshletAttributeSemantic::eNone)
    return std::make_pair(semantic, 0u);

  // Parse semantic index
  uint32_t index = 0;
  
  if (name[p++]) {
    while (name[p]) {
      char c = name[p++];

      if (c < '0' || c > '9')
        return std::make_pair(GfxMeshletAttributeSemantic::eNone, 0u);

      index *= 10;
      index += uint32_t(c - '0');
    }
  }

  return std::make_pair(semantic, index);
}




GltfPackedVertexLayoutMap::GltfPackedVertexLayoutMap() {

}


GltfPackedVertexLayoutMap::~GltfPackedVertexLayoutMap() {

}


std::shared_ptr<GltfPackedVertexLayout> GltfPackedVertexLayoutMap::emplace(
  const GltfPackedVertexLayoutDesc&   desc) {
  auto layout = std::make_shared<GltfPackedVertexLayout>(desc);
  auto result = m_map.emplace(desc.name, layout);

  if (!result.second)
    result.first->second = layout;

  return layout;
}


std::shared_ptr<GltfPackedVertexLayout> GltfPackedVertexLayoutMap::find(
  const char*                         name) const {
  auto entry = m_map.find(name);

  if (entry == m_map.end())
    return nullptr;

  return entry->second;
}



GltfMeshletBuilder::GltfMeshletBuilder(
        std::shared_ptr<GltfMeshPrimitive> primitive,
        GltfVertexLayout                inputLayout,
        std::shared_ptr<GltfPackedVertexLayout> packedLayout,
        std::shared_ptr<GltfMorphTargetMap> morphTargetMap,
  const meshopt_Meshlet&                meshlet)
: m_primitive     (std::move(primitive))
, m_packedLayout  (std::move(packedLayout))
, m_morphTargetMap(std::move(morphTargetMap))
, m_inputLayout   (std::move(inputLayout))
, m_meshlet       (meshlet) {

}


GltfMeshletBuilder::~GltfMeshletBuilder() {

}


void GltfMeshletBuilder::buildMeshlet(
  const uint8_t*                      primitiveIndices,
  const uint32_t*                     vertexIndices,
  const GltfVertex*                   vertexData) {
  // Set basic meshlet properties that we can already process
  m_metadata.header.vertexCount = m_meshlet.vertex_count;
  m_metadata.header.primitiveCount = m_meshlet.triangle_count;

  // Initialize joint index to be invalid so that task shaders
  // don't apply an incorrect transform for culling by accident.
  m_metadata.info.jointIndex = 0xffffu;

  // Load input vertex data for further processing
  std::vector<GltfVertex> inputVertices = loadVertices(vertexIndices, vertexData);

  // If possible, use local joint indices for the meshlet
  // and also assign a dominant joint for culling.
  bool canCull = processJoints(inputVertices.data());

  if (canCull)
    computeMeshletBounds(inputVertices.data(), primitiveIndices);

  // Read both shading and vertex data into local arrays
  std::vector<char> vertexBuffer = packVertices(
    GltfPackedVertexStream::eVertexData, inputVertices.data());

  std::vector<char> shadingBuffer = packVertices(
    GltfPackedVertexStream::eShadingData, inputVertices.data());

  // Compute dual index buffer by deduplicating vertex and shading data.
  // The data buffers are changed even if dual indexing is disabled, so
  // that case needs to be handled separately when building the buffer.
  std::vector<std::pair<uint8_t, uint8_t>> dualIndexData =
    computeDualIndexBuffer(vertexBuffer, shadingBuffer);

  // Generate morph target data. If the meshlet has any morph targets,
  // this will also adjust any culling parameters as necessary.
  std::vector<GfxMeshletMorphTargetInfo> morphTargets;
  std::vector<char> morphBuffer;

  m_metadata.header.morphTargetMask = processMorphTargets(
    morphTargets, morphBuffer, vertexIndices);

  // Build the actual meshlet buffer
  buildMeshletBuffer(
    primitiveIndices,
    vertexBuffer.data(),
    shadingBuffer.data(),
    dualIndexData.data(),
    morphTargets, morphBuffer);
}


std::vector<GltfVertex> GltfMeshletBuilder::loadVertices(
  const uint32_t*                     indices,
  const GltfVertex*                   vertices) {
  std::vector<GltfVertex> result(m_meshlet.vertex_count);

  for (uint32_t i = 0; i < m_meshlet.vertex_count; i++)
    result[i] = vertices[indices[i]];

  return result;
}


std::vector<char> GltfMeshletBuilder::packVertices(
        GltfPackedVertexStream        stream,
  const GltfVertex*                   vertices) {
  uint32_t stride = m_packedLayout->getStreamDataStride(stream);
  std::vector<char> result(stride * m_meshlet.vertex_count);

  m_packedLayout->processVertices(m_inputLayout,
    m_meshlet.vertex_count, vertices, nullptr,
    stream, result.data());

  return result;
}


void GltfMeshletBuilder::computeMeshletBounds(
  const GltfVertex*                   vertices,
  const uint8_t*                      indices) {
  auto position = m_inputLayout.findAttribute("POSITION");

  // Build local index buffer that we can pass to meshoptimizer
  std::vector<uint32_t> indexBuffer(m_meshlet.triangle_count * 3);

  for (size_t i = 0; i < indexBuffer.size(); i++)
    indexBuffer[i] = uint32_t(indices[i]);

  // Compute meshlet bounds
  meshopt_Bounds bounds = meshopt_computeClusterBounds(
    indexBuffer.data(), indexBuffer.size(),
    &vertices[0].f32[position->offset],
    m_meshlet.vertex_count, sizeof(GltfVertex));

  // Assign bounds to meshlet
  if (bounds.radius > 0.0f) {
    m_metadata.info.flags |= GfxMeshletCullFlag::eCullSphere;
    m_metadata.info.sphereCenter = Vector<float16_t, 3>(
      float16_t(bounds.center[0]),
      float16_t(bounds.center[1]),
      float16_t(bounds.center[2]));
    m_metadata.info.sphereRadius = float16_t(bounds.radius);
  }

  if (bounds.cone_cutoff < 1.0f) {
    m_metadata.info.flags |= GfxMeshletCullFlag::eCullCone;
    m_metadata.info.coneOrigin = Vector<float16_t, 3>(
      float16_t(bounds.cone_apex[0]),
      float16_t(bounds.cone_apex[1]),
      float16_t(bounds.cone_apex[2]));
    m_metadata.info.coneAxis = Vector<float16_t, 2>(
      float16_t(bounds.cone_axis[0]),
      float16_t(bounds.cone_axis[1]));
    m_metadata.info.coneCutoff = float16_t(bounds.cone_axis[2] >= 0.0f
      ? bounds.cone_cutoff : -bounds.cone_cutoff);
  }
}


std::vector<std::pair<uint8_t, uint8_t>> GltfMeshletBuilder::computeDualIndexBuffer(
        std::vector<char>&            vertexData,
        std::vector<char>&            shadingData) {
  std::vector<std::pair<uint8_t, uint8_t>> result(m_meshlet.vertex_count);

  uint32_t vertexStride = m_packedLayout->getStreamDataStride(GltfPackedVertexStream::eVertexData);
  uint32_t shadingStride = m_packedLayout->getStreamDataStride(GltfPackedVertexStream::eShadingData);

  uint32_t vertexDataCount = 0;
  uint32_t shadingDataCount = 0;

  for (uint32_t i = 0; i < m_meshlet.vertex_count; i++) {
    result[i] = std::make_pair(
      uint8_t(deduplicateData(vertexData, vertexStride, vertexDataCount, i)),
      uint8_t(deduplicateData(shadingData, shadingStride, shadingDataCount, i)));
  }

  vertexData.resize(vertexDataCount);
  shadingData.resize(shadingDataCount);

  // Only enable dual indexing if doing so allows us to save memory
  uint32_t oldDataSize = m_meshlet.vertex_count * (vertexStride + shadingStride);
  uint32_t newDataSize = vertexDataCount * vertexStride + shadingDataCount * shadingStride + 2 * m_meshlet.vertex_count;

  if (newDataSize <= oldDataSize) {
    m_metadata.header.flags |= GfxMeshletFlag::eDualIndex;
    m_metadata.header.vertexDataCount = vertexDataCount;
    m_metadata.header.shadingDataCount = shadingDataCount;
  } else {
    m_metadata.header.vertexDataCount = m_meshlet.vertex_count;
    m_metadata.header.shadingDataCount = m_meshlet.vertex_count;
  }

  return result;
}


uint32_t GltfMeshletBuilder::deduplicateData(
        std::vector<char>&            data,
        uint32_t                      stride,
        uint32_t&                     count,
        uint32_t                      index) {
  for (uint32_t i = 0; i < count; i++) {
    if (!std::memcmp(&data[stride * i], &data[stride * index], stride))
      return i;
  }

  if (count != index)
    std::memcpy(&data[stride * count], &data[stride * index], stride);

  return count++;
}


bool GltfMeshletBuilder::processJoints(
        GltfVertex*                   vertices) {
  constexpr float DominantJointThreshold = 0.9999f;

  // Find joint and joint weight attributes
  std::vector<std::pair<uint32_t, uint32_t>> attributeOffsets;

  for (uint32_t a = 0; ; a++) {
    auto joints = m_inputLayout.findAttribute(strcat("JOINTS_", a).c_str());
    auto weights = m_inputLayout.findAttribute(strcat("WEIGHTS_", a).c_str());

    if (!joints || !weights)
      break;

    // Joint attributes are vec4 in GLTF
    for (uint32_t c = 0; c < 4; c++) {
      attributeOffsets.push_back(std::make_pair(
        joints->offset + c, weights->offset + c));
    }
  }

  if (attributeOffsets.empty())
    return true;

  // Joint map and local joint indices. If the number of unique joints
  // used within the meshlet is small, we can use local joints.
  std::unordered_map<uint32_t, uint32_t> jointMap;
  std::vector<uint32_t> localJoints;

  // List of candidates for the dominant joint. A dominant joint is a
  // joint with a weight close to 1.0 for all vertices. If it is the
  // only joint used in the meshlet, we can still enable culling.
  std::unordered_set<uint32_t> dominantJoints;

  for (uint32_t v = 0; v < m_meshlet.vertex_count; v++) {
    for (const auto& a : attributeOffsets) {
      uint32_t j = vertices[v].u32[a.first];
      float    w = vertices[v].f32[a.second];

      // If the weight is zero, set the joint index to 0 so the data
      // compresses better. Mesh shaders must ignore joints with a
      // weight of zero so the actual index does not matter.
      if (w == 0.0f) {
        vertices[v].u32[a.first] = 0u;
        continue;
      }

      // Allocate a local joint index if necessary
      if (jointMap.emplace(j, localJoints.size()).second)
        localJoints.push_back(j);

      if (w >= DominantJointThreshold)
        dominantJoints.emplace(j);
    }
  }

  // If there are no joints with a non-zero index, treat the
  // meshlet as entirely static.
  if (localJoints.empty())
    return true;

  // For each dominant joint candidate, check whether it is truly
  // dominant for all vertices
  uint32_t dominantJoint = ~0u;

  for (uint32_t d : dominantJoints) {
    bool isDominant = true;

    for (uint32_t v = 0; v < m_meshlet.vertex_count; v++) {
      bool foundJoint = false;

      for (const auto& a : attributeOffsets) {
        uint32_t j = vertices[v].u32[a.first];
        float    w = vertices[v].f32[a.second];

        if (j == d) {
          foundJoint = true;

          if (!(isDominant &= (w >= DominantJointThreshold)))
            break;
        }
      }

      if (!(isDominant &= foundJoint))
        break;
    }

    if (isDominant) {
      if (dominantJoint == ~0u) {
        // Found a valid dominant joint
        dominantJoint = d;
      } else {
        // Multiple joints with maximum weight,
        // we can't really do much in that case
        dominantJoint = ~0u;
        break;
      }
    }
  }

  // Also, if the number of unique joints is sufficiently small,
  // enable local indexing for the meshlet. Set an invalid joint
  // index for any unused entry.
  if (localJoints.size() <= m_metadata.header.jointIndices.size()) {
    m_metadata.header.flags |= GfxMeshletFlag::eLocalJoints;

    for (size_t i = 0; i < m_metadata.header.jointIndices.size(); i++) {
      m_metadata.header.jointIndices[i] = (i < localJoints.size())
        ? uint16_t(localJoints[i])
        : uint16_t(0xffffu);
    }
  }

  // Repack joint indices for each vertex and order them by weight,
  // and resolve local indexing at the same time if enabled.
  std::vector<std::pair<uint32_t, float>> repackBuffer(attributeOffsets.size());

  for (uint32_t v = 0; v < m_meshlet.vertex_count; v++) {
    for (size_t a = 0; a < attributeOffsets.size(); a++) {
      repackBuffer[a] = std::make_pair(
        vertices[v].u32[attributeOffsets[a].first],
        vertices[v].f32[attributeOffsets[a].second]);
    }

    std::sort(repackBuffer.begin(), repackBuffer.end(),
      [] (const std::pair<uint32_t, float>& a, const std::pair<uint32_t, float>& b) {
        return a.second > b.second;
      });

    for (size_t a = 0; a < attributeOffsets.size(); a++) {
      uint32_t jointIndex = repackBuffer[a].first;
      float    jointWeight = repackBuffer[a].second;

      if ((m_metadata.header.flags & GfxMeshletFlag::eLocalJoints) && jointWeight != 0.0f) {
        auto entry = jointMap.find(jointIndex);
        dbg_assert(entry != jointMap.end());
        jointIndex = entry->second;
      }

      vertices[v].u32[attributeOffsets[a].first] = jointIndex;
      vertices[v].f32[attributeOffsets[a].second] = jointWeight;
    }
  }

  // Assign dominant joint to the meshlet. If there are multiple joints
  // or we do not have a dominant joint, disable culling for now.
  //
  // TODO: Find a way to allow culling anyway as long as a dominant
  //    joint could be found (animation keyframes or something?)
  m_metadata.info.jointIndex = uint16_t(dominantJoint);
  return dominantJoint != ~0u && localJoints.size() == 1;
}


uint32_t GltfMeshletBuilder::processMorphTargets(
        std::vector<GfxMeshletMorphTargetInfo>& morphTargets,
        std::vector<char>&            morphBuffer,
  const uint32_t*                     vertexIndices) {
  // Exit early if the final output does not store morph targets
  size_t morphDataStride = m_packedLayout->getStreamDataStride(
    GltfPackedVertexStream::eMorphData);

  if (!morphDataStride)
    return 0;

  // Initialize vertex data reader
  GltfVertexDataReader reader(m_primitive);
  GltfVertexLayout inputLayout = reader.getLayout();

  // Dummy buffer we can compare vertex data against. There are
  // more efficient solutions, but this way we can just memcmp.
  std::vector<char> zeroVertex(morphDataStride);

  // On input, morph targets may occur in any order so use this
  // fixed-size array as a reorder buffer. We only support 32
  // morph targets due to the morph target bit mask existing.
  std::array<GfxMeshletMorphTargetInfo, 32> targetList = { };

  // If positions are morphed, we will have to enlarge the bounding sphere
  float sphereRadiusDelta = 0.0f;

  // Iterate over morph targets for the current primitive, convert
  // their vertex data and check whether any of the deltas are zero.
  auto targets = m_primitive->getMorphTargets();

  for (auto t = targets.first; t != targets.second; t++) {
    auto target = *t;

    // Read morphed vertex attributes from the GLTF accessor
    std::vector<GltfVertex> vertices(m_meshlet.vertex_count);

    reader.readMorphedVertices(target,
      m_meshlet.vertex_count, vertexIndices, vertices.data());

    // Check whether the position attribute is morphed, and if so, find
    // the maximum vertex position delta to adjust the bounding sphere
    auto srcPosition = inputLayout.findAttribute("POSITION");
    auto dstPosition = m_packedLayout->findAttribute("POSITION");

    if (srcPosition && dstPosition && dstPosition->morph) {
      uint32_t o = srcPosition->offset;
      float maxDelta = 0.0f;

      for (const auto& v : vertices) {
        maxDelta = std::max(maxDelta, length(
          Vector3D(v.f32[o], v.f32[o + 1], v.f32[o + 2])));
      }

      sphereRadiusDelta += maxDelta;
    }

    // Find index of the current morph target
    auto entry = m_morphTargetMap->find(target->getName());
    dbg_assert(entry != m_morphTargetMap->end());
    auto& metadata = targetList.at(entry->second);
    metadata.dataIndex = morphBuffer.size() / morphDataStride;

    // Pack morphed vertex data and append all vertices
    // which have any non-zero data to the output.
    std::vector<char> morphData = packVertices(
      GltfPackedVertexStream::eMorphData, vertices.data());

    for (uint32_t v = 0; v < m_meshlet.vertex_count; v++) {
      if (!std::memcmp(&morphData[v * morphDataStride], &zeroVertex[0], morphDataStride))
        continue;

      metadata.vertexMask.at(v / 32) |= (1u << (v % 32));

      size_t offset = morphBuffer.size();
      morphBuffer.resize(offset + morphDataStride);

      std::memcpy(&morphBuffer[offset],
        &morphData[v * morphDataStride], morphDataStride);
    }
  }

  // Add morph targets with a non-zero vertex mask to the output
  // array in the correct order, and compute the morph target mask.
  uint32_t targetMask = 0u;

  for (uint32_t i = 0; i < targetList.size(); i++) {
    uint32_t vertexMaskAccum = 0;

    for (auto mask : targetList.at(i).vertexMask)
      vertexMaskAccum |= mask;

    if (vertexMaskAccum) {
      targetMask |= 1u << i;
      morphTargets.push_back(targetList.at(i));
    }
  }

  // Disable cone culling if any morph targets are enabled and
  // vertex positions are morphed, since face normals may change
  // significantly. Also enlarge bounding sphere as necessary.
  if (targetMask && sphereRadiusDelta > 0.0f) {
    m_metadata.info.flags -= GfxMeshletCullFlag::eCullCone;
    m_metadata.info.coneOrigin = Vector<float16_t, 3>(0.0_f16);
    m_metadata.info.coneAxis = Vector<float16_t, 2>(0.0_f16);
    m_metadata.info.coneCutoff = float16_t(1.0f);

    if (m_metadata.info.flags & GfxMeshletCullFlag::eCullSphere) {
      float sphereRadius = float(m_metadata.info.sphereRadius);
      m_metadata.info.sphereRadius = float16_t(sphereRadius + sphereRadiusDelta);
    }
  }

  return targetMask;
}


void GltfMeshletBuilder::buildMeshletBuffer(
  const uint8_t*                      primitiveIndices,
  const char*                         vertexData,
  const char*                         shadingData,
  const std::pair<uint8_t, uint8_t>*  dualIndexData,
  const std::vector<GfxMeshletMorphTargetInfo>& morphTargets,
  const std::vector<char>&            morphBuffer) {
  uint16_t offset = 0;
  allocateStorage(offset, sizeof(m_metadata.header));

  // Dual index data is always accessed first
  if (m_metadata.header.flags & GfxMeshletFlag::eDualIndex) {
    m_metadata.header.dualIndexOffset = allocateStorage(offset,
      m_meshlet.vertex_count * 2);
  }

  // Generally followed by vertex data. Ignore morph targets for
  // now, if those are used then access patterns are weird anyway.
  uint32_t vertexStride = m_packedLayout->getStreamDataStride(GltfPackedVertexStream::eVertexData);

  if (vertexStride) {
    m_metadata.header.vertexDataOffset = allocateStorage(
      offset, m_metadata.header.vertexDataCount * vertexStride);
  }

  // Index data is used for primitive culling after vertex
  // positions are computed, so we need it next.
  m_metadata.header.primitiveOffset = allocateStorage(offset,
    m_meshlet.triangle_count * sizeof(GfxMeshletPrimitive));

  // Put shading data last since it is only used to compute
  // fragment shader inputs.
  uint32_t shadingStride = m_packedLayout->getStreamDataStride(GltfPackedVertexStream::eShadingData);

  if (shadingStride) {
    m_metadata.header.shadingDataOffset = allocateStorage(
      offset, m_metadata.header.shadingDataCount * shadingStride);
  }

  // Allocate storage for morph target metadata, as well as
  // the morph data buffer.
  if (!morphTargets.empty()) {
    m_metadata.header.morphTargetOffset = allocateStorage(
      offset, morphTargets.size() * sizeof(GfxMeshletMorphTargetInfo));
    m_metadata.header.morphDataOffset = allocateStorage(
      offset, morphBuffer.size());
  }

  // Allocate buffer and write the header
  m_buffer.resize(offset * 16);
  std::memcpy(&m_buffer[0], &m_metadata.header, sizeof(m_metadata.header));

  // Write out vertex and shading data
  char* dstVertexData = &m_buffer[m_metadata.header.vertexDataOffset * 16];
  char* dstShadingData = &m_buffer[m_metadata.header.shadingDataOffset * 16];

  if (m_metadata.header.flags & GfxMeshletFlag::eDualIndex) {
    // If dual indexing is enabled, we can copy vertex data as it is
    std::memcpy(dstVertexData, vertexData, m_metadata.header.vertexDataCount * vertexStride);
    std::memcpy(dstShadingData, shadingData, m_metadata.header.shadingDataCount * shadingStride);

    auto dstDualIndexData = reinterpret_cast<uint8_t*>(&m_buffer[m_metadata.header.dualIndexOffset * 16]);

    for (uint32_t i = 0; i < m_meshlet.vertex_count; i++) {
      dstDualIndexData[2 * i + 0] = dualIndexData[i].first;
      dstDualIndexData[2 * i + 1] = dualIndexData[i].second;
    }
  } else {
    // Otherwise, we need to resolve the dual index buffer by hand
    for (uint32_t i = 0; i < m_meshlet.vertex_count; i++) {
      auto d = dualIndexData[i];
      std::memcpy(&dstVertexData[i * vertexStride], &vertexData[d.first * vertexStride], vertexStride);
      std::memcpy(&dstShadingData[i * shadingStride], &shadingData[d.second * shadingStride], shadingStride);
    }
  }

  // Write out primitive data
  auto dstPrimitiveData = reinterpret_cast<GfxMeshletPrimitive*>(
    &m_buffer[m_metadata.header.primitiveOffset * 16]);

  for (uint32_t i = 0; i < m_meshlet.triangle_count; i++) {
    dstPrimitiveData[i] = GfxMeshletPrimitive(
      primitiveIndices[3 * i + 0],
      primitiveIndices[3 * i + 1],
      primitiveIndices[3 * i + 2]);
  }

  // Write out morph target data
  if (!morphTargets.empty()) {
    auto dstMorphTargetMetadata = reinterpret_cast<GfxMeshletMorphTargetInfo*>(
      &m_buffer[m_metadata.header.morphTargetOffset * 16]);
    auto dstMorphTargetData = &m_buffer[m_metadata.header.morphDataOffset * 16];

    for (size_t i = 0; i < morphTargets.size(); i++)
      dstMorphTargetMetadata[i] = morphTargets.at(i);

    std::memcpy(dstMorphTargetData, morphBuffer.data(), morphBuffer.size());
  }
}


uint16_t GltfMeshletBuilder::allocateStorage(
        uint16_t&                       allocator,
        size_t                          amount) {
  // All offsets are in units of 16 bytes
  uint16_t offset = allocator;
  allocator += uint16_t((amount + 15) / 16);
  return offset;
}




GltfMeshPrimitiveConverter::GltfMeshPrimitiveConverter(
        std::shared_ptr<GltfPackedVertexLayout> layout,
        std::shared_ptr<GltfMeshPrimitive> primitive,
        std::shared_ptr<GltfMorphTargetMap> morphTargetMap)
: m_layout          (std::move(layout))
, m_primitive       (std::move(primitive))
, m_morphTargetMap  (std::move(morphTargetMap)) {

}


GltfMeshPrimitiveConverter::~GltfMeshPrimitiveConverter() {

}


Job GltfMeshPrimitiveConverter::dispatchConvert(
  const Jobs&                         jobs) {
  Job processJob = jobs->create<BatchJob>([
    cThis = shared_from_this()
  ] (uint32_t index) {
    cThis->buildMeshlet(index);
  }, 0, 1);

  Job setupJob = jobs->dispatch(jobs->create<SimpleJob>([
    cThis       = shared_from_this(),
    cProcessJob = processJob
  ] {
    cThis->readPrimitiveData();
    cThis->generateMeshlets();

    cProcessJob->setWorkItemCount(cThis->m_meshlets.size());
  }));

  jobs->dispatch(processJob, setupJob);
  return processJob;
}


GfxAabb<float> GltfMeshPrimitiveConverter::computeAabb(
        QuatTransform                 transform) const {
  Vector4D lo = Vector4D(0.0f);
  Vector4D hi = Vector4D(0.0f);

  // Compute tight bounding box for transformed vertices
  auto position = m_inputLayout.findAttribute("POSITION");

  for (size_t i = 0; i < m_sourceVertexBuffer.size(); i++) {
    const float* f = &m_sourceVertexBuffer[i].f32[position->offset];

    Vector4D pos = transform.apply(Vector4D(f[0], f[1], f[2], 0.0f));

    if (i) {
      lo = min(lo, pos);
      hi = max(hi, pos);
    } else {
      lo = pos;
      hi = pos;
    }
  }

  // If necessary, expand bounding box by including the
  // full bounding spheres or morphed meshlets.
  for (const auto& meshlet : m_meshlets) {
    auto metadata = meshlet->getMetadata();

    if (metadata.header.morphTargetMask) {
      float radius = transform.getRotation().scaling() * float(metadata.info.sphereRadius);
      Vector4D pos = transform.apply(Vector4D(metadata.info.sphereCenter, 0.0f));

      lo = min(lo, pos - radius);
      hi = max(hi, pos + radius);
    }
  }

  return GfxAabb<float>(
    lo.get<0, 1, 2>(),
    hi.get<0, 1, 2>());
}


void GltfMeshPrimitiveConverter::readPrimitiveData() {
  GltfVertexDataReader reader(m_primitive);

  m_sourceIndexBuffer.resize(reader.countIndices());
  m_sourceVertexBuffer.resize(reader.countVertices());

  reader.readIndices(m_sourceIndexBuffer.data());
  reader.readVertices(m_sourceVertexBuffer.data());

  m_inputLayout = reader.getLayout();
}


void GltfMeshPrimitiveConverter::generateMeshlets() {
  constexpr uint32_t maxVertexCount = 128;
  constexpr uint32_t maxPrimitiveCount = 128;

  auto position = m_inputLayout.findAttribute("POSITION");

  // Figure out an upper bound for the number of meshlets
  size_t meshletCount = meshopt_buildMeshletsBound(
    m_sourceIndexBuffer.size(), maxVertexCount, maxPrimitiveCount);

  m_meshletMetadata.resize(meshletCount);
  m_meshletIndexBuffer.resize(meshletCount * maxPrimitiveCount * 3);
  m_meshletVertexIndices.resize(meshletCount * maxVertexCount);

  meshletCount = meshopt_buildMeshlets(
    m_meshletMetadata.data(),
    m_meshletVertexIndices.data(),
    m_meshletIndexBuffer.data(),
    m_sourceIndexBuffer.data(),
    m_sourceIndexBuffer.size(),
    &m_sourceVertexBuffer[0].f32[position->offset],
    m_sourceVertexBuffer.size(), sizeof(GltfVertex),
    maxVertexCount, maxPrimitiveCount, 0.85f);

  // Already allocate the meshlet builder array so
  // that the caller doesn't have to worry about it
  m_meshlets.resize(meshletCount);
}


void GltfMeshPrimitiveConverter::buildMeshlet(
        uint32_t                      meshlet) {
  meshopt_Meshlet m = m_meshletMetadata[meshlet];

  auto builder = std::make_shared<GltfMeshletBuilder>(
    m_primitive, m_inputLayout, m_layout, m_morphTargetMap, m);

  builder->buildMeshlet(
    &m_meshletIndexBuffer[m.triangle_offset],
    &m_meshletVertexIndices[m.vertex_offset],
    m_sourceVertexBuffer.data());

  m_meshlets[meshlet] = std::move(builder);
}



GltfMeshLodConverter::GltfMeshLodConverter(
        std::shared_ptr<GltfMesh>     mesh,
        std::shared_ptr<GltfPackedVertexLayout> layout)
: m_maxDistance (mesh->getMaxDistance())
, m_mesh        (std::move(mesh))
, m_layout      (std::move(layout)) {

}


GltfMeshLodConverter::~GltfMeshLodConverter() {

}


GfxMeshLodMetadata GltfMeshLodConverter::getMetadata() const {
  GfxMeshLodMetadata result;
  result.info.maxDistance = float16_t(m_maxDistance);
  result.info.meshletCount = uint32_t(m_meshlets.size());

  return result;
}


void GltfMeshLodConverter::addPrimitive(
        std::shared_ptr<GltfMeshPrimitive> primitive,
        std::shared_ptr<GltfMorphTargetMap> morphTargetMap) {
  m_primitives.push_back(
    std::make_shared<GltfMeshPrimitiveConverter>(
      m_layout, std::move(primitive), std::move(morphTargetMap)));
}


Job GltfMeshLodConverter::dispatchConvert(
  const Jobs&                         jobs) {
  std::vector<Job> primitiveJobs;
  primitiveJobs.reserve(m_primitives.size());

  for (const auto& prim : m_primitives)
    primitiveJobs.push_back(prim->dispatchConvert(jobs));

  Job accumulateJob = jobs->create<SimpleJob>(
    [cThis = shared_from_this()] {
      cThis->accumulateMeshlets();
    });

  return jobs->dispatch(accumulateJob,
    std::make_pair(primitiveJobs.begin(), primitiveJobs.end()));
}


GfxAabb<float> GltfMeshLodConverter::computeAabb(
        QuatTransform                 transform) const {
  if (m_primitives.empty())
    return GfxAabb<float>();

  auto result = m_primitives[0]->computeAabb(transform);

  for (size_t i = 1; i < m_primitives.size(); i++) {
    auto aabb = m_primitives[i]->computeAabb(transform);
    result.min = min(result.min, aabb.min);
    result.max = max(result.max, aabb.max);
  }

  return result;
}


void GltfMeshLodConverter::accumulateMeshlets() {
  // Flatten meshlet array for the entire LOD and compute the
  // total size of meshlet data within the data buffer.
  for (const auto& prim : m_primitives) {
    for (uint32_t i = 0; i < prim->getMeshletCount(); i++) {
      m_meshlets.push_back(prim->getMeshlet(i));
      m_dataSize += m_meshlets.back()->getBuffer().getSize();
      m_dataSize += sizeof(GfxMeshletInfo);
    }
  }
}




GltfMeshConverter::GltfMeshConverter(
        std::shared_ptr<GltfMesh>     mesh,
        std::shared_ptr<GltfMaterial> material,
        std::shared_ptr<GltfPackedVertexLayout> layout)
: m_mesh      (std::move(mesh))
, m_material  (std::move(material))
, m_layout    (std::move(layout)) {

}


GltfMeshConverter::~GltfMeshConverter() {

}


GfxMeshMetadata GltfMeshConverter::getMetadata() const {
  GfxMeshMetadata result;
  result.name = m_mesh->getName();
  result.info.lodCount = uint8_t(m_lods.size());
  result.info.instanceCount = uint8_t(m_instances.size());
  result.info.skinJoints = m_jointCountPerSkin;

  for (const auto& lod : m_lods) {
    result.info.maxMeshletCount = std::max(
      result.info.maxMeshletCount, uint16_t(lod->getMeshletCount()));
  }

  if (!m_lods.empty()) {
    result.info.minDistance = float16_t(m_mesh->getMinDistance());
    result.info.maxDistance = m_lods.front()->getMetadata().info.maxDistance;
  }

  return result;
}


bool GltfMeshConverter::isSameMeshMaterial(
  const std::shared_ptr<GltfMesh>&    mesh,
  const std::shared_ptr<GltfPackedVertexLayout>& layout) const {
  // Never accept a different material
  if (m_layout != layout)
    return false;

  // Always accept the same mesh
  if (m_mesh == mesh)
    return true;

  // Accept if the mesh is a lower LOD of this mesh
  return mesh->getParentName() == m_mesh->getName();
}


void GltfMeshConverter::addPrimitive(
  const std::shared_ptr<GltfMesh>&    mesh,
        std::shared_ptr<GltfMeshPrimitive> primitive,
        std::shared_ptr<GltfMorphTargetMap> morphTargetMap) {
  // Scan existing LODs for one that uses the same maximum
  // view distance as the primitive's parent mesh
  for (const auto& lod : m_lods) {
    if (lod->isSameLod(mesh)) {
      lod->addPrimitive(std::move(primitive), std::move(morphTargetMap));
      return;
    }
  }

  // Create new LOD for the parent mesh as necessary
  auto lod = std::make_shared<GltfMeshLodConverter>(mesh, m_layout);
  lod->addPrimitive(std::move(primitive), std::move(morphTargetMap));

  m_lods.push_back(std::move(lod));
}


void GltfMeshConverter::addInstance(
  const std::shared_ptr<GltfNode>&    node) {
  m_nodes.push_back(std::move(node));
}


void GltfMeshConverter::applySkins(
  const std::unordered_map<std::shared_ptr<GltfNode>, uint32_t>& jointIndexMap) {
  for (const auto& node : m_nodes) {
    std::shared_ptr<GltfSkin> skin = node->getSkin();

    if (skin == nullptr || m_skinOffsets.find(skin) != m_skinOffsets.end())
      continue;

    // For each unique skin, look up the absolute joint indices
    uint16_t skinOffset = uint16_t(m_jointIndices.size());
    uint16_t skinJoints = 0;

    auto joints = skin->getJoints();

    for (auto j = joints.first; j != joints.second; j++) {
      auto entry = jointIndexMap.find(*j);
      dbg_assert(entry != jointIndexMap.end());

      m_jointIndices.push_back(entry->second);

      skinJoints += 1;
    }

    // Be conservative here since we do not store the total size
    // of the joint index array anywhere
    if (m_jointCountPerSkin && m_jointCountPerSkin != skinJoints)
      Log::err("Skins assigned to the mesh ", m_mesh->getName(), " have different joint counts");

    m_jointCountPerSkin = m_jointCountPerSkin
      ? std::min(m_jointCountPerSkin, skinJoints)
      : skinJoints;

    m_skinOffsets.emplace(skin, skinOffset);
  }
}


Job GltfMeshConverter::dispatchConvert(
  const Jobs&                         jobs) {
  std::vector<Job> lodJobs;
  lodJobs.reserve(m_lods.size());

  for (const auto& lod : m_lods)
    lodJobs.push_back(lod->dispatchConvert(jobs));

  Job instanceJob = jobs->create<SimpleJob>(
    [cThis = shared_from_this()] {
      cThis->accumulateLods();
      cThis->processInstances();
    });

  return jobs->dispatch(instanceJob,
    std::make_pair(lodJobs.begin(), lodJobs.end()));
}


GfxAabb<float> GltfMeshConverter::computeAabb() const {
  auto result = GfxAabb<float>();

  for (size_t i = 0; i < m_instances.size() || !i; i++) {
    QuatTransform instanceTransform = QuatTransform::identity();

    if (i < m_instances.size()) {
      instanceTransform = QuatTransform(
        Vector4D(m_instances[i].info.transform),
        Vector4D(m_instances[i].info.translate, 0.0f));
    }

    for (size_t j = 0; j < m_lods.size(); j++) {
      auto aabb = m_lods[j]->computeAabb(instanceTransform);

      if (i + j) {
        result.min = min(result.min, aabb.min);
        result.max = max(result.max, aabb.max);
      } else {
        result = aabb;
      }
    }
  }

  return result;
}


void GltfMeshConverter::accumulateLods() {
  // Just order LODs by distance, not much else to do here
  std::sort(m_lods.begin(), m_lods.end(), [] (
    const std::shared_ptr<GltfMeshLodConverter>& a,
    const std::shared_ptr<GltfMeshLodConverter>& b) {
    return a->isOrderedBefore(*b);
  });
}


void GltfMeshConverter::processInstances() {
  m_instances.reserve(m_nodes.size());

  uint16_t dummySkinOffset = 0;

  for (const auto& node : m_nodes) {
    QuatTransform transform = node->computeTransform();

    GfxMeshInstanceMetadata& instance = m_instances.emplace_back();
    instance.name = node->getName();
    instance.info.transform = transform.getRotation().getVector();
    instance.info.translate = Vector3D(transform.getTranslation());
    instance.instanceIndex = uint32_t(m_instances.size());

    if (m_jointCountPerSkin) {
      std::shared_ptr<GltfSkin> skin = node->getSkin();

      if (skin != nullptr) {
        auto entry = m_skinOffsets.find(skin);
        dbg_assert(entry != m_skinOffsets.end());

        instance.info.jointIndex = entry->second;
      } else {
        // If this happens, just add a list of invalid joint
        // indices, shaders are responsible for bound-checking.
        Log::err("No skin assigned to instance ", node->getName(), " of skinned mesh ", m_mesh->getName());

        if (!dummySkinOffset) {
          dummySkinOffset = uint16_t(m_jointIndices.size());

          for (size_t i = 0; i < m_jointCountPerSkin; i++)
            m_jointIndices.push_back(0xffffu);
        }

        instance.info.jointIndex = dummySkinOffset;
      }
    }
  }
}




GltfConverter::GltfConverter(
        Jobs                          jobs,
        std::shared_ptr<Gltf>         asset,
        std::shared_ptr<GltfPackedVertexLayoutMap> layouts)
: m_jobs            (std::move(jobs))
, m_asset           (std::move(asset))
, m_layouts         (std::move(layouts))
, m_morphTargetMap  (std::make_shared<GltfMorphTargetMap>()) {

}


GltfConverter::~GltfConverter() {

}


Job GltfConverter::dispatchConvert() {
  auto meshes = m_asset->getMeshes();
  auto nodes = m_asset->getNodes();

  // Add meshes with no parent mesh
  for (auto m = meshes.first; m != meshes.second; m++) {
    if ((*m)->getParentName().empty())
      addMesh(*m);
  }

  // Add meshes that are LODs of others
  for (auto m = meshes.first; m != meshes.second; m++) {
    if (!(*m)->getParentName().empty())
      addMesh(*m);
  }

  // Iterate over nodes again and add instances
  for (auto n = nodes.first; n != nodes.second; n++) {
    auto mesh = (*n)->getMesh();

    if (mesh != nullptr) {
      if (mesh->getParentName().empty())
        addMeshInstance(*n);
    }
  }

  // Remap joints so that they are ordered correctly, and
  // apply remapped joint indices to mesh instances
  computeJointIndices();

  for (const auto& converter : m_meshConverters)
    converter->applySkins(m_jointIndices);

  // Dispatch actual mesh conversion jobs
  std::vector<Job> dependencies;

  for (const auto& converter : m_meshConverters)
    dependencies.push_back(converter->dispatchConvert(m_jobs));

  // Dispatch final job that creates the geometry
  // object as well as all the mesh buffers.
  Job buildGeometryJob = m_jobs->create<SimpleJob>(
    [cThis = shared_from_this()] {
      cThis->buildGeometry();
    });

  return m_jobs->dispatch(buildGeometryJob,
    std::make_pair(dependencies.begin(), dependencies.end()));
}


void GltfConverter::buildGeometry() {
  // Meshlet metadata buffer
  std::vector<GfxMeshletMetadata> meshlets;

  // Compute bounding box from source vertex data
  GfxAabb<float> aabb = computeAabb();

  m_geometry = std::make_shared<GfxGeometry>();
  m_geometry->info.aabb = GfxAabb<float16_t>(
    Vector<float16_t, 3>(aabb.min),
    Vector<float16_t, 3>(aabb.max));
  m_geometry->info.meshCount = uint8_t(m_meshConverters.size());
  m_geometry->info.bufferCount = 1;

  // Do an initial pass over all LODs to find the number of data buffers.
  // Mostly useful because we want the buffer pointer array to be stored
  // close to mesh and geometry metadata for cache efficiency reasons.
  for (const auto& converter : m_meshConverters) {
    for (uint32_t i = 0; i < converter->getMetadata().info.lodCount; i++) {
      auto lod = converter->getLodConverter(i);

      m_geometry->info.bufferCount = std::max(m_geometry->info.bufferCount,
        uint16_t(lod->getMetadata().info.bufferIndex + 1));
    }
  }

  // Initialize metadata allocator with the size of the overall geometry
  // header. This consists of the geometry info, immediately followed by
  // a tightly packed array of mesh metadata.
  uint32_t bufferOffset = 0;

  allocateStorage(bufferOffset,
    sizeof(GfxGeometryInfo) +
    sizeof(GfxMeshInfo) * m_meshConverters.size());

  // Allocate storage for buffer pointers. Buffer 0 is stored with the
  // metadata buffer, so we don't need to allocate a pointer for that.
  if (m_geometry->info.bufferCount > 1) {
    m_geometry->info.bufferPointerOffset = allocateStorage(
      bufferOffset, sizeof(uint64_t) * m_geometry->info.bufferCount - 1);
  }

  // Allocate storage for joint positions and assign joint metadata.
  m_geometry->info.jointCount = uint16_t(m_jointMetadata.size());
  m_geometry->joints = m_jointMetadata;

  if (m_geometry->info.jointCount) {
    m_geometry->info.jointDataOffset = allocateStorage(
      bufferOffset, sizeof(GfxJoint) * m_geometry->info.jointCount);
  }

  // Number of meshlets per buffer
  std::vector<uint32_t> bufferMeshletCount(m_geometry->info.bufferCount);
  std::vector<uint32_t> bufferDataSizes(m_geometry->info.bufferCount);

  // Iterate over meshes and add mesh metadata. At this point we
  // can also start allocating storage for the metadata buffer.
  for (const auto& converter : m_meshConverters) {
    GfxMeshMetadata meshMetadata = converter->getMetadata();
    meshMetadata.meshIndex = uint32_t(m_geometry->meshes.size());
    meshMetadata.lodMetadataIndex = uint32_t(m_geometry->lods.size());
    meshMetadata.instanceDataIndex = uint32_t(m_geometry->instances.size());

    meshMetadata.info.materialIndex = getMaterialIndex(converter->getMaterial());

    m_geometry->info.materialCount = std::max(
      uint8_t(m_geometry->info.materialCount),
      uint8_t(meshMetadata.info.materialIndex + 1));

    if (meshMetadata.info.skinJoints) {
      meshMetadata.info.skinDataOffset = allocateStorage(
        bufferOffset, sizeof(uint16_t) * converter->getJointIndexArraySize());
    }

    if (meshMetadata.info.lodCount) {
      meshMetadata.info.lodInfoOffset = allocateStorage(bufferOffset,
        sizeof(GfxMeshLod) * meshMetadata.info.lodCount);
    }

    if (meshMetadata.info.instanceCount) {
      meshMetadata.info.instanceDataOffset = allocateStorage(bufferOffset,
        sizeof(GfxMeshInstance) * meshMetadata.info.instanceCount);
    }

    m_geometry->meshes.push_back(meshMetadata);

    // Iterate over LODs and accumulate the number of meshlets
    // and the meshlet data size for every defined data buffer.
    for (uint32_t i = 0; i < meshMetadata.info.lodCount; i++) {
      auto lod = converter->getLodConverter(i);

      GfxMeshLodMetadata lodMetadata = lod->getMetadata();
      lodMetadata.firstMeshletIndex = meshlets.size();
      lodMetadata.info.meshletIndex = bufferMeshletCount.at(lodMetadata.info.bufferIndex);

      bufferMeshletCount.at(lodMetadata.info.bufferIndex) += lodMetadata.info.meshletCount;

      m_geometry->lods.push_back(lodMetadata);

      // Iterate over meshlets and add their size to the buffer size.
      // Also add meshlet metadata, but the computed offset will not
      // include the total metadata offset yet.
      for (uint32_t j = 0; j < lodMetadata.info.meshletCount; j++) {
        auto meshlet = lod->getMeshlet(j);

        GfxMeshletMetadata meshletMetadata = meshlet->getMetadata();
        meshletMetadata.info.dataOffset = allocateStorage(
          bufferDataSizes.at(lodMetadata.info.bufferIndex),
          meshlet->getBuffer().getSize());

        meshlets.push_back(meshletMetadata);
      }
    }

    // Iterate over instances and add metadata
    for (uint32_t i = 0; i < meshMetadata.info.instanceCount; i++) {
      auto instanceMetadata = converter->getInstanceMetadata(i);
      instanceMetadata.meshIndex = meshMetadata.meshIndex;

      m_geometry->instances.push_back(instanceMetadata);
    }
  }

  // Add morph target metadata to the geometry object
  m_geometry->info.morphTargetCount = uint8_t(m_morphTargetMap->size());
  m_geometry->morphTargets.resize(m_geometry->info.morphTargetCount);

  for (const auto& t : *m_morphTargetMap) {
    GfxMorphTargetMetadata morphTarget;
    morphTarget.name = t.first;
    morphTarget.morphTargetIndex = t.second;

    m_geometry->morphTargets.at(morphTarget.morphTargetIndex) = morphTarget;
  }

  // At this point, all non-meshlet metadata is accounted
  // for, so we can compute the final buffer sizes.
  m_geometry->info.meshletDataOffset = bufferOffset;

  // Compute meshlet buffer metadata size
  std::vector<uint32_t> bufferMetadataSizes(m_geometry->info.bufferCount);

  for (uint32_t i = 0; i < m_geometry->info.bufferCount; i++) {
    uint32_t metadataSize = 0u;
    allocateStorage(metadataSize, sizeof(GfxMeshletInfo) * bufferMeshletCount.at(i));

    bufferMetadataSizes.at(i) = metadataSize;
    bufferDataSizes.at(i) += metadataSize;
  }

  // At this point, we know the total size of the metadata
  // buffer as well as all meshlet data buffers. Do another
  // pass over all meshlets and fix up the data offsets.
  m_geometry->meshletOffsets.resize(meshlets.size());

  for (size_t i = 0; i < m_geometry->info.meshCount; i++) {
    const auto& meshMetadata = m_geometry->meshes.at(i);

    for (size_t j = 0; j < meshMetadata.info.lodCount; j++) {
      const auto& lodMetadata = m_geometry->lods.at(meshMetadata.lodMetadataIndex + j);
      uint32_t lodBufferOffset = bufferMetadataSizes.at(lodMetadata.info.bufferIndex);

      for (size_t k = 0; k < lodMetadata.info.meshletCount; k++) {
        auto& meshletMetadata = meshlets.at(lodMetadata.firstMeshletIndex + k);
        meshletMetadata.info.dataOffset += lodBufferOffset;

        m_geometry->meshletOffsets.at(lodMetadata.firstMeshletIndex + k) =
          meshletMetadata.info.dataOffset + meshletMetadata.header.vertexDataOffset +
          (lodMetadata.info.bufferIndex ? 0u : m_geometry->info.meshletDataOffset);
      }
    }
  }

  // Allocate buffer storage and write out buffer data
  m_buffers.resize(m_geometry->info.bufferCount);

  for (uint32_t i = 0; i < m_geometry->info.bufferCount; i++) {
    uint32_t headerSize = i ? 0u : m_geometry->info.meshletDataOffset;
    m_buffers.at(i).resize(headerSize + bufferDataSizes.at(i));
  }

  buildBuffers(meshlets);
}


void GltfConverter::buildBuffers(
  const std::vector<GfxMeshletMetadata>& meshlets) {
  writeBufferData(0, 0, &m_geometry->info, sizeof(m_geometry->info));

  for (size_t i = 0; i < m_geometry->info.meshCount; i++) {
    const auto& meshConverter = m_meshConverters.at(i);
    const auto& meshMetadata = m_geometry->meshes.at(i);

    writeBufferData(0, sizeof(m_geometry->info) +
      sizeof(meshMetadata.info) * i,
      &meshMetadata.info, sizeof(meshMetadata.info));

    for (size_t j = 0; j < meshMetadata.info.lodCount; j++) {
      const auto& lodConverter = meshConverter->getLodConverter(j);
      const auto& lodMetadata = m_geometry->lods.at(meshMetadata.lodMetadataIndex + j);

      writeBufferData(0, meshMetadata.info.lodInfoOffset +
        sizeof(lodMetadata.info) * j,
        &lodMetadata.info, sizeof(lodMetadata.info));

      uint32_t meshletDataOffset = lodMetadata.info.bufferIndex
        ? 0u : m_geometry->info.meshletDataOffset;

      for (size_t k = 0; k < lodMetadata.info.meshletCount; k++) {
        const auto& meshletConverter = lodConverter->getMeshlet(k);
        const auto& meshletMetadata = meshlets.at(lodMetadata.firstMeshletIndex + k);

        writeBufferData(lodMetadata.info.bufferIndex, meshletDataOffset +
          sizeof(meshletMetadata.info) * (lodMetadata.info.meshletIndex + k),
          &meshletMetadata.info, sizeof(meshletMetadata.info));

        writeBufferData(lodMetadata.info.bufferIndex,
          meshletMetadata.info.dataOffset + meshletDataOffset,
          meshletConverter->getBuffer().getData(),
          meshletConverter->getBuffer().getSize());
      }
    }

    for (size_t j = 0; j < meshMetadata.info.instanceCount; j++) {
      const auto& instanceMetadata = m_geometry->instances.at(meshMetadata.instanceDataIndex + j);

      writeBufferData(0, meshMetadata.info.instanceDataOffset +
        sizeof(instanceMetadata.info) * j,
        &instanceMetadata.info, sizeof(instanceMetadata.info));
    }

    for (size_t j = 0; j < meshConverter->getJointIndexArraySize(); j++) {
      uint16_t jointIndex = meshConverter->getJointIndex(j);

      writeBufferData(0, meshMetadata.info.skinDataOffset +
        sizeof(jointIndex) * j, &jointIndex, sizeof(jointIndex));
    }
  }

  for (size_t i = 0; i < m_geometry->info.jointCount; i++) {
    const auto& jointMetatata = m_geometry->joints.at(i);

    writeBufferData(0, m_geometry->info.jointDataOffset +
      sizeof(jointMetatata.info) * i,
      &jointMetatata.info, sizeof(jointMetatata.info));
  }
}


GfxAabb<float> GltfConverter::computeAabb() const {
  if (m_meshConverters.empty())
    return GfxAabb<float>();

  auto result = m_meshConverters[0]->computeAabb();

  for (size_t i = 1; i < m_meshConverters.size(); i++) {
    auto aabb = m_meshConverters[i]->computeAabb();
    result.min = min(result.min, aabb.min);
    result.max = max(result.max, aabb.max);
  }

  return result;
}


std::shared_ptr<GltfPackedVertexLayout> GltfConverter::getMaterialLayout(
  const std::shared_ptr<GltfMaterial>& material) {
  auto result = m_layouts->find(material->getName().c_str());

  if (result == nullptr) {
    Log::err("No vertex layout found for material ", material->getName());
    return m_layouts->find("default");
  }

  return result;
}


uint32_t GltfConverter::getMaterialIndex(
  const std::shared_ptr<GltfMaterial>& material) {
  auto entry = m_materialIndices.find(material);

  if (entry != m_materialIndices.end())
    return entry->second;

  // Look up vertex layout for material. We shouldn't ever hit
  // this being null since mesh conversion requires a layout.
  auto layout = getMaterialLayout(material);
  dbg_assert(layout != nullptr);

  // Add material to the geometry object
  GfxMeshMaterialMetadata metadata = layout->getMetadata();
  metadata.materialIndex = uint32_t(m_geometry->materials.size());
  metadata.attributeIndex = uint32_t(m_geometry->attributes.size());

  m_geometry->materials.push_back(metadata);

  // Add all vertex attributes to the geometry object
  auto attributes = layout->getAttributes();

  for (auto a = attributes.first; a != attributes.second; a++)
    m_geometry->attributes.push_back(*a);

  // Add material to the lookup table
  m_materialIndices.emplace(material, metadata.materialIndex);
  return metadata.materialIndex;
}


std::shared_ptr<GltfMeshConverter> GltfConverter::getMeshConverter(
  const std::shared_ptr<GltfMesh>&    mesh,
  const std::shared_ptr<GltfMaterial>& material) {
  auto layout = getMaterialLayout(material);

  if (layout == nullptr)
    return nullptr;

  // Scan list of mesh converters
  for (const auto& converter : m_meshConverters) {
    if (converter->isSameMeshMaterial(mesh, layout))
      return converter;
  }

  // Create new converter as necessary
  auto converter = std::make_shared<GltfMeshConverter>(mesh, material, std::move(layout));
  m_meshConverters.push_back(converter);
  return converter;
}


void GltfConverter::addMesh(
  const std::shared_ptr<GltfMesh>&    mesh) {
  auto primitives = mesh->getPrimitives();

  for (auto p = primitives.first; p != primitives.second; p++) {
    std::shared_ptr<GltfMeshPrimitive> primitive = *p;
    auto converter = getMeshConverter(mesh, primitive->getMaterial());

    if (converter != nullptr) {
      auto morphTargets = primitive->getMorphTargets();

      for (auto t = morphTargets.first; t != morphTargets.second; t++)
        m_morphTargetMap->emplace((*t)->getName(), m_morphTargetMap->size());

      converter->addPrimitive(mesh, std::move(primitive), m_morphTargetMap);
    }
  }
}


void GltfConverter::addMeshInstance(
        std::shared_ptr<GltfNode>     node) {
  auto mesh = node->getMesh();
  auto primitives = mesh->getPrimitives();

  for (auto p = primitives.first; p != primitives.second; p++) {
    std::shared_ptr<GltfMeshPrimitive> primitive = *p;
    auto converter = getMeshConverter(mesh, primitive->getMaterial());

    if (converter != nullptr)
      converter->addInstance(std::move(node));
  }

  if (node->getSkin() != nullptr)
    addSkin(node->getSkin());
}


void GltfConverter::addSkin(
        std::shared_ptr<GltfSkin>     skin) {
  // Ignore joint indices for now, just accumulate the
  // nodes in the asset that are actually joints
  auto joints = skin->getJoints();

  for (auto j = joints.first; j != joints.second; j++)
    m_jointIndices.emplace(*j, 0u);
}


void GltfConverter::computeJointIndices() {
  // Find joints that either have no parent or whose parent is not
  // used as a joint, and traverse the hierarchy breadth first.
  // This gives us the correct joint order for the final geometry.
  std::queue<std::shared_ptr<GltfNode>> jointQueue;

  for (const auto& p : m_jointIndices) {
    if (m_jointIndices.find(p.first->getParent()) == m_jointIndices.end())
      jointQueue.push(p.first);
  }

  while (!jointQueue.empty()) {
    std::shared_ptr<GltfNode> joint = std::move(jointQueue.front());

    // Assign final joint index for the current joint
    uint32_t jointIndex = uint32_t(m_jointMetadata.size());

    auto entry = m_jointIndices.find(joint);
    dbg_assert(entry != m_jointIndices.end());
    entry->second = jointIndex;

    // Set joint metadata
    auto parent = m_jointIndices.find(joint->getParent());

    auto& metadata = m_jointMetadata.emplace_back();
    metadata.name = joint->getName();
    metadata.jointIndex = jointIndex;
    metadata.info.position = Vector3D(joint->computeTransform().getTranslation());
    metadata.info.parent = parent == m_jointIndices.end() ? ~0u : parent->second;

    // Add child nodes to the queue
    auto children = joint->getChildren();

    for (auto j = children.first; j != children.second; j++) {
      if (m_jointIndices.find(*j) != m_jointIndices.end())
        jointQueue.push(*j);
    }

    jointQueue.pop();
  }
}


void GltfConverter::writeBufferData(
        uint32_t                      buffer,
        uint32_t                      offset,
  const void*                         data,
        size_t                        size) {
  auto& storage = m_buffers.at(buffer);

  if (offset + size > storage.size()) {
    Log::err("Buffer write failed: buffer index = ", buffer,
      " (", storage.size(), "), offset = ", offset, ", size = ", size);
    return;
  }

  std::memcpy(&storage[offset], data, size);
}


uint32_t GltfConverter::allocateStorage(
        uint32_t&                     allocator,
        size_t                        amount) {
  uint32_t result = allocator;
  allocator += align(uint32_t(amount), 16u);
  return result;
}

}