#include "gfx_scene_draw.h"

#include "../../util/util_ptr.h"

namespace as {

GfxSceneDrawBuffer::GfxSceneDrawBuffer(
        GfxDevice                     device)
: m_device(std::move(device)) {

}


GfxSceneDrawBuffer::~GfxSceneDrawBuffer() {

}


uint32_t GfxSceneDrawBuffer::getDrawCount(
        uint32_t                      drawGroup) const {
  if (drawGroup >= m_header.drawGroupCount)
    return 0u;

  return m_entries[drawGroup].dispatchCount;
}


GfxDescriptor GfxSceneDrawBuffer::getDrawParameterDescriptor(
        uint32_t                      drawGroup) const {
  if (m_buffer == nullptr || drawGroup >= m_header.drawGroupCount)
    return GfxDescriptor();

  return m_buffer->getDescriptor(GfxUsage::eParameterBuffer,
    sizeof(GfxDispatchArgs) * m_entries[drawGroup].dispatchIndex + m_header.drawParameterOffset,
    sizeof(GfxDispatchArgs) * m_entries[drawGroup].dispatchCount);
}


void GfxSceneDrawBuffer::updateLayout(
  const GfxContext&                   context,
  const GfxSceneDrawBufferDesc&       desc) {
  uint32_t totalDrawCount = 0u;
  uint32_t totalDispatchCount = 0u;

  // Compute individual draw group offsets
  m_entries.resize(desc.drawGroupCount);

  for (uint32_t i = 0; i < desc.drawGroupCount; i++) {
    m_entries[i].drawIndex = totalDrawCount;
    m_entries[i].drawCount = desc.drawGroups[i].drawCount;
    totalDrawCount += desc.drawGroups[i].drawCount;

    uint32_t meshletCountPerDispatch = GfxSceneDrawMaxTsWorkgroupsPerDispatch *
      desc.drawGroups[i].meshletCountPerWorkgroup;

    m_entries[i].dispatchIndex = totalDispatchCount;
    m_entries[i].dispatchCount = align(desc.drawGroups[i].meshletCount,
      meshletCountPerDispatch) / meshletCountPerDispatch;
    m_entries[i].searchTreeDepth = 0u;
    totalDispatchCount += m_entries[i].dispatchCount;
  }

  // Compute buffer layout
  size_t drawGroupListSize = sizeof(GfxSceneDrawListHeader) +
    sizeof(GfxSceneDrawListEntry) * desc.drawGroupCount;

  uint64_t newSize = 0;
  allocateStorage(newSize, drawGroupListSize);

  m_header = GfxSceneDrawListHeader { };
  m_header.drawGroupCount = desc.drawGroupCount;
  m_header.drawParameterOffset = allocateStorage(newSize,
    sizeof(GfxDispatchArgs) * totalDispatchCount);
  m_header.drawInfoOffset = allocateStorage(newSize,
    sizeof(GfxSceneDrawInstanceInfo) * totalDrawCount);

  // Allocate storage for each draw group's search tree
  uint32_t workgroupCounterCount = 0u;

  for (uint32_t i = 0; i < desc.drawGroupCount; i++) {
    uint32_t layerWidth = desc.drawGroups[i].meshletCountPerWorkgroup;
    uint32_t layerSize = align(m_entries[i].drawCount, layerWidth) / layerWidth;

    m_entries[i].searchTreeDepth = 2u;
    m_entries[i].searchTreeCounterIndex = workgroupCounterCount++;
    m_entries[i].searchTreeLayerOffsets[0] = allocateStorage(
      newSize, sizeof(uint32_t) * m_entries[i].drawCount);
    m_entries[i].searchTreeLayerOffsets[1] = allocateStorage(
      newSize, sizeof(uint32_t) * layerSize);

    for (uint32_t j = m_entries[i].searchTreeDepth; j < GfxSceneDrawSearchTreeDepth; j++) {
      m_entries[i].searchTreeLayerOffsets[j] = 0u;

      if (layerSize > layerWidth) {
        layerSize = align(layerSize, layerWidth) / layerWidth;

        m_entries[i].searchTreeDepth += 1u;
        m_entries[i].searchTreeLayerOffsets[j] = allocateStorage(
          newSize, sizeof(uint32_t) * layerSize);

        workgroupCounterCount += layerSize;
      }
    }
  }

  recreateDrawBuffer(context, newSize);
  recreateCounterBuffer(context, workgroupCounterCount);

  // Write new buffer contents to a scratch buffer
  GfxScratchBuffer scratch = context->allocScratch(
    GfxUsage::eCpuWrite | GfxUsage::eTransferSrc, drawGroupListSize);

  void* scratchPtr = scratch.map(GfxUsage::eCpuWrite, 0);

  auto dstHeader = reinterpret_cast<GfxSceneDrawListHeader*>(scratchPtr);
  auto dstEntries = reinterpret_cast<GfxSceneDrawListEntry*>(ptroffset(scratchPtr, sizeof(*dstHeader)));

  *dstHeader = m_header;

  for (uint32_t i = 0; i < m_header.drawGroupCount; i++) {
    dstEntries[i] = m_entries[i];
    dstEntries[i].drawCount = 0u;
  }

  // Copy scratch buffer to the draw buffer, and clear the rest to zero
  // if the buffer has been newly allocated. Don't bother otherwise since
  // the layout is expected to be volatile anyway.
  context->beginDebugLabel("Initialize draw buffer", 0xff96c096u);
  context->copyBuffer(m_buffer, 0, scratch.buffer, scratch.offset, scratch.size);
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
  context->beginDebugLabel("Generate draw list", 0xff7878ff);
  context->beginDebugLabel("Reset counters", 0xffb4b0ff);

  GfxSceneDrawListInitArgs resetArgs = { };
  resetArgs.drawListVa = m_buffer->getGpuAddress();
  resetArgs.drawGroupCount = m_header.drawGroupCount;

  pipelines.initDrawList(context, resetArgs);

  context->memoryBarrier(
    GfxUsage::eShaderStorage, GfxShaderStage::eCompute,
    GfxUsage::eShaderStorage, GfxShaderStage::eCompute);

  context->endDebugLabel();

  // Scan instances that are visible in the given passes
  context->beginDebugLabel("Emit draw infos", 0xffb4b0ff);

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
    GfxShaderStage::eCompute);

  context->endDebugLabel();

  // For each draw group, build a search tree for the task shader
  // and emit indirect draw parameters.
  context->beginDebugLabel("Emit draw parameters", 0xffb4b0ff);

  for (uint32_t j = 0; j < m_header.drawGroupCount; j++) {
    uint32_t dispatchOffset = sizeof(GfxSceneDrawListHeader) +
      sizeof(GfxSceneDrawListEntry) * j +
      offsetof(GfxSceneDrawListEntry, searchTreeDispatch);

    GfxSceneDrawListBuildSearchTreeArgs args = { };
    args.counterVa = m_counters->getGpuAddress();
    args.drawListVa = getGpuAddress();
    args.drawGroup = j;

    GfxDescriptor dispatch = m_buffer->getDescriptor(GfxUsage::eParameterBuffer,
      dispatchOffset, sizeof(GfxDispatchArgs));

    pipelines.generateDrawParameters(context, dispatch, args);
  }

  context->memoryBarrier(
    GfxUsage::eShaderStorage | GfxUsage::eShaderResource | GfxUsage::eParameterBuffer, GfxShaderStage::eCompute,
    GfxUsage::eShaderStorage | GfxUsage::eShaderResource | GfxUsage::eParameterBuffer, GfxShaderStage::eMeshTask);

  context->endDebugLabel();
  context->endDebugLabel();
}


void GfxSceneDrawBuffer::recreateDrawBuffer(
  const GfxContext&                   context,
        uint64_t                      size) {
  uint64_t newSize = align(size, uint64_t(4u << 20));
  uint64_t oldSize = 0u;

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
}


void GfxSceneDrawBuffer::recreateCounterBuffer(
  const GfxContext&                   context,
        uint32_t                      counters) {
  uint64_t newSize = sizeof(uint32_t) * align(counters, 1u << 18u);
  uint64_t oldSize = 0u;

  if (m_counters != nullptr)
    oldSize = m_counters->getDesc().size;

  if (newSize > oldSize) {
    GfxBufferDesc bufferDesc;
    bufferDesc.debugName = "Draw list counters";
    bufferDesc.usage = GfxUsage::eTransferDst | GfxUsage::eShaderStorage;
    bufferDesc.size = newSize;

    GfxBuffer oldBuffer = std::exchange(m_counters,
      m_device->createBuffer(bufferDesc, GfxMemoryType::eAny));

    if (oldBuffer)
      context->trackObject(oldBuffer);

    // Zero-initialize counters right away
    context->beginDebugLabel("Initialize draw list counters", 0xff96c096u);
    context->clearBuffer(m_counters, 0, newSize);
    context->endDebugLabel();
  }
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
