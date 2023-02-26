#include <cstring>

#include "../util/util_math.h"
#include "../util/util_string.h"

#include "gfx_shader.h"
#include "gfx_utils.h"

namespace as {

bool GfxShaderDesc::serialize(
        WrBufferedStream&             output) const {
  WrStream stream(output);
  bool success = true;

  // The shader stage enum is used for bit mask but only
  // has one bit set, so just write out the set bit index
  uint32_t stageNum = stage != GfxShaderStage::eFlagEnum
    ? tzcnt(uint32_t(stage)) : 0xFFFFu;

  success &= stream.write(uint16_t(stageNum))
          && stream.write(uint16_t(constantSize))
          && stream.write(uint32_t(flags));

  // Only write out workgroup size if the stage needs it
  if (gfxShaderStageHasWorkgroupSize(stage)) {
    success &= stream.write(uint16_t(workgroupSize.at<0>()))
            && stream.write(uint16_t(workgroupSize.at<1>()))
            && stream.write(uint16_t(workgroupSize.at<2>()));
  }

  // Write out binding info. Names are not null-terminated.
  success &= stream.write(uint16_t(bindings.size()));

  for (const auto& b : bindings) {
    success &= stream.write(uint8_t(b.type))
            && stream.write(uint8_t(b.descriptorSet))
            && stream.write(uint16_t(b.descriptorIndex))
            && stream.write(uint16_t(b.descriptorCount))
            && stream.write(uint16_t(b.name.size()))
            && stream.write(b.name.c_str(), b.name.size());
  }

  return success;
}


bool GfxShaderDesc::deserialize(
        RdMemoryView                  input) {
  RdStream stream(input);

  // Read and decode basic binding info
  uint32_t stageNum = 0xFFFFu;

  if (!stream.readAs<uint16_t>(stageNum)
   || !stream.readAs<uint16_t>(constantSize)
   || !stream.readAs<uint32_t>(flags))
    return false;

  stage = stageNum < 32
    ? GfxShaderStage(1u << stageNum)
    : GfxShaderStage::eFlagEnum;

  // Decode workgroup size if necessary for the given stage
  if (gfxShaderStageHasWorkgroupSize(stage)) {
    uint32_t x, y, z;

    if (!stream.readAs<uint16_t>(x)
     || !stream.readAs<uint16_t>(y)
     || !stream.readAs<uint16_t>(z))
      return false;

    workgroupSize = Extent3D(x, y, z);
  }

  // Decode binding infos
  uint32_t bindingCount = 0;

  if (!stream.readAs<uint16_t>(bindingCount))
    return false;

  bindings.resize(bindingCount);
  std::vector<char> nameBuffer;

  for (auto& b : bindings) {
    uint32_t nameLength = 0;

    if (!stream.readAs<uint8_t>(b.type)
     || !stream.readAs<uint8_t>(b.descriptorSet)
     || !stream.readAs<uint16_t>(b.descriptorIndex)
     || !stream.readAs<uint16_t>(b.descriptorCount)
     || !stream.readAs<uint16_t>(nameLength))
      return false;

    nameBuffer.resize(nameLength + 1);

    if (!stream.read(nameBuffer.data(), nameLength))
      return false;

    b.name = std::string(nameBuffer.data());
  }

  return true;
}


GfxShaderIface::GfxShaderIface(
        GfxShaderDesc&&               desc,
        GfxShaderBinaryDesc&&         binary)
: m_desc    (std::move(desc))
, m_binary  (std::move(binary))
, m_hash    (UniqueHash::compute(m_binary.data.size(), m_binary.data.data())) {
  m_debugName = m_desc.debugName
    ? strcat(m_desc.debugName)
    : m_hash.toString();

  m_desc.debugName = m_debugName.c_str();
}


GfxShaderIface::~GfxShaderIface() {

}


std::optional<GfxShaderBinding> GfxShaderIface::findBinding(
  const char*                         name) const {
  for (auto& binding : m_desc.bindings) {
    if (binding.name == name)
      return binding;
  }

  return std::nullopt;
}




GfxShader::GfxShader(
        GfxShaderDesc&&               desc,
        GfxShaderBinaryDesc&&         binary)
: IfaceRef<GfxShaderIface>(std::make_shared<GfxShaderIface>(
    std::move(desc), std::move(binary))) {
  
}

}
