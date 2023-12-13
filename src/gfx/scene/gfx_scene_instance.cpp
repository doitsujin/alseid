#include "gfx_scene_instance.h"
#include "gfx_scene_pass.h"

namespace as {

GfxSceneInstanceDataBuffer::GfxSceneInstanceDataBuffer(
  const GfxSceneInstanceDesc&         desc) {
  std::vector<uint32_t> shadingParameterOffset(desc.drawCount);

  // Compute size and layout of the instance data buffer
  GfxSceneInstanceDataHeader header = { };

  uint32_t dataAllocator = 0u;
  allocateStorage(dataAllocator, sizeof(header));

  header.instanceParameterOffset = allocateStorage(
    dataAllocator, desc.parameterDataSize);
  header.instanceParameterSize = desc.parameterDataSize;
  header.drawCount = desc.drawCount;
  header.drawOffset = allocateStorage(
    dataAllocator, sizeof(GfxSceneInstanceDraw) * desc.drawCount);
  header.jointCount = desc.jointCount;
  header.jointRelativeOffset = allocateStorage(
    dataAllocator, sizeof(QuatTransform) * desc.jointCount);
  header.jointAbsoluteOffset = allocateStorage(
    dataAllocator, sizeof(QuatTransform) * desc.jointCount * 2u);
  header.weightCount = desc.weightCount;
  header.weightOffset = allocateStorage(
    dataAllocator, sizeof(int16_t) * desc.weightCount * 3u);

  for (uint32_t i = 0; i < desc.drawCount; i++) {
    shadingParameterOffset[i] = allocateStorage(
      dataAllocator, desc.draws[i].shadingParameterSize);
  }

  // Initialize actual host data
  m_buffer = AlignedBuffer(dataAllocator, 16);
  std::memset(m_buffer.getData(), 0, m_buffer.getSize());
  std::memcpy(m_buffer.getData(), &header, sizeof(header));

  QuatTransform identityTransform = QuatTransform::identity();

  auto relativeJoints = m_buffer.template getAs<QuatTransform>(header.jointRelativeOffset);
  auto absoluteJoints = m_buffer.template getAs<QuatTransform>(header.jointAbsoluteOffset);

  for (uint32_t i = 0; i < desc.jointCount; i++) {
    relativeJoints[i] = identityTransform;

    absoluteJoints[2 * i + 0] = identityTransform;
    absoluteJoints[2 * i + 1] = identityTransform;
  }

  auto draws = m_buffer.template getAs<GfxSceneInstanceDraw>(header.drawOffset);

  for (uint32_t i = 0; i < desc.drawCount; i++) {
    draws[i] = desc.draws[i];
    draws[i].shadingParameterOffset = shadingParameterOffset[i];
  }
}


GfxSceneInstanceDataBuffer::~GfxSceneInstanceDataBuffer() {

}


uint32_t GfxSceneInstanceDataBuffer::allocateStorage(
        uint32_t&                     allocator,
        uint32_t                      size) {
  if (!size)
    return 0u;

  uint32_t offset = allocator;
  allocator += align(size, 16u);
  return offset;
}




GfxSceneInstanceBuffer::GfxSceneInstanceBuffer(
        GfxDevice                     device)
: m_device(std::move(device)) {
  GfxBufferDesc dataBufferDesc;
  dataBufferDesc.debugName = "Instance data";
  dataBufferDesc.usage = GfxUsage::eTransferSrc | GfxUsage::eTransferDst |
    GfxUsage::eShaderResource | GfxUsage::eShaderStorage;
  dataBufferDesc.size = 4ull << 20;
  dataBufferDesc.flags = GfxBufferFlag::eDedicatedAllocation;

  m_dataBuffer = std::make_unique<GfxBufferPool>(
    m_device, dataBufferDesc, GfxMemoryType::eAny);
}


GfxSceneInstanceBuffer::~GfxSceneInstanceBuffer() {
  
}


GfxBuffer GfxSceneInstanceBuffer::resizeBuffer(
  const GfxContext&                   context,
        uint32_t                      instanceCount) {
  // Only recreate the buffer if necessary
  uint64_t newSize = sizeof(GfxSceneInstanceNodeInfo) * align(instanceCount, 1u << 16);
  uint64_t oldSize = 0;

  if (m_nodeBuffer)
    oldSize = m_nodeBuffer->getDesc().size;

  if (newSize <= oldSize)
    return GfxBuffer();

  GfxBufferDesc bufferDesc;
  bufferDesc.debugName = "Instance nodes";
  bufferDesc.usage = GfxUsage::eTransferSrc | GfxUsage::eTransferDst |
    GfxUsage::eShaderResource | GfxUsage::eShaderStorage;
  bufferDesc.size = newSize;
  bufferDesc.flags = GfxBufferFlag::eDedicatedAllocation;

  GfxBuffer oldBuffer = std::move(m_nodeBuffer);
  GfxBuffer newBuffer = m_device->createBuffer(bufferDesc, GfxMemoryType::eAny);

  if (oldBuffer)
    context->copyBuffer(newBuffer, 0, oldBuffer, 0, oldSize);

  context->clearBuffer(newBuffer, oldSize, newSize - oldSize);

  m_nodeBuffer = std::move(newBuffer);
  return oldBuffer;
}


GfxBufferSlice GfxSceneInstanceBuffer::allocData(
        uint64_t                      dataSize) {
  return m_dataBuffer->alloc(dataSize, 64ull);
}


void GfxSceneInstanceBuffer::freeData(
  const GfxBufferSlice&               dataSlice) {
  m_dataBuffer->free(dataSlice);
}


void GfxSceneInstanceBuffer::trim() {
  m_dataBuffer->trim(0.4f);
}




GfxSceneInstanceManager::GfxSceneInstanceManager(
        GfxDevice                     device)
: m_gpuResources(std::move(device)) {

}


GfxSceneInstanceManager::~GfxSceneInstanceManager() {

}


GfxSceneNodeRef GfxSceneInstanceManager::createInstance(
  const GfxSceneInstanceDesc&         desc) {
  uint32_t index = m_instanceAllocator.allocate();

  // Initialize actual host data. Most of this can or needs
  // to be set dynamically anyway.
  auto& nodeData = m_instanceNodeData.emplace(index);
  nodeData.nodeIndex = int32_t(desc.nodeIndex);
  nodeData.flags = desc.flags;

  auto& hostData = m_instanceHostData.emplace(index);
  hostData.dirtyFlags = GfxSceneInstanceDirtyFlag::eDirtyNode;
  hostData.dataBuffer = GfxSceneInstanceDataBuffer(desc);

  // Mark instance as dirty so the node gets uploaded to the GPU
  addToDirtyList(index);

  return GfxSceneNodeRef(GfxSceneNodeType::eInstance, index);
}


void GfxSceneInstanceManager::destroyInstance(
        GfxSceneNodeRef               instance,
        uint32_t                      frameId) {
  uint32_t index = uint32_t(instance.index);

  GfxBufferSlice slice = std::move(m_instanceHostData[index].dataSlice);

  std::lock_guard lock(m_freeMutex);
  m_freeQueue.insert({ frameId, index });

  if (slice.buffer)
    m_freeSlices.insert({ frameId, std::move(slice) });
}


void GfxSceneInstanceManager::updateInstance(
        GfxSceneNodeRef               instance,
        GfxSceneInstanceFlags         flags) {
  uint32_t index = uint32_t(instance.index);

  auto& nodeData = m_instanceNodeData[index];

  if (nodeData.flags != flags) {
    nodeData.flags = flags;
    markDirty(index, GfxSceneInstanceDirtyFlag::eDirtyNode);
  }
}


void GfxSceneInstanceManager::updateJoints(
        GfxSceneNodeRef               instance,
        uint32_t                      first,
        uint32_t                      count,
  const QuatTransform*                joints) {
  uint32_t index = uint32_t(instance.index);

  auto& hostData = m_instanceHostData[index];

  auto dstHeader = hostData.dataBuffer.getHeader();
  auto dstJoints = hostData.dataBuffer.getJoints();

  dbg_assert(first + count <= dstHeader->jointCount);

  for (uint32_t i = 0; i < count; i++)
    dstJoints[first + i] = joints[i];

  markDirty(index, GfxSceneInstanceDirtyFlag::eDirtyRelativeTransforms);
}


void GfxSceneInstanceManager::updateWeights(
        GfxSceneNodeRef               instance,
        uint32_t                      first,
        uint32_t                      count,
  const int16_t*                      weights) {
  uint32_t index = uint32_t(instance.index);

  auto& hostData = m_instanceHostData[index];

  auto dstHeader = hostData.dataBuffer.getHeader();
  auto dstWeights = hostData.dataBuffer.getWeights();

  dbg_assert(first + count <= dstHeader->weightCount);

  for (uint32_t i = 0; i < count; i++)
    dstWeights[first + i] = weights[i];

  markDirty(index, GfxSceneInstanceDirtyFlag::eDirtyMorphTagretWeights);
}


void GfxSceneInstanceManager::updateShadingParameters(
        GfxSceneNodeRef               instance,
        size_t                        size,
  const void*                         data) {
  uint32_t index = uint32_t(instance.index);

  auto& hostData = m_instanceHostData[index];

  auto dstHeader = hostData.dataBuffer.getHeader();
  auto dstData = hostData.dataBuffer.getShadingParameters();

  size = std::min(size, size_t(dstHeader->instanceParameterSize));
  std::memcpy(dstData, data, size);

  markDirty(index, GfxSceneInstanceDirtyFlag::eDirtyShadingParameters);
}


void GfxSceneInstanceManager::updateMaterialParameters(
        GfxSceneNodeRef               instance,
        uint32_t                      draw,
        size_t                        size,
  const void*                         data) {
  uint32_t index = uint32_t(instance.index);

  auto& hostData = m_instanceHostData[index];

  auto dstDraws = hostData.dataBuffer.getDraws();
  auto dstData = hostData.dataBuffer.getMaterialParameters(draw);

  size = std::min(size, size_t(dstDraws[draw].shadingParameterSize));
  std::memcpy(dstData, data, size);

  markDirty(index, GfxSceneInstanceDirtyFlag::eDirtyMaterialParameters);
}


void GfxSceneInstanceManager::updateGeometryBuffer(
        GfxSceneNodeRef               instance,
        uint64_t                      geometryBuffer) {
  uint32_t index = uint32_t(instance.index);

  auto& nodeData = m_instanceNodeData[index];

  if (nodeData.geometryBuffer != geometryBuffer) {
    nodeData.geometryBuffer = geometryBuffer;
    markDirty(index, GfxSceneInstanceDirtyFlag::eDirtyNode);
  }
}


void GfxSceneInstanceManager::allocateGpuBuffer(
        GfxSceneNodeRef               instance) {
  uint32_t index = uint32_t(instance.index);

  auto& nodeData = m_instanceNodeData[index];
  auto& hostData = m_instanceHostData[index];

  if (hostData.dataSlice.buffer)
    return;

  hostData.dataSlice = m_gpuResources.allocData(hostData.dataBuffer.getSize());
  nodeData.propertyBuffer = hostData.dataSlice.buffer->getGpuAddress() + hostData.dataSlice.offset;

  markDirty(index, GfxSceneInstanceDirtyFlag::eDirtyNode | GfxSceneInstanceDirtyFlag::eDirtyHeader);
}


void GfxSceneInstanceManager::freeGpuBuffer(
        GfxSceneNodeRef               instance) {
  uint32_t index = uint32_t(instance.index);

  auto& nodeData = m_instanceNodeData[index];
  auto& hostData = m_instanceHostData[index];

  if (!hostData.dataSlice.buffer)
    return;

  hostData.dataSlice = GfxBufferSlice();
  nodeData.propertyBuffer = 0;

  markDirty(index, GfxSceneInstanceDirtyFlag::eDirtyNode);
}


void GfxSceneInstanceManager::commitUpdates(
  const GfxContext&                   context,
  const GfxScenePipelines&            pipelines,
        uint32_t                      currFrameId,
        uint32_t                      lastFrameId) {
  cleanupGpuBuffers(lastFrameId);

  updateBufferData(context, pipelines, currFrameId);

  cleanupInstanceNodes(lastFrameId);
  cleanupBufferSlices(lastFrameId);
}


void GfxSceneInstanceManager::processPassGroupInstances(
  const GfxContext&                   context,
  const GfxScenePipelines&            pipelines,
  const GfxSceneNodeManager&          nodeManager,
        uint32_t                      groupCount,
  const GfxScenePassGroupInfo*        groupInfos,
        uint32_t                      frameId) {
  context->beginDebugLabel("Process instances", 0xff78f0ff);
  context->beginDebugLabel("Prepare updates", 0xffb4f6ff);

  for (uint32_t i = 0; i < groupCount; i++) {
    GfxDescriptor dispatch = groupInfos[i].groupBuffer->getDispatchDescriptors(GfxSceneNodeType::eInstance).first;

    GfxSceneInstanceUpdatePrepareArgs args = { };
    args.instanceBufferVa = m_gpuResources.getGpuAddress();
    args.sceneBufferVa = nodeManager.getGpuAddress();
    args.groupBufferVa = groupInfos[i].groupBuffer->getGpuAddress();
    args.frameId = frameId;

    pipelines.prepareInstanceUpdates(context, dispatch, args);
  }

  context->memoryBarrier(
    GfxUsage::eShaderStorage | GfxUsage::eShaderResource | GfxUsage::eParameterBuffer, GfxShaderStage::eCompute,
    GfxUsage::eShaderStorage | GfxUsage::eShaderResource | GfxUsage::eParameterBuffer, GfxShaderStage::eCompute);

  context->endDebugLabel();
  context->beginDebugLabel("Execute updates", 0xffb4f6ff);

  for (uint32_t i = 0; i < groupCount; i++) {
    GfxDescriptor dispatch = groupInfos[i].groupBuffer->getDispatchDescriptors(GfxSceneNodeType::eInstance).second;

    GfxSceneInstanceUpdateExecuteArgs args = { };
    args.instanceBufferVa = m_gpuResources.getGpuAddress();
    args.sceneBufferVa = nodeManager.getGpuAddress();
    args.groupBufferVa = groupInfos[i].groupBuffer->getGpuAddress();

    pipelines.executeInstanceUpdates(context, dispatch, args);
  }

  context->memoryBarrier(
    GfxUsage::eShaderStorage | GfxUsage::eShaderResource | GfxUsage::eParameterBuffer, GfxShaderStage::eCompute,
    GfxUsage::eShaderStorage | GfxUsage::eShaderResource | GfxUsage::eParameterBuffer, GfxShaderStage::eCompute);

  context->endDebugLabel();
  context->endDebugLabel();
}


void GfxSceneInstanceManager::updateBufferData(
  const GfxContext&                   context,
  const GfxScenePipelines&            pipelines,
        uint32_t                      frameId) {
  context->beginDebugLabel("Update instances", 0xff96c096u);

  for (auto index : m_dirtyIndices) {
    // Update frame ID for all nodes before dispatching the node update.
    m_instanceNodeData[index].dirtyFrameId = frameId;

    // Check dirty flags and dispatch the necessary buffer updates
    auto& hostData = m_instanceHostData[index];

    if (hostData.dataSlice.buffer) {
      GfxSceneInstanceDirtyFlags dirtyFlags = hostData.dirtyFlags.exchange(0);

      auto header = hostData.dataBuffer.getHeader();
      auto draws = hostData.dataBuffer.getDraws();

      if (dirtyFlags & GfxSceneInstanceDirtyFlag::eDirtyHeader) {
        uploadInstanceData(context, hostData, 0, sizeof(*header));
        uploadInstanceData(context, hostData, header->drawOffset, header->drawCount * sizeof(*draws));
      }

      if (dirtyFlags & GfxSceneInstanceDirtyFlag::eDirtyRelativeTransforms)
        uploadInstanceData(context, hostData, header->jointRelativeOffset, header->jointCount * sizeof(QuatTransform));

      if (dirtyFlags & GfxSceneInstanceDirtyFlag::eDirtyMorphTagretWeights) {
        uint32_t weightSize = header->weightCount * sizeof(int16_t);
        uploadInstanceData(context, hostData, header->weightOffset + 2u * weightSize, weightSize);
      }

      if (dirtyFlags & GfxSceneInstanceDirtyFlag::eDirtyShadingParameters)
        uploadInstanceData(context, hostData, header->instanceParameterOffset, header->instanceParameterSize);

      if (dirtyFlags & GfxSceneInstanceDirtyFlag::eDirtyMaterialParameters) {
        // Batch material parameter updates. In general, this data will always be
        // laid out in such a way that this can be batched, but the structure allows
        // for different layouts, and there's no harm in being conservative here.
        uint32_t updateOffset = 0;
        uint32_t updateSize = 0;

        for (uint32_t i = 0; i < header->drawCount; i++) {
          if (!draws[i].shadingParameterSize)
            continue;

          if (draws[i].shadingParameterOffset == updateOffset + updateSize) {
            updateSize += draws[i].shadingParameterSize;
          } else {
            uploadInstanceData(context, hostData, updateOffset, updateSize);

            updateOffset = draws[i].shadingParameterOffset;
            updateSize = draws[i].shadingParameterSize;
          }
        }

        uploadInstanceData(context, hostData, updateOffset, updateSize);
      }
    }
  }

  // Update basic node data
  if (!m_dirtyIndices.empty()) {
    resizeGpuBuffer(context, frameId);

    pipelines.updateSceneBuffer(context,
      m_gpuResources.getGpuAddress(),
      m_dirtyIndices.size(),
      m_dirtyIndices.data(),
      m_instanceNodeData);
  }

  m_dirtyIndices.clear();

  context->endDebugLabel();
}


void GfxSceneInstanceManager::cleanupInstanceNodes(
        uint32_t                      frameId) {
  // Release resources for all nodes freed in the given frame
  auto range = m_freeQueue.equal_range(frameId);

  for (auto i = range.first; i != range.second; i++) {
    uint32_t index = i->second;

    m_instanceHostData.erase(index);
    m_instanceNodeData.erase(index);

    m_instanceAllocator.free(index);
  }

  m_freeQueue.erase(range.first, range.second);
}


void GfxSceneInstanceManager::cleanupBufferSlices(
        uint32_t                      frameId) {
  auto range = m_freeSlices.equal_range(frameId);

  for (auto i = range.first; i != range.second; i++)
    m_gpuResources.freeData(i->second);

  m_freeSlices.erase(range.first, range.second);
}


void GfxSceneInstanceManager::cleanupGpuBuffers(
        uint32_t                      frameId) {
  m_gpuBuffers.erase(frameId);
}


void GfxSceneInstanceManager::markDirty(
        uint32_t                      index,
        GfxSceneInstanceDirtyFlags    flags) {
  if (!m_instanceHostData[index].dirtyFlags.set(flags))
    addToDirtyList(index);
}


void GfxSceneInstanceManager::addToDirtyList(
        uint32_t                      index) {
  std::lock_guard lock(m_dirtyMutex);
  m_dirtyIndices.push_back(index);
}


void GfxSceneInstanceManager::resizeGpuBuffer(
  const GfxContext&                   context,
        uint32_t                      frameId) {
  uint32_t count = m_instanceAllocator.getCount();
  GfxBuffer oldBuffer = m_gpuResources.resizeBuffer(context, count);

  if (oldBuffer)
    m_gpuBuffers.insert({ frameId, std::move(oldBuffer) });
}


void GfxSceneInstanceManager::uploadInstanceData(
  const GfxContext&                   context,
  const GfxSceneInstanceHostInfo&     hostData,
        uint32_t                      offset,
        uint32_t                      size) {
  if (!size)
    return;

  GfxScratchBuffer scratch = context->writeScratch(
    GfxUsage::eTransferSrc, size, hostData.dataBuffer.getAt(offset));

  context->copyBuffer(
    hostData.dataSlice.buffer,
    hostData.dataSlice.offset + offset,
    scratch.buffer, scratch.offset, size);
}

}
