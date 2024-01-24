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


GfxDescriptor GfxScenePassGroupBuffer::getOcclusionTestDispatchDescriptor(
        GfxShaderStage                stage) const {
  if (!m_buffer)
    return GfxDescriptor();

  uint32_t offset = m_header.bvhOcclusionTestOffset;
  
  switch (stage) {
    case GfxShaderStage::eCompute:
      offset += offsetof(GfxSceneBvhOcclusionTestHeader, csDispatch);
      break;
    case GfxShaderStage::eMesh:
      offset += offsetof(GfxSceneBvhOcclusionTestHeader, msDispatch);
      break;
    default:
      return GfxDescriptor();
  }

  return m_buffer->getDescriptor(GfxUsage::eParameterBuffer, offset, sizeof(GfxDispatchArgs));
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
  const GfxSceneNodeManager&          nodeManager) {
  context->beginDebugLabel("Update pass group buffer", 0xff96c096u);

  bool doClear = resizeBuffer(context, nodeManager);

  auto scratch = context->allocScratch(GfxUsage::eCpuWrite | GfxUsage::eTransferSrc, sizeof(m_header));
  std::memcpy(scratch.map(GfxUsage::eCpuWrite, 0), &m_header, sizeof(m_header));

  context->copyBuffer(m_buffer, 0,
    scratch.buffer, scratch.offset,
    scratch.size);

  if (doClear) {
    context->clearBuffer(m_buffer, scratch.size,
      m_buffer->getDesc().size - scratch.size);
  }

  context->endDebugLabel();

  // Subsequent frames should perform occlusion
  // testing normally, barring any future updates
  m_header.ignoreOcclusionTestMask = 0u;
}


void GfxScenePassGroupBuffer::resetUpdateLists(
  const GfxContext&                   context,
  const GfxScenePipelines&            pipelines) {
  context->beginDebugLabel("Reset lists", 0xff7d9cd5);

  pipelines.resetUpdateLists(context, getGpuAddress());

  context->memoryBarrier(
    GfxUsage::eShaderStorage | GfxUsage::eParameterBuffer, GfxShaderStage::eCompute,
    GfxUsage::eShaderStorage | GfxUsage::eParameterBuffer, GfxShaderStage::eCompute);

  context->endDebugLabel();
}


void GfxScenePassGroupBuffer::performOcclusionTest(
  const GfxContext&                   context,
  const GfxScenePipelines&            pipelines,
  const GfxCommonHizImage&            hizImage,
  const GfxSceneNodeManager&          nodeManager,
  const GfxScenePassManager&          passManager,
        uint32_t                      passIndex,
        uint32_t                      frameId) {
  context->beginDebugLabel("BVH occlusion test", 0xff841b67);

  // Reset mesh shader dispatch parameters
  context->beginDebugLabel("Reset parameters", 0xff7d9cd5);
  pipelines.resetUpdateLists(context, getGpuAddress());

  context->memoryBarrier(
    GfxUsage::eShaderStorage, GfxShaderStage::eCompute,
    GfxUsage::eShaderStorage | GfxUsage::eParameterBuffer, GfxShaderStage::eCompute);
  context->endDebugLabel();

  // First pass: Use the Hi-Z buffer to quickly reject or accept nodes.
  context->beginDebugLabel("Pre-cull BVH nodes", 0xffd79dc7);

  GfxSceneOcclusionTestArgs args = { };
  args.passInfoVa = passManager.getGpuAddress();
  args.passGroupVa = getGpuAddress();
  args.sceneVa = nodeManager.getGpuAddress();
  args.passIndex = passIndex;
  args.frameId = frameId;

  pipelines.precullBvhOcclusion(context, hizImage.getImageView(0),
    getOcclusionTestDispatchDescriptor(GfxShaderStage::eCompute),
    args);

  context->memoryBarrier(
    GfxUsage::eShaderStorage, GfxShaderStage::eCompute,
    GfxUsage::eShaderStorage | GfxUsage::eShaderResource | GfxUsage::eParameterBuffer,
    GfxShaderStage::eMesh | GfxShaderStage::eFragment);
  context->endDebugLabel();

  // Second pass: Render nodes and compare against a fixed mip level of the
  // Hi-Z buffer, which is 1/16th the size of the original render resolution
  // in either dimension. We do this to reduce the number of FS invocations,
  // and this is generally expected to be reliable due to conservative
  // rasterization being used for rendering.
  context->beginDebugLabel("Render BVH nodes", 0xffd79dc7);

  pipelines.testBvhOcclusion(context, hizImage.getImageMipView(3, 0),
    getOcclusionTestDispatchDescriptor(GfxShaderStage::eMesh),
    args);

  context->memoryBarrier(
    GfxUsage::eShaderStorage | GfxUsage::eShaderResource | GfxUsage::eParameterBuffer,
    GfxShaderStage::eMesh | GfxShaderStage::eFragment,
    GfxUsage::eShaderStorage | GfxUsage::eShaderResource,
    GfxShaderStage::eCompute);
  context->endDebugLabel();

  context->endDebugLabel();
}


bool GfxScenePassGroupBuffer::resizeBuffer(
  const GfxContext&                   context,
  const GfxSceneNodeManager&          nodeManager) {
  std::array<uint32_t, uint32_t(GfxSceneNodeType::eCount)> nodeCounts = { };

  for (uint32_t i = 0; i < nodeCounts.size(); i++)
    nodeCounts[i] = nodeManager.getNodeCount(GfxSceneNodeType(i));

  // Do nothing if the none of the capacities grow
  bool hasGrownCapacity = false;

  for (uint32_t i = 0; i < uint32_t(GfxSceneNodeType::eCount) && !hasGrownCapacity; i++)
    hasGrownCapacity = nodeCounts[i] > m_nodeCounts[i];

  if (!hasGrownCapacity)
    return false;

  // Need to invalidate occlusion test results as well
  m_header.ignoreOcclusionTestMask = (2u << (m_header.passCount - 1u)) - 1u;

  // Align capacities in such a way that we're unlikely to need to
  // resize or restructure the buffer again very soon
  for (uint32_t i = 0; i < uint32_t(GfxSceneNodeType::eCount); i++)
    m_nodeCounts[i] = align(nodeCounts[i], 4096u);

  // Compute minimum buffer size required to store everything
  uint32_t maxBvhNodes = m_nodeCounts[size_t(GfxSceneNodeType::eBvh)];

  uint32_t allocator = 0u;
  allocStorage(allocator, sizeof(m_header));

  // Allocate two traversal list items per BVH node since
  // we may need to revisit nodes after occlusion testing.
  m_header.bvhListOffset = allocStorage(allocator,
    sizeof(GfxSceneBvhListHeader) +
    sizeof(GfxSceneBvhListEntry) * maxBvhNodes * 2u);

  m_header.bvhVisibilityOffset = allocStorage(allocator,
    sizeof(GfxSceneBvhVisibility) * maxBvhNodes);

  m_header.bvhOcclusionTestOffset = allocStorage(allocator,
    sizeof(GfxSceneBvhOcclusionTestHeader) +
    sizeof(GfxSceneNodeRef) * maxBvhNodes);

  // Keep the offset for any unused node type at zero. This also allows
  // us to ignore certain node types for certain pass groups entirely,
  // e.g. light nodes during shadow passes.
  for (uint32_t i = uint32_t(GfxSceneNodeType::eBuiltInCount); i < uint32_t(GfxSceneNodeType::eCount); i++) {
    GfxScenePassTypedNodeListOffsets offsets = { };

    if (m_nodeCounts[i]) {
      offsets.nodeList = allocStorage(allocator,
        sizeof(GfxSceneNodeListHeader) +
        sizeof(GfxSceneNodeListEntry) * m_nodeCounts[i]);
      offsets.updateList = allocStorage(allocator,
        sizeof(GfxSceneNodeListHeader) +
        sizeof(uint32_t) * m_nodeCounts[i]);
    }

    m_header.listOffsets.at(i - uint32_t(GfxSceneNodeType::eBuiltInCount)) = offsets;
  }

  // If possible, just reuse the existing buffer. We don't need to do
  // anything, the header update and the required initialization pass
  // will set everything up.
  if (m_buffer && m_buffer->getDesc().size >= allocator)
    return true;

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
    context->trackObject(m_buffer);

  m_buffer = m_device->createBuffer(bufferDesc, GfxMemoryType::eAny);
  return true;
}


uint32_t GfxScenePassGroupBuffer::allocStorage(
        uint32_t&                     allocator,
        size_t                        size) {
  uint32_t offset = allocator;
  allocator += align(uint32_t(size), 256u);
  return offset;
}




GfxScenePassManager::GfxScenePassManager(
        GfxDevice                     device)
: m_device(std::move(device)) {

}


GfxScenePassManager::~GfxScenePassManager() {

}


uint16_t GfxScenePassManager::createRenderPass(
  const GfxScenePassDesc&             desc) {
  uint16_t index = uint16_t(m_passAllocator.allocate());

  auto& passInfo = m_passData.emplace(index);
  passInfo.flags = desc.flags;
  passInfo.dirtyFrameId = ~0u;
  passInfo.passTypeMask = desc.typeMask;
  passInfo.cameraNode = desc.cameraNode;
  passInfo.cameraJoint = desc.cameraJoint;
  passInfo.mirrorNode = desc.mirrorNode;
  passInfo.mirrorJoint = desc.mirrorJoint;
  passInfo.lodDistanceScale = 1.0f;

  GfxScenePassFlags flags = desc.flags;
  flags -= GfxScenePassFlag::eKeepMetadata;
  flags |= GfxScenePassFlag::eIgnoreOcclusionTest;

  addDirtyPass(index, flags);
  return index;
}


void GfxScenePassManager::freeRenderPass(
        uint16_t                      pass) {
  // Instead of destroying the pass object, just initialize it with
  // default properties so that update shaders can safely skip it.
  auto& passInfo = m_passData[pass];

  // Keep the dirty frame ID since we abuse this for dirty tracking
  uint32_t dirtyFrameId = passInfo.dirtyFrameId;

  passInfo = GfxScenePassInfo();
  passInfo.dirtyFrameId = dirtyFrameId;
  passInfo.cameraNode = -1;
  passInfo.mirrorNode = -1;

  m_passAllocator.free(pass);

  addDirtyPass(pass, 0u);
}


void GfxScenePassManager::updateRenderPassMetadata(
        uint16_t                      pass,
  const GfxScenePassDesc&             desc) {
  auto& passInfo = m_passData[pass];
  passInfo.flags = desc.flags;
  passInfo.passTypeMask = desc.typeMask;
  passInfo.cameraNode = desc.cameraNode;
  passInfo.cameraJoint = desc.cameraJoint;
  passInfo.mirrorNode = desc.mirrorNode;
  passInfo.mirrorJoint = desc.mirrorJoint;

  GfxScenePassFlags flags = desc.flags;
  flags -= GfxScenePassFlag::eKeepMetadata;
  flags |= GfxScenePassFlag::eIgnoreOcclusionTest;

  addDirtyPass(pass, flags);
}


void GfxScenePassManager::updateRenderPassProjection(
        uint16_t                      pass,
  const Projection&                   projection) {
  auto& passInfo = m_passData[pass];
  passInfo.projection = projection;

  if (!(passInfo.flags & GfxScenePassFlag::eKeepProjection))
    addDirtyPass(pass, passInfo.flags | GfxScenePassFlag::eIgnoreOcclusionTest);
}


void GfxScenePassManager::updateRenderPassTransform(
        uint16_t                      pass,
  const QuatTransform&                transform,
        bool                          cut) {
  auto& passInfo = m_passData[pass];
  passInfo.viewTransform = transform;

  GfxScenePassFlags flags = passInfo.flags;

  if (cut)
    flags |= GfxScenePassFlag::eIgnoreOcclusionTest;

  if (!(flags & GfxScenePassFlag::eKeepViewTransform))
    addDirtyPass(pass, flags);
}


void GfxScenePassManager::updateRenderPassMirrorPlane(
        uint16_t                      pass,
  const Vector4D&                     plane,
        bool                          cut) {
  auto& passInfo = m_passData[pass];
  passInfo.mirrorPlane = plane;

  GfxScenePassFlags flags = passInfo.flags;

  if (cut)
    flags |= GfxScenePassFlag::eIgnoreOcclusionTest;

  if (!(flags & GfxScenePassFlag::eKeepMirrorPlane))
    addDirtyPass(pass, flags);
}


void GfxScenePassManager::updateRenderPassViewDistance(
        uint16_t                      pass,
        float                         viewDistance) {
  auto& passInfo = m_passData[pass];
  passInfo.viewDistanceLimit = viewDistance;

  if (!(passInfo.flags & GfxScenePassFlag::eKeepViewDistance))
    addDirtyPass(pass, passInfo.flags);
}


void GfxScenePassManager::updateRenderPassLodSelection(
        uint16_t                      pass,
        float                         factor) {
  auto& passInfo = m_passData[pass];
  passInfo.lodDistanceScale = factor;

  if (!(passInfo.flags & GfxScenePassFlag::eKeepViewDistance))
    addDirtyPass(pass, passInfo.flags);
}


void GfxScenePassManager::updateRenderPassViewportLayer(
        uint16_t                      pass,
        uint32_t                      viewport,
        uint32_t                      layer) {
  auto& passInfo = m_passData[pass];
  passInfo.viewportIndex = viewport;
  passInfo.layerIndex = layer;

  if (!(passInfo.flags & GfxScenePassFlag::eKeepViewportLayerIndex))
    addDirtyPass(pass, passInfo.flags);
}


void GfxScenePassManager::updateRenderPassViewportRegion(
        uint16_t                      pass,
  const Offset2D&                     offset,
  const Extent2D&                     extent) {
  auto& passInfo = m_passData[pass];
  passInfo.viewportOffset = offset;
  passInfo.viewportExtent = extent;

  if (!(passInfo.flags & GfxScenePassFlag::eKeepViewportRegion))
    addDirtyPass(pass, passInfo.flags);
}


void GfxScenePassManager::commitUpdates(
  const GfxContext&                   context,
  const GfxScenePipelines&            pipelines,
        uint32_t                      frameId) {
  context->beginDebugLabel("Update render passes", 0xff96c096u);
  resizeBuffer(context, m_passAllocator.getCount(), frameId);

  if (!m_dirtyList.empty())
    dispatchHostCopy(context, pipelines, frameId);

  dispatchUpdateListInit(context, pipelines);
  context->endDebugLabel();
}


void GfxScenePassManager::processPasses(
  const GfxContext&                   context,
  const GfxScenePipelines&            pipelines,
  const GfxSceneNodeManager&          nodeManager,
        uint32_t                      frameId) {
  context->beginDebugLabel("Process render passes", 0xff6490ff);
  context->beginDebugLabel("Scan pass list", 0xffa0beff);

  GfxPassInfoUpdatePrepareArgs prepArgs = { };
  prepArgs.passInfoVa = getGpuAddress();
  prepArgs.passListVa = getGpuAddress() + m_bufferUpdateOffset;
  prepArgs.frameId = frameId;
  prepArgs.passCount = m_passAllocator.getCount();

  pipelines.prepareRenderPassUpdates(context, prepArgs);

  context->memoryBarrier(
    GfxUsage::eShaderStorage | GfxUsage::eShaderResource, GfxShaderStage::eCompute,
    GfxUsage::eShaderStorage | GfxUsage::eShaderResource | GfxUsage::eParameterBuffer, GfxShaderStage::eCompute);

  context->endDebugLabel();

  context->beginDebugLabel("Execute update", 0xffa0beff);

  GfxPassInfoUpdateExecuteArgs execArgs = { };
  execArgs.passInfoVa = getGpuAddress();
  execArgs.passListVa = getGpuAddress() + m_bufferUpdateOffset;
  execArgs.sceneVa = nodeManager.getGpuAddress();
  execArgs.frameId = frameId;

  GfxDescriptor dispatch = m_buffer->getDescriptor(GfxUsage::eParameterBuffer,
    m_bufferUpdateOffset, sizeof(GfxDispatchArgs));
  pipelines.executeRenderPassUpdates(context, dispatch, execArgs);

  context->memoryBarrier(
    GfxUsage::eShaderStorage | GfxUsage::eShaderResource, GfxShaderStage::eCompute,
    GfxUsage::eShaderResource, GfxShaderStage::eCompute | GfxShaderStage::eTask | GfxShaderStage::eMesh);

  context->endDebugLabel();
  context->endDebugLabel();
}


void GfxScenePassManager::resizeBuffer(
        GfxContext                    context,
        uint32_t                      passCount,
        uint32_t                      currFrameId) {
  // 1/sqrt = sin(pi/4) = cos(pi/4)
  constexpr float f = 0.7071067812f;

  // Pad pass count so that we avoid frequent reallocations
  passCount = align(passCount, 1024u);

  uint64_t passDataSize = align<uint64_t>(sizeof(GfxScenePassBufferHeader) + sizeof(GfxScenePassInfo) * passCount, 256u);
  uint64_t passListSize = align<uint64_t>(sizeof(GfxSceneNodeListHeader) + sizeof(uint32_t) * passCount, 256u);

  uint64_t newSize = passDataSize + passListSize;
  uint64_t oldSize = m_buffer ? m_buffer->getDesc().size : uint64_t(0u);

  if (newSize <= oldSize)
    return;

  GfxBufferDesc bufferDesc;
  bufferDesc.debugName = "Render pass buffer";
  bufferDesc.usage = GfxUsage::eParameterBuffer |
    GfxUsage::eTransferDst |
    GfxUsage::eTransferSrc |
    GfxUsage::eShaderResource |
    GfxUsage::eShaderStorage;
  bufferDesc.size = newSize;
  bufferDesc.flags = GfxBufferFlag::eDedicatedAllocation;

  GfxBuffer newBuffer = m_device->createBuffer(bufferDesc, GfxMemoryType::eAny);
  GfxBuffer oldBuffer = std::move(m_buffer);

  // Initialize the buffer with a fixed header
  GfxScenePassBufferHeader header = { };
  header.cubeFaceRotations[0] = Quaternion(0.0f, 0.0f,    f,    f);  /* +X */
  header.cubeFaceRotations[1] = Quaternion(0.0f, 0.0f,   -f,    f);  /* -X */
  header.cubeFaceRotations[2] = Quaternion(   f, 0.0f, 0.0f,    f);  /* +Y */
  header.cubeFaceRotations[3] = Quaternion(  -f, 0.0f, 0.0f,    f);  /* -Y */
  header.cubeFaceRotations[4] = Quaternion(0.0f, 0.0f, 1.0f, 0.0f);  /* +Z */
  header.cubeFaceRotations[5] = Quaternion(0.0f, 0.0f, 0.0f, 1.0f);  /* -Z */

  // Initialize buffer and copy existing pass infos from the old buffer.
  GfxScratchBuffer scratch = context->writeScratch(GfxUsage::eTransferSrc, header);
  context->copyBuffer(newBuffer, 0, scratch.buffer, scratch.offset, scratch.size);

  if (oldBuffer) {
    context->copyBuffer(
      newBuffer, sizeof(header),
      oldBuffer, sizeof(header),
      m_bufferUpdateOffset - sizeof(header));
  }

  uint64_t clearOffset = oldBuffer ? m_bufferUpdateOffset : sizeof(header);
  context->clearBuffer(newBuffer, clearOffset, newSize - clearOffset);

  context->memoryBarrier(
    GfxUsage::eTransferDst, 0,
    GfxUsage::eTransferSrc | GfxUsage::eShaderStorage | GfxUsage::eShaderResource,
    GfxShaderStage::eCompute);

  m_bufferUpdateOffset = passDataSize;  
  m_buffer = std::move(newBuffer);

  if (oldBuffer)
    context->trackObject(oldBuffer);
}


void GfxScenePassManager::dispatchUpdateListInit(
  const GfxContext&                   context,
  const GfxScenePipelines&            pipelines) {
  pipelines.initRenderPassUpdateList(context,
    getGpuAddress() + m_bufferUpdateOffset);
}


void GfxScenePassManager::dispatchHostCopy(
  const GfxContext&                   context,
  const GfxScenePipelines&            pipelines,
        uint32_t                      currFrameId) {
  GfxScratchBuffer hostPassInfos = context->allocScratch(
    GfxUsage::eCpuWrite | GfxUsage::eShaderResource,
    sizeof(GfxScenePassInfo) * m_dirtyList.size());

  GfxScratchBuffer hostPassIndices = context->allocScratch(
    GfxUsage::eCpuWrite | GfxUsage::eShaderResource,
    sizeof(uint16_t) * m_dirtyList.size());

  // Populate scratch buffers
  auto passInfos = reinterpret_cast<GfxScenePassInfo*>(hostPassInfos.map(GfxUsage::eCpuWrite, 0));
  auto passIndices = reinterpret_cast<uint16_t*>(hostPassIndices.map(GfxUsage::eCpuWrite, 0));

  for (size_t i = 0; i < m_dirtyList.size(); i++) {
    const auto& dirty = m_dirtyList[i];

    GfxScenePassInfo passInfo = m_passData[dirty.pass];
    passInfo.flags = dirty.flags;
    passInfo.dirtyFrameId = currFrameId;

    passInfos[i] = passInfo;
    passIndices[i] = dirty.pass;

    m_passData[dirty.pass].dirtyFrameId = ~0u;
  }

  // Dispatch host update shader
  GfxPassInfoUpdateCopyArgs args = { };
  args.dstPassInfoVa = getGpuAddress();
  args.srcPassIndexVa = hostPassIndices.getGpuAddress();
  args.srcPassInfoVa = hostPassInfos.getGpuAddress();
  args.frameId = currFrameId;
  args.passUpdateCount = uint32_t(m_dirtyList.size());

  pipelines.uploadRenderPassInfos(context, args);

  m_dirtyList.clear();
}


void GfxScenePassManager::addDirtyPass(
        uint16_t                      pass,
        GfxScenePassFlags             flags) {
  auto& passInfo = m_passData[pass];

  // Use the dirty frame ID field as an index into the dirty list.
  // If the index is invalid, append a new element.
  uint32_t dirtyCount = m_dirtyList.size();

  if (passInfo.dirtyFrameId < dirtyCount) {
    auto& dirty = m_dirtyList[passInfo.dirtyFrameId];
    dbg_assert(dirty.pass == pass);

    // Logically AND all the flags to ensure that some potentially undesired
    // keep flags get masked out, and OR special flags back in.
    GfxScenePassFlags specialFlags = GfxScenePassFlag::eIgnoreOcclusionTest;

    dirty.flags &= flags;
    dirty.flags |= flags & specialFlags;
  } else {
    passInfo.dirtyFrameId = dirtyCount;

    DirtyPass dirty = { };
    dirty.pass = pass;
    dirty.flags = flags;

    m_dirtyList.push_back(dirty);
  }
}

}
