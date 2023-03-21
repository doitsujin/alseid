#include "gfx.h"
#include "gfx_geometry.h"

namespace as {

bool GfxGeometryBufferDesc::serialize(
        WrBufferedStream&             output) {
  WrStream stream(output);

  uint16_t version = 1;
  bool success = stream.write(version);

  success &= stream.write(flags)
          && stream.write(materialCount)
          && stream.write(bufferSize)
          && stream.write(aabb)
          && stream.write(meshletCount)
          && stream.write(meshletOffset)
          && stream.write(uint16_t(streams.size()))
          && stream.write(uint16_t(meshes.size()))
          && stream.write(uint16_t(lods.size()));

  for (size_t i = 0; i < streams.size(); i++) {
    const auto& streamDesc = streams[i];

    success &= stream.write(streamDesc.indexFormat)
            && stream.write(streamDesc.indexDataOffset)
            && stream.write(streamDesc.indexDataSize)
            && stream.write(streamDesc.primitiveDataOffset)
            && stream.write(streamDesc.primitiveDataSize)
            && stream.write(streamDesc.positionFormat)
            && stream.write(streamDesc.vertexPositionOffset)
            && stream.write(streamDesc.vertexDataOffset)
            && stream.write(streamDesc.vertexDataStride)
            && stream.write(streamDesc.vertexCount);
  }

  for (size_t i = 0; i < meshes.size(); i++) {
    const auto& meshDesc = meshes[i];

    success &= stream.write(meshDesc.materialIndex)
            && stream.write(meshDesc.streamIndex)
            && stream.write(meshDesc.lodIndex)
            && stream.write(meshDesc.lodCount)
            && stream.write(meshDesc.maxMeshletCount);
  }

  for (size_t i = 0; i < lods.size(); i++) {
    const auto& lodDesc = lods[i];

    success &= stream.write(lodDesc.maxDistance)
            && stream.write(lodDesc.indexCount)
            && stream.write(lodDesc.firstIndex)
            && stream.write(lodDesc.firstVertex)
            && stream.write(lodDesc.meshletIndex)
            && stream.write(lodDesc.meshletCount);
  }

  return success;
}


std::optional<GfxGeometryBufferDesc> GfxGeometryBufferDesc::deserialize(
        RdMemoryView                  input) {
  RdStream stream(input);
  uint16_t version = 0;

  if (!stream.read(version) || version != 1)
    return std::nullopt;

  std::optional<GfxGeometryBufferDesc> result;
  auto& desc = result.emplace();

  uint16_t streamCount = 0;
  uint16_t meshCount = 0;
  uint16_t lodCount = 0;

  if (!stream.read(desc.flags)
   || !stream.read(desc.materialCount)
   || !stream.read(desc.bufferSize)
   || !stream.read(desc.aabb)
   || !stream.read(desc.meshletCount)
   || !stream.read(desc.meshletOffset)
   || !stream.read(streamCount)
   || !stream.read(meshCount)
   || !stream.read(lodCount))
    return std::nullopt;

  desc.streams.resize(streamCount);

  for (size_t i = 0; i < streamCount; i++) {
    auto& streamDesc = desc.streams[i];

    if (!stream.read(streamDesc.indexFormat)
     || !stream.read(streamDesc.indexDataOffset)
     || !stream.read(streamDesc.indexDataSize)
     || !stream.read(streamDesc.primitiveDataOffset)
     || !stream.read(streamDesc.primitiveDataSize)
     || !stream.read(streamDesc.positionFormat)
     || !stream.read(streamDesc.vertexPositionOffset)
     || !stream.read(streamDesc.vertexDataOffset)
     || !stream.read(streamDesc.vertexDataStride)
     || !stream.read(streamDesc.vertexCount))
      return std::nullopt;
  }

  desc.meshes.resize(meshCount);

  for (size_t i = 0; i < meshCount; i++) {
    auto& meshDesc = desc.meshes[i];

    if (!stream.read(meshDesc.materialIndex)
     || !stream.read(meshDesc.streamIndex)
     || !stream.read(meshDesc.lodIndex)
     || !stream.read(meshDesc.lodCount)
     || !stream.read(meshDesc.maxMeshletCount))
      return std::nullopt;

    if (meshDesc.materialIndex >= desc.materialCount
     || meshDesc.streamIndex >= streamCount
     || meshDesc.lodIndex + meshDesc.lodCount > lodCount)
      return std::nullopt;
  }

  desc.lods.resize(lodCount);

  for (size_t i = 0; i < lodCount; i++) {
    auto& lodDesc = desc.lods[i];

    if (!stream.read(lodDesc.maxDistance)
     || !stream.read(lodDesc.indexCount)
     || !stream.read(lodDesc.firstIndex)
     || !stream.read(lodDesc.firstVertex)
     || !stream.read(lodDesc.meshletIndex)
     || !stream.read(lodDesc.meshletCount))
      return std::nullopt;

    if (lodDesc.meshletIndex + lodDesc.meshletCount > desc.meshletCount)
      return std::nullopt;
  }

  for (size_t i = 0; i < meshCount; i++) {
    const auto& meshDesc = desc.meshes[i];

    for (size_t j = meshDesc.lodIndex; j < meshDesc.lodIndex + meshDesc.lodCount; j++) {
      const auto& lodDesc = desc.lods[j];

      if (lodDesc.meshletCount > meshDesc.maxMeshletCount)
        return std::nullopt;
    }
  }

  return result;
}




GfxGeometryBufferIface::GfxGeometryBufferIface(
        GfxGeometryBufferDesc         desc,
        GfxBuffer                     buffer,
        uint64_t                      offset)
: m_desc    (desc)
, m_buffer  (buffer)
, m_offset  (offset) {

}


GfxGeometryBufferIface::~GfxGeometryBufferIface() {

}

}
