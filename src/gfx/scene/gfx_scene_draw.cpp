#include "gfx_scene_draw.h"

#include "../../util/util_ptr.h"

namespace as {

GfxSceneDrawBuffer::GfxSceneDrawBuffer(
        GfxDevice                     device)
: m_device(std::move(device)) {

}


GfxSceneDrawBuffer::~GfxSceneDrawBuffer() {

}


GfxDescriptor GfxSceneDrawBuffer::getDrawCountDescriptor(
        uint32_t                      drawGroup) const {
  if (m_buffer == nullptr || drawGroup >= m_header.drawGroupCount)
    return GfxDescriptor();

  size_t offset = sizeof(GfxSceneDrawListHeader) +
    sizeof(GfxSceneDrawListEntry) * m_entries[drawGroup].drawIndex +
    offsetof(GfxSceneDrawListEntry, drawCount);
  size_t size = sizeof(uint32_t);

  return m_buffer->getDescriptor(GfxUsage::eParameterBuffer, offset, size);
}


GfxDescriptor GfxSceneDrawBuffer::getDrawParameterDescriptor(
        uint32_t                      drawGroup) const {
  if (m_buffer == nullptr || drawGroup >= m_header.drawGroupCount)
    return GfxDescriptor();

  return m_buffer->getDescriptor(GfxUsage::eParameterBuffer,
    sizeof(GfxDispatchArgs) * m_entries[drawGroup].drawIndex + m_header.drawParameterOffset,
    sizeof(GfxDispatchArgs) * m_entries[drawGroup].drawCount);
}


void GfxSceneDrawBuffer::updateLayout(
  const GfxContext&                   context,
  const GfxSceneDrawBufferDesc&       desc) {
  uint32_t totalDrawCount = 0;

  // Compute individual draw group offsets
  m_entries.resize(desc.drawGroupCount);

  for (uint32_t i = 0; i < desc.drawGroupCount; i++) {
    m_entries[i].drawIndex = totalDrawCount;
    m_entries[i].drawCount = desc.drawGroups[i].drawCount;
    totalDrawCount += desc.drawGroups[i].drawCount;
  }

  // Compute buffer layout
  size_t drawGroupListSize = sizeof(GfxSceneDrawListHeader) +
    sizeof(GfxSceneDrawListEntry) * desc.drawGroupCount;

  uint64_t newSize = 0;
  uint64_t oldSize = 0;

  allocateStorage(newSize, drawGroupListSize);

  m_header = GfxSceneDrawListHeader{};
  m_header.drawGroupCount = desc.drawGroupCount;
  m_header.drawParameterOffset = allocateStorage(newSize,
    sizeof(GfxDispatchArgs) * totalDrawCount);
  m_header.drawInfoOffset = allocateStorage(newSize,
    sizeof(GfxSceneDrawInstanceInfo) * totalDrawCount);

  // Align to 4 MiB to avoid frequent reallocations
  newSize = align(newSize, uint64_t(4u << 20));

  if (m_buffer != nullptr)
    oldSize = m_buffer->getDesc().size;

  if (newSize > oldSize) {
    GfxBufferDesc bufferDesc;
    bufferDesc.debugName = "Draw parameters";
    bufferDesc.usage = GfxUsage::eTransferDst | GfxUsage::eParameterBuffer |
      GfxUsage::eShaderResource | GfxUsage::eShaderStorage;
    bufferDesc.size = newSize;
    bufferDesc.flags = GfxBufferFlag::eDedicatedAllocation;

    GfxBuffer oldBuffer = std::exchange(m_buffer,
      m_device->createBuffer(bufferDesc, GfxMemoryType::eAny));

    if (oldBuffer)
      context->trackObject(oldBuffer);
  }

  // Write new buffer contents to a scratch buffer
  GfxScratchBuffer scratch = context->allocScratch(
    GfxUsage::eCpuWrite | GfxUsage::eTransferSrc, drawGroupListSize);

  void* scratchPtr = scratch.map(GfxUsage::eCpuWrite, 0);

  auto dstHeader = reinterpret_cast<GfxSceneDrawListHeader*>(scratchPtr);
  auto dstEntries = reinterpret_cast<GfxSceneDrawListEntry*>(ptroffset(scratchPtr, sizeof(*dstHeader)));

  *dstHeader = m_header;

  for (uint32_t i = 0; i < m_header.drawGroupCount; i++) {
    dstEntries[i].drawIndex = m_entries[i].drawIndex;
    dstEntries[i].drawCount = 0u;
  }

  // Copy scratch buffer to the draw buffer, and clear the rest to zero
  // if the buffer has been newly allocated. Don't bother otherwise since
  // the layout is expected to be volatile anyway.
  context->beginDebugLabel("Initialize draw buffer", 0xff96c096u);
  context->copyBuffer(m_buffer, 0, scratch.buffer, scratch.offset, scratch.size);

  if (newSize > oldSize)
    context->clearBuffer(m_buffer, drawGroupListSize, newSize - drawGroupListSize);

  context->endDebugLabel();
}


void GfxSceneDrawBuffer::generateDraws(
  const GfxContext&                   context,
  const GfxScenePipelines&            pipelines,
        uint64_t                      passInfoVa,
  const GfxSceneNodeManager&          nodeManager,
  const GfxSceneInstanceManager&      instanceManager,
  const GfxScenePassGroupBuffer&      groupBuffer,
        uint32_t                      frameId,
        uint32_t                      passMask,
        uint32_t                      lodSelectionPass) {
  context->beginDebugLabel("Generate draws", 0xff7878ff);
  context->beginDebugLabel("Reset counters", 0xffb4b0ff);

  GfxSceneDrawListInitArgs resetArgs = { };
  resetArgs.drawListVa = m_buffer->getGpuAddress();
  resetArgs.drawGroupCount = m_header.drawGroupCount;

  pipelines.initDrawList(context, resetArgs);

  context->memoryBarrier(
    GfxUsage::eShaderStorage, GfxShaderStage::eCompute,
    GfxUsage::eShaderStorage, GfxShaderStage::eCompute);

  context->endDebugLabel();

  context->beginDebugLabel("Emit draws", 0xffb4b0ff);

  GfxDescriptor dispatch = groupBuffer.getDispatchDescriptors(GfxSceneNodeType::eInstance).processAll;

  GfxSceneDrawListGenerateArgs generateArgs = { };
  generateArgs.drawListVa = m_buffer->getGpuAddress();
  generateArgs.instanceBufferVa = instanceManager.getGpuAddress();
  generateArgs.sceneBufferVa = nodeManager.getGpuAddress();
  generateArgs.passInfoVa = passInfoVa;
  generateArgs.passGroupVa = groupBuffer.getGpuAddress();
  generateArgs.frameId = frameId;
  generateArgs.passMask = passMask;
  generateArgs.lodSelectionPass = lodSelectionPass;

  pipelines.generateDrawList(context, dispatch, generateArgs);

  context->memoryBarrier(
    GfxUsage::eShaderStorage, GfxShaderStage::eCompute,
    GfxUsage::eShaderStorage | GfxUsage::eShaderResource | GfxUsage::eParameterBuffer,
    GfxShaderStage::eCompute | GfxShaderStage::eMeshTask);

  context->endDebugLabel();
  context->endDebugLabel();
}


uint32_t GfxSceneDrawBuffer::allocateStorage(
        uint64_t&                     allocator,
        size_t                        size) {
  if (!size)
    return 0u;

  uint32_t result = uint32_t(allocator);
  allocator += align(uint32_t(size), 16u);
  return result;
}

};
