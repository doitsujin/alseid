#include "gfx_scene_pass.h"

namespace as {

GfxScenePassGroupBuffer::GfxScenePassGroupBuffer(
        GfxDevice                     device)
: m_device(std::move(device)) {

}


GfxScenePassGroupBuffer::~GfxScenePassGroupBuffer() {

}


GfxDescriptor GfxScenePassGroupBuffer::getBvhDispatchDescriptor(
        uint32_t                      bvhLayer) const {
  if (!m_buffer)
    return GfxDescriptor();

  size_t listId = bvhLayer & 1u;

  size_t offset = m_header.bvhListOffset
    + offsetof(GfxSceneBvhListHeader, args)
    + sizeof(GfxSceneBvhListArgs) * listId
    + offsetof(GfxSceneBvhListArgs, dispatch);

  size_t size = sizeof(GfxDispatchArgs);

  return m_buffer->getDescriptor(GfxUsage::eParameterBuffer, offset, size);
}


void GfxScenePassGroupBuffer::setPasses(
        uint32_t                      passCount,
  const uint16_t*                     passIndices) {
  for (uint32_t i = 0; i < passCount; i++) {
    // Discard occlusion test results for any pass that we don't have data for.
    if (m_header.passes.at(i) != passIndices[i] || i >= m_header.passCount)
      m_header.ignoreOcclusionTestMask |= 1u << i;

    m_header.passes.at(i) = passIndices[i];
  }

  m_header.passCount = passCount;
}


void GfxScenePassGroupBuffer::updateBuffer(
  const GfxContext&                   context) {
  context->beginDebugLabel("Pass buffer update", GfxColorValue(0.6f, 0.6f, 0.6f, 1.0f));

  auto scratch = context->allocScratch(GfxUsage::eCpuWrite | GfxUsage::eTransferSrc, sizeof(m_header));
  std::memcpy(scratch.map(GfxUsage::eCpuWrite, 0), &m_header, sizeof(m_header));

  context->copyBuffer(m_buffer, 0,
    scratch.buffer, scratch.offset,
    scratch.size);

  if (std::exchange(m_doClear, false)) {
    context->clearBuffer(m_buffer, scratch.size,
      m_buffer->getDesc().size - scratch.size);
  }

  context->endDebugLabel();
}


GfxBuffer GfxScenePassGroupBuffer::resizeBuffer(
  const GfxScenePassGroupBufferDesc&  desc) {
  // Do nothing if the none of the capacities grow
  if (desc.maxBvhNodes <= m_desc.maxBvhNodes
   && desc.maxInstanceNodes <= m_desc.maxInstanceNodes
   && desc.maxLightNodes <= m_desc.maxLightNodes)
    return GfxBuffer();

  // Clear buffer on next update. While technically unnecessary since
  // shaders will initialize all the list headers etc. anyway, clearing
  // unused data to zero may simplify debugging.
  m_doClear = true;

  // Need to invalidate occlusion test results as well
  m_header.ignoreOcclusionTestMask = (2u << (m_header.passCount - 1u)) - 1u;

  // Align capacities in such a way that we're unlikely to need to
  // resize or restructure the buffer again very soon
  m_desc.maxBvhNodes      = align(desc.maxBvhNodes,       4096u);
  m_desc.maxInstanceNodes = align(desc.maxInstanceNodes, 65536u);
  m_desc.maxLightNodes    = align(desc.maxLightNodes,     4096u);

  // Compute minimum buffer size required to store everything
  uint32_t allocator = 0u;
  allocStorage(allocator, sizeof(m_header));

  m_header.bvhListOffset = allocStorage(allocator,
    sizeof(GfxSceneBvhListHeader) +
    sizeof(GfxSceneNodeListEntry) * m_desc.maxBvhNodes);

  m_header.bvhVisibilityOffset = allocStorage(allocator,
    sizeof(GfxSceneBvhVisibility) * m_desc.maxBvhNodes);

  m_header.instanceListOffset = allocStorage(allocator,
    sizeof(GfxSceneNodeListHeader) +
    sizeof(GfxSceneNodeListEntry) * m_desc.maxInstanceNodes);

  m_header.lightListOffset = allocStorage(allocator,
    sizeof(GfxSceneNodeListHeader) +
    sizeof(GfxSceneNodeListEntry) * m_desc.maxLightNodes);

  // If possible, just reuse the existing buffer. We don't need to do
  // anything, the header update and the required initialization pass
  // will set everything up.
  if (m_buffer && m_buffer->getDesc().size >= allocator)
    return GfxBuffer();

  // Otherwise, we actually need to create a new buffer
  std::string name = strcat("Pass group v", ++m_version);

  GfxBufferDesc bufferDesc = { };
  bufferDesc.debugName = name.c_str();
  bufferDesc.usage = GfxUsage::eTransferDst |
    GfxUsage::eParameterBuffer |
    GfxUsage::eShaderResource |
    GfxUsage::eShaderStorage;
  bufferDesc.size = align<uint64_t>(allocator, 1u << 20);
  bufferDesc.flags = GfxBufferFlag::eDedicatedAllocation;

  return std::exchange(m_buffer, m_device->createBuffer(bufferDesc, GfxMemoryType::eAny));
}


uint32_t GfxScenePassGroupBuffer::allocStorage(
        uint32_t&                     allocator,
        size_t                        size) {
  uint32_t offset = allocator;
  allocator += align(uint32_t(size), 256u);
  return offset;
}

}
