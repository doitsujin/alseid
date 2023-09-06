#include "gfx_scene_node.h"

namespace as {

GfxSceneBuffer::GfxSceneBuffer(
        GfxDevice                     device)
: m_device(std::move(device)) {

}


GfxSceneBuffer::~GfxSceneBuffer() {

}


GfxBuffer GfxSceneBuffer::resizeBuffer(
  const GfxContext&                   context,
  const GfxSceneBufferDesc&           desc) {
  // Don't do anything if the buffer layout does not change
  GfxSceneBufferDesc oldDesc = m_desc;

  if (desc.nodeCount <= oldDesc.nodeCount
   && desc.bvhCount <= oldDesc.bvhCount)
    return GfxBuffer();

  // Align all capacities to large enough numbers to reduce reallocations.
  m_desc.nodeCount = std::max(m_desc.nodeCount, align(desc.nodeCount, 1u << 16));
  m_desc.bvhCount  = std::max(m_desc.bvhCount,  align(desc.bvhCount,  1u << 12));

  // Compute the actual buffer layout.
  uint32_t allocator = 0;
  allocStorage(allocator, sizeof(GfxSceneHeader));

  GfxSceneHeader newHeader = { };
  newHeader.nodeParameterOffset = allocStorage(allocator,
    sizeof(GfxSceneNodeInfo) * m_desc.nodeCount);

  newHeader.nodeTransformOffset = allocStorage(allocator,
    sizeof(GfxSceneNodeTransform) * m_desc.nodeCount);

  newHeader.nodeResidencyOffset = allocStorage(allocator,
    sizeof(GfxSceneNodeResidencyFlags) * m_desc.nodeCount);

  newHeader.bvhOffset = allocStorage(allocator,
    sizeof(GfxSceneBvhInfo) * m_desc.bvhCount);

  GfxSceneHeader oldHeader = m_header;

  // Create a new buffer that's large enough to hold all data
  std::string name = strcat("Scene buffer v", ++m_version);

  GfxBufferDesc bufferDesc = { };
  bufferDesc.debugName = name.c_str();
  bufferDesc.usage = GfxUsage::eTransferDst |
    GfxUsage::eTransferSrc |
    GfxUsage::eShaderResource |
    GfxUsage::eShaderStorage;
  bufferDesc.size = allocator;
  bufferDesc.flags = GfxBufferFlag::eDedicatedAllocation;

  GfxBuffer newBuffer = m_device->createBuffer(bufferDesc, GfxMemoryType::eAny);
  GfxBuffer oldBuffer = std::move(m_buffer);

  // Zero-initialize entire buffer. This is more robust and easier
  // to reason about than just clearing the parts that require it.
  context->beginDebugLabel("Scene buffer copy", GfxColorValue(0.8f, 0.8f, 0.8f, 1.0f));
  context->clearBuffer(newBuffer, 0, allocator);
  context->memoryBarrier(GfxUsage::eTransferDst, 0, GfxUsage::eTransferDst, 0);

  // Write new buffer header to the buffer
  auto scratch = context->writeScratch(GfxUsage::eTransferSrc, newHeader);
  context->copyBuffer(newBuffer, 0, scratch.buffer, scratch.offset, scratch.size);

  // Copy data from the old buffer to the new one
  if (oldDesc.nodeCount) {
    context->copyBuffer(
      newBuffer, newHeader.nodeParameterOffset,
      oldBuffer, oldHeader.nodeParameterOffset,
      sizeof(GfxSceneNodeInfo) * oldDesc.nodeCount);

    context->copyBuffer(
      newBuffer, newHeader.nodeTransformOffset,
      oldBuffer, oldHeader.nodeTransformOffset,
      sizeof(GfxSceneNodeTransform) * oldDesc.nodeCount);

    context->copyBuffer(
      newBuffer, newHeader.nodeResidencyOffset,
      oldBuffer, oldHeader.nodeResidencyOffset,
      sizeof(GfxSceneNodeResidencyFlags) * oldDesc.nodeCount);
  }

  if (oldDesc.bvhCount) {
    context->copyBuffer(
      newBuffer, newHeader.bvhOffset,
      oldBuffer, oldHeader.bvhOffset,
      sizeof(GfxSceneBvhInfo) * oldDesc.bvhCount);
  }

  context->endDebugLabel();

  // Write back new buffer layout properties
  m_buffer = newBuffer;
  m_header = newHeader;
  return oldBuffer;
}


uint32_t GfxSceneBuffer::allocStorage(
        uint32_t&                     allocator,
        size_t                        size) {
  uint32_t offset = allocator;
  allocator += align(uint32_t(size), 256u);
  return offset;
}

}
