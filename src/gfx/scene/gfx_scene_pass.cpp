#include "gfx_scene_pass.h"

namespace as {

GfxScenePassGroupBuffer::GfxScenePassGroupBuffer(
        GfxDevice                     device)
: m_device(std::move(device)) {

}


GfxScenePassGroupBuffer::~GfxScenePassGroupBuffer() {

}


GfxDescriptor GfxScenePassGroupBuffer::getBvhDispatchDescriptor(
        uint32_t                      bvhLayer,
        bool                          traverse) const {
  if (!m_buffer)
    return GfxDescriptor();

  size_t listId = bvhLayer & 1u;

  size_t offset = m_header.bvhListOffset
    + offsetof(GfxSceneBvhListHeader, args)
    + sizeof(GfxSceneBvhListArgs) * listId
    + (traverse
      ? offsetof(GfxSceneBvhListArgs, dispatchTraverse)
      : offsetof(GfxSceneBvhListArgs, dispatchReset));

  size_t size = sizeof(GfxDispatchArgs);

  return m_buffer->getDescriptor(GfxUsage::eParameterBuffer, offset, size);
}


std::pair<GfxDescriptor, GfxDescriptor> GfxScenePassGroupBuffer::getDispatchDescriptors(
        GfxSceneNodeType              nodeType) const {
  if (!m_buffer)
    return std::make_pair(GfxDescriptor(), GfxDescriptor());

  GfxScenePassTypedNodeListOffsets offsets = m_header.listOffsets.at(
    size_t(nodeType) - size_t(GfxSceneNodeType::eBuiltInCount));

  size_t size = sizeof(GfxDispatchArgs);

  return std::make_pair(
    m_buffer->getDescriptor(GfxUsage::eParameterBuffer, offsets.nodeList, size),
    m_buffer->getDescriptor(GfxUsage::eParameterBuffer, offsets.updateList, size));
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


void GfxScenePassGroupBuffer::commitUpdates(
  const GfxContext&                   context,
        uint32_t                      currFrameId,
        uint32_t                      lastFrameId) {
  cleanupGpuBuffers(lastFrameId);

  updateGpuBuffer(context);
}


void GfxScenePassGroupBuffer::resizeBuffer(
  const GfxScenePassGroupBufferDesc&  desc,
        uint32_t                      currFrameId) {
  // Do nothing if the none of the capacities grow
  bool hasGrownCapacity = false;

  for (uint32_t i = 1; i < uint32_t(GfxSceneNodeType::eCount) && !hasGrownCapacity; i++)
    hasGrownCapacity = desc.maxNodeCounts.at(i) > m_desc.maxNodeCounts.at(i);

  if (!hasGrownCapacity)
    return;

  // Clear buffer on next update. While technically unnecessary since
  // shaders will initialize all the list headers etc. anyway, clearing
  // unused data to zero may simplify debugging.
  m_doClear = true;

  // Need to invalidate occlusion test results as well
  m_header.ignoreOcclusionTestMask = (2u << (m_header.passCount - 1u)) - 1u;

  // Align capacities in such a way that we're unlikely to need to
  // resize or restructure the buffer again very soon
  for (uint32_t i = 1; i < uint32_t(GfxSceneNodeType::eCount); i++)
    m_desc.maxNodeCounts.at(i) = align(desc.maxNodeCounts.at(i), 4096u);

  // Compute minimum buffer size required to store everything
  uint32_t maxBvhNodes = m_desc.maxNodeCounts[size_t(GfxSceneNodeType::eBvh)];

  uint32_t allocator = 0u;
  allocStorage(allocator, sizeof(m_header));

  m_header.bvhListOffset = allocStorage(allocator,
    sizeof(GfxSceneBvhListHeader) +
    sizeof(GfxSceneNodeListEntry) * maxBvhNodes);

  m_header.bvhVisibilityOffset = allocStorage(allocator,
    sizeof(GfxSceneBvhVisibility) * maxBvhNodes);

  // Keep the offset for any unused node type at zero. This also allows
  // us to ignore certain node types for certain pass groups entirely,
  // e.g. light nodes during shadow passes.
  for (uint32_t i = uint32_t(GfxSceneNodeType::eBuiltInCount); i < uint32_t(GfxSceneNodeType::eCount); i++) {
    uint32_t count = m_desc.maxNodeCounts.at(i);
    GfxScenePassTypedNodeListOffsets offsets = { };

    if (count) {
      offsets.nodeList = allocStorage(allocator,
        sizeof(GfxSceneNodeListHeader) +
        sizeof(GfxSceneNodeListEntry) * count);
      offsets.updateList = allocStorage(allocator,
        sizeof(GfxSceneNodeListHeader) +
        sizeof(uint32_t) * count);
    }

    m_header.listOffsets.at(i - uint32_t(GfxSceneNodeType::eBuiltInCount)) = offsets;
  }

  // If possible, just reuse the existing buffer. We don't need to do
  // anything, the header update and the required initialization pass
  // will set everything up.
  if (m_buffer && m_buffer->getDesc().size >= allocator)
    return;

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

  if (m_buffer)
    m_gpuBuffers.insert({ currFrameId, std::move(m_buffer) });

  m_buffer = m_device->createBuffer(bufferDesc, GfxMemoryType::eAny);
}


void GfxScenePassGroupBuffer::cleanupGpuBuffers(
        uint32_t                      lastFrameId) {
  m_gpuBuffers.erase(lastFrameId);
}


void GfxScenePassGroupBuffer::updateGpuBuffer(
  const GfxContext&                   context) {
  context->beginDebugLabel("Update pass group buffer", 0xff96c096u);

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


uint32_t GfxScenePassGroupBuffer::allocStorage(
        uint32_t&                     allocator,
        size_t                        size) {
  uint32_t offset = allocator;
  allocator += align(uint32_t(size), 256u);
  return offset;
}

}
