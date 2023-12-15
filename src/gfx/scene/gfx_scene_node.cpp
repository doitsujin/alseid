#include <algorithm>

#include "../../util/util_small_vector.h"

#include "gfx_scene_node.h"
#include "gfx_scene_pass.h"

namespace as {

GfxSceneNodeBuffer::GfxSceneNodeBuffer(
        GfxDevice                     device)
: m_device(std::move(device)) {

}


GfxSceneNodeBuffer::~GfxSceneNodeBuffer() {

}


GfxBuffer GfxSceneNodeBuffer::resizeBuffer(
  const GfxContext&                   context,
  const GfxSceneNodeBufferDesc&       desc) {
  // Don't do anything if the buffer layout does not change
  GfxSceneNodeBufferDesc oldDesc = m_desc;

  if (desc.nodeCount <= oldDesc.nodeCount
   && desc.bvhCount <= oldDesc.bvhCount)
    return GfxBuffer();

  // Align all capacities to large enough numbers to reduce reallocations.
  m_desc.nodeCount = std::max(m_desc.nodeCount, align(desc.nodeCount, 1u << 16));
  m_desc.bvhCount  = std::max(m_desc.bvhCount,  align(desc.bvhCount,  1u << 12));

  // Compute the actual buffer layout.
  uint32_t allocator = 0;
  allocStorage(allocator, sizeof(GfxSceneNodeHeader));

  GfxSceneNodeHeader newHeader = { };
  newHeader.nodeParameterOffset = allocStorage(allocator,
    sizeof(GfxSceneNodeInfo) * m_desc.nodeCount);

  newHeader.nodeTransformOffset = allocStorage(allocator,
    sizeof(GfxSceneNodeTransform) * m_desc.nodeCount * 2u);

  newHeader.nodeResidencyOffset = allocStorage(allocator,
    sizeof(GfxSceneNodeResidencyFlags) * m_desc.nodeCount);

  newHeader.nodeCount = m_desc.nodeCount;

  newHeader.bvhOffset = allocStorage(allocator,
    sizeof(GfxSceneBvhInfo) * m_desc.bvhCount);

  newHeader.bvhCount = m_desc.bvhCount;

  GfxSceneNodeHeader oldHeader = m_header;

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
  context->beginDebugLabel("Copy scene buffer", 0xffffc096u);
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
      sizeof(GfxSceneNodeTransform) * oldDesc.nodeCount * 2u);

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


uint32_t GfxSceneNodeBuffer::allocStorage(
        uint32_t&                     allocator,
        size_t                        size) {
  uint32_t offset = allocator;
  allocator += align(uint32_t(size), 256u);
  return offset;
}




GfxSceneNodeManager::GfxSceneNodeManager(
        GfxDevice                     device)
: m_gpuResources(std::move(device)) {

}


GfxSceneNodeManager::~GfxSceneNodeManager() {

}


uint32_t GfxSceneNodeManager::createNode() {
  uint32_t index = m_nodeAllocator.allocate();

  auto& nodeData = m_nodeData.emplace(index);
  nodeData.rotation = Vector4D(0.0f, 0.0f, 0.0f, 1.0f);
  nodeData.translation = Vector3D(0.0f, 0.0f, 0.0f);
  nodeData.updateFrameId = 0;
  nodeData.parentNode = -1;
  nodeData.parentTransform = -1;
  nodeData.parentNodeRef = GfxSceneNodeRef();
  nodeData.nodeRef = GfxSceneNodeRef();

  m_hostData.emplace(index);

  markDirty(index, GfxSceneNodeDirtyFlag::eDirtyNode);
  return index;  
}


void GfxSceneNodeManager::destroyNode(
        uint32_t                      index,
        uint32_t                      frameId) {
  std::lock_guard lock(m_freeMutex);

  m_freeNodeQueue.insert({ frameId,
    GfxSceneNodeRef(GfxSceneNodeType::eNone, index) });

  removeFromNodeMap(m_nodeData[index].nodeRef);
}


void GfxSceneNodeManager::updateNodeReference(
        uint32_t                      index,
        GfxSceneNodeRef               reference) {
  auto& node = m_nodeData[index];

  removeFromNodeMap(node.nodeRef);
  insertIntoNodeMap(reference, index);

  node.nodeRef = reference;

  markDirty(index, GfxSceneNodeDirtyFlag::eDirtyNode);
}


void GfxSceneNodeManager::updateNodeParent(
        uint32_t                      index,
        int32_t                       parent,
        int32_t                       transform) {
  auto& node = m_nodeData[index];

  if (parent >= 0) {
    node.parentNode = parent;
    node.parentNodeRef = m_nodeData[parent].nodeRef;
    node.parentTransform = transform;
  } else {
    node.parentNode = -1;
    node.parentNodeRef = GfxSceneNodeRef(GfxSceneNodeType::eNone, 0);
    node.parentTransform = -1;
  }

  markDirty(index, GfxSceneNodeDirtyFlag::eDirtyNode);
}


void GfxSceneNodeManager::updateNodeTransform(
        uint32_t                      index,
  const QuatTransform&                transform) {
  auto& node = m_nodeData[index];
  node.rotation = transform.getRotation().getVector();
  node.translation = Vector3D(transform.getTranslation());

  markDirty(index, GfxSceneNodeDirtyFlag::eDirtyNode);
}


GfxSceneNodeRef GfxSceneNodeManager::createBvhNode(
  const GfxSceneBvhDesc&              desc) {
  uint32_t index = m_bvhAllocator.allocate();

  // Initialize BVH properties
  auto& bvhData = m_bvhData.emplace(index);
  bvhData.nodeIndex = desc.nodeIndex;
  bvhData.aabb = desc.aabb;
  bvhData.maxDistance = desc.maxDistance;

  // Mark node as dirty to update all the BVH bits
  markDirty(bvhData.nodeIndex,
    GfxSceneNodeDirtyFlag::eDirtyBvhNode |
    GfxSceneNodeDirtyFlag::eDirtyBvhChain);

  return GfxSceneNodeRef(GfxSceneNodeType::eBvh, index);
}


void GfxSceneNodeManager::destroyBvhNode(
        GfxSceneNodeRef               reference,
        uint32_t                      frameId) {
  std::lock_guard lock(m_freeMutex);

  while (reference.type == GfxSceneNodeType::eBvh) {
    auto& bvhNode = m_bvhData[uint32_t(reference.index)];

    for (uint32_t i = 0; i < bvhNode.childCount; i++) {
      auto& childHostData = m_hostData[bvhNode.childNodes[i]];

      if (childHostData.parentBvhNode == reference) {
        childHostData.parentBvhNode = GfxSceneNodeRef();
        childHostData.childIndex = 0;
      }
    }

    m_freeNodeQueue.insert({ frameId, reference });
    reference = m_bvhData[uint32_t(reference.index)].chainedBvh;
  }  
}


void GfxSceneNodeManager::updateBvhVolume(
        GfxSceneNodeRef               reference,
  const GfxAabb<float16_t>&           aabb,
        float16_t                     maxDistance) {
  uint32_t index = uint32_t(reference.index);

  auto& bvh = m_bvhData[index];
  bvh.aabb = aabb;
  bvh.maxDistance = maxDistance;

  markDirty(bvh.nodeIndex, GfxSceneNodeDirtyFlag::eDirtyBvhNode);
}


void GfxSceneNodeManager::attachNodesToBvh(
        GfxSceneNodeRef               target,
        uint32_t                      nodeCount,
  const GfxSceneNodeRef*              nodes) {
  bool targetMarkedDirty = false;

  // Just take a global lock since locking each individual node without
  // introducing deadlocks is very difficult and also inefficient.
  std::lock_guard lock(m_nodeMutex);

  uint32_t childDepth = 0u;

  for (uint32_t i = 0; i < nodeCount; i++) {
    uint32_t nodeIndex = getNodeIndex(nodes[i]);

    // Attaching a node to its current parent is a no-op, just skip it.
    auto& nodeHostData = m_hostData[nodeIndex];

    if (nodeHostData.parentBvhNode == target)
      continue;

    // Implicitly requires that nodes be fully created before being
    // attached to a BVH node
    if (nodes[i].type == GfxSceneNodeType::eBvh)
      childDepth = std::max(childDepth, nodeHostData.childDepth + 1u);

    // Detach node from its current parent and assign it a null node.
    // Generally, we expect to mostly work with orphaned nodes here.
    GfxSceneNodeRef parent = std::exchange(nodeHostData.parentBvhNode, GfxSceneNodeRef());

    if (parent.type == GfxSceneNodeType::eBvh) {
      auto& parentBvh = m_bvhData[uint32_t(parent.index)];

      uint32_t childCount = --parentBvh.childCount;
      uint32_t childIndex = std::exchange(nodeHostData.childIndex, 0u);
      uint32_t lastChild = parentBvh.childNodes[childCount];

      parentBvh.childNodes[childIndex] = lastChild;
      parentBvh.childNodes[childCount] = 0u;

      if (childIndex != childCount) {
        auto& childHostData = m_hostData[lastChild];
        childHostData.childIndex = childIndex;
      }

      markDirty(parentBvh.nodeIndex, GfxSceneNodeDirtyFlag::eDirtyBvhChain);
    }

    // Attach nodes to the target BVH node, unless we're orphaning them.
    if (target.type == GfxSceneNodeType::eBvh) {
      uint32_t targetIndex = uint32_t(target.index);
      auto* targetBvh = &m_bvhData[targetIndex];

      while (targetBvh->childCount == targetBvh->childNodes.size()) {
        // Create a new chained node if necessary. Ignore all the bounding
        // volume parameters, just ensure that the node index is valid.
        if (targetBvh->chainedBvh.type == GfxSceneNodeType::eBvh) {
          targetIndex = uint32_t(targetBvh->chainedBvh.index);
        } else {
          GfxSceneBvhDesc bvhDesc = { };
          bvhDesc.nodeIndex = targetBvh->nodeIndex;

          targetIndex = uint32_t(createBvhNode(bvhDesc).index);
          targetBvh->chainedBvh = GfxSceneNodeRef(GfxSceneNodeType::eBvh, targetIndex);
        }

        targetBvh = &m_bvhData[targetIndex];
      }

      // Assign parent node and child intex to the new child node, and
      // write the node index to the list of child nodes of the BVH.
      nodeHostData.parentBvhNode = GfxSceneNodeRef(GfxSceneNodeType::eBvh, targetIndex);
      nodeHostData.childIndex = targetBvh->childCount++;

      targetBvh->childNodes[nodeHostData.childIndex] = nodeIndex;

      if (!std::exchange(targetMarkedDirty, true))
        markDirty(targetBvh->nodeIndex, GfxSceneNodeDirtyFlag::eDirtyBvhChain);
    }
  }

  // Recursively recompute the maximum BVH depth
  while (target.type == GfxSceneNodeType::eBvh) {
    auto& targetBvh = m_bvhData[uint32_t(target.index)];
    auto& targetHostData = m_hostData[targetBvh.nodeIndex];

    if (childDepth <= targetHostData.childDepth)
      break;

    targetHostData.childDepth = childDepth++;
    target = targetHostData.parentBvhNode;
  }
}


void GfxSceneNodeManager::commitUpdates(
  const GfxContext&                   context,
  const GfxScenePipelines&            pipelines,
        uint32_t                      currFrameId,
        uint32_t                      lastFrameId) {
  cleanupGpuBuffers(lastFrameId);

  updateBufferData(context, pipelines, currFrameId);

  cleanupNodes(lastFrameId);
}


void GfxSceneNodeManager::traverseBvh(
  const GfxContext&                   context,
  const GfxScenePipelines&            pipelines,
        uint32_t                      groupCount,
  const GfxScenePassGroupInfo*        groupInfos,
        uint32_t                      frameId,
        uint32_t                      referencePass) {
  context->beginDebugLabel("Traverse scene BVH", 0xff64c0ff);

  uint64_t sceneBufferVa = m_gpuResources.getGpuAddress();

  // Find upper bound for BVH depth for each individual
  // group, and then the maximum value across all groups.
  small_vector<uint32_t, 8> bvhDepth(groupCount);
  uint32_t maxDepth = 0u;

  for (uint32_t i = 0; i < groupCount; i++) {
    const auto& groupInfo = groupInfos[i];

    uint32_t groupDepth = 0u;

    for (uint32_t j = 0; j < groupInfo.rootNodeCount; j++)
      groupDepth = std::max(groupDepth, m_hostData[getNodeIndex(groupInfo.rootNodes[j])].childDepth);

    bvhDepth[i] = groupDepth;
    maxDepth = std::max(maxDepth, groupDepth);
  }

  // Prepare the pass buffers for the first traversal iteration
  context->beginDebugLabel("Initialization", 0xffa0e0ff);

  for (uint32_t i = 0; i < groupCount; i++) {
    const auto& groupInfo = groupInfos[i];

    GfxScenePassInitArgs initArgs = { };
    initArgs.sceneBufferVa = sceneBufferVa;
    initArgs.groupBufferVa = groupInfo.groupBuffer->getGpuAddress();
    initArgs.nodeCount = groupInfo.rootNodeCount;
    initArgs.frameId = frameId;

    pipelines.initPassGroupBuffer(context, initArgs, groupInfo.rootNodes);
  }

  context->endDebugLabel();

  // Process nodes of each BVH layer
  for (uint32_t i = 0; i <= maxDepth; i++) {
    context->beginDebugLabel(strcat("Layer ", i).c_str(), 0xffa0e0ff);

    context->memoryBarrier(
      GfxUsage::eShaderStorage | GfxUsage::eShaderResource | GfxUsage::eParameterBuffer, GfxShaderStage::eCompute,
      GfxUsage::eShaderStorage | GfxUsage::eShaderResource | GfxUsage::eParameterBuffer, GfxShaderStage::eCompute);

    for (uint32_t j = 0; j < groupCount; j++) {
      if (i <= bvhDepth[j]) {
        const auto& groupInfo = groupInfos[j];

        GfxSceneTraverseBvhArgs traverseArgs = { };
        traverseArgs.passBufferVa = groupInfo.passBufferVa;
        traverseArgs.sceneBufferVa = sceneBufferVa;
        traverseArgs.groupBufferVa = groupInfo.groupBuffer->getGpuAddress();
        traverseArgs.frameId = frameId;
        traverseArgs.bvhLayer = i;
        traverseArgs.distanceCullingPass = uint16_t(referencePass);

        pipelines.processBvhLayer(context,
          groupInfo.groupBuffer->getBvhDispatchDescriptor(i, true),
          groupInfo.groupBuffer->getBvhDispatchDescriptor(i, false),
          traverseArgs);
      }
    }

    context->endDebugLabel();
  }

  context->memoryBarrier(
    GfxUsage::eShaderStorage | GfxUsage::eShaderResource | GfxUsage::eParameterBuffer, GfxShaderStage::eCompute,
    GfxUsage::eShaderStorage | GfxUsage::eShaderResource | GfxUsage::eParameterBuffer, GfxShaderStage::eCompute);

  context->endDebugLabel();
}


void GfxSceneNodeManager::markDirty(
        uint32_t                      index,
        GfxSceneNodeDirtyFlags        flags) {
  if (!m_hostData[index].dirtyFlags.set(flags))
    addDirtyNode(index);
}


void GfxSceneNodeManager::addDirtyNode(
        uint32_t                      index) {
  std::lock_guard lock(m_dirtyMutex);
  m_dirtyNodes.push_back(index);
}


void GfxSceneNodeManager::addDirtyBvhLocked(
        uint32_t                      index) {
  m_dirtyBvhs.push_back(index);
}


void GfxSceneNodeManager::updateBufferData(
  const GfxContext&                   context,
  const GfxScenePipelines&            pipelines,
        uint32_t                      frameId) {
  context->beginDebugLabel("Update nodes", 0xff96c096u);

  for (uint32_t node : m_dirtyNodes) {
    auto& hostData = m_hostData[node];

    GfxSceneNodeDirtyFlags dirtyFlags = hostData.dirtyFlags.exchange(0u);
    GfxSceneNodeDirtyFlags dirtyBvhFlags = GfxSceneNodeDirtyFlag::eDirtyBvhNode | GfxSceneNodeDirtyFlag::eDirtyBvhChain;

    if (dirtyFlags & dirtyBvhFlags) {
      uint32_t bvhIndex = uint32_t(m_nodeData[node].nodeRef.index);

      uint32_t childCount = 0;
      uint32_t chainCount = 0;

      addDirtyBvhLocked(bvhIndex);

      while (dirtyFlags & GfxSceneNodeDirtyFlag::eDirtyBvhChain) {
        auto& bvhNode = m_bvhData[bvhIndex];

        childCount += bvhNode.childCount;
        chainCount += 1;

        if (bvhNode.chainedBvh.type != GfxSceneNodeType::eBvh)
          break;

        bvhIndex = uint32_t(bvhNode.chainedBvh.index);
        addDirtyBvhLocked(bvhIndex);
      }

      if (chainCount > 1 && (childCount <= (chainCount - 1u) * GfxSceneBvhInfo::MaxChildCount))
        compactBvhChain(m_nodeData[node].nodeRef, frameId);
    }
  }

  // Update node and BVH node data
  resizeGpuBuffer(context, frameId);

  GfxSceneNodeHeader gpuHeader = m_gpuResources.getHeader();

  if (!m_dirtyNodes.empty()) {
    pipelines.updateSceneBuffer(context,
      m_gpuResources.getGpuAddress() + gpuHeader.nodeParameterOffset,
      m_dirtyNodes.size(),
      m_dirtyNodes.data(),
      m_nodeData);
  }

  if (!m_dirtyBvhs.empty()) {
    pipelines.updateSceneBuffer(context,
      m_gpuResources.getGpuAddress() + gpuHeader.bvhOffset,
      m_dirtyBvhs.size(),
      m_dirtyBvhs.data(),
      m_bvhData);
  }

  m_dirtyNodes.clear();
  m_dirtyBvhs.clear();

  context->endDebugLabel();
}


void GfxSceneNodeManager::cleanupNodes(
        uint32_t                      frameId) {
  auto range = m_freeNodeQueue.equal_range(frameId);

  for (auto i = range.first; i != range.second; i++) {
    uint32_t index = uint32_t(i->second.index);

    if (i->second.type == GfxSceneNodeType::eBvh) {
      m_bvhData.erase(index);

      m_bvhAllocator.free(index);
    } else {
      m_hostData.erase(index);
      m_nodeData.erase(index);

      m_nodeAllocator.free(index);
    }
  }

  m_freeNodeQueue.erase(range.first, range.second);
}


void GfxSceneNodeManager::cleanupGpuBuffers(
        uint32_t                      frameId) {
  m_gpuBuffers.erase(frameId);
}


void GfxSceneNodeManager::compactBvhChain(
        GfxSceneNodeRef                 bvh,
        uint32_t                        frameId) {
  // Gather all child nodes into a linear array
  small_vector<uint32_t, 128> childNodes;

  for (GfxSceneNodeRef ref = bvh; ref.type == GfxSceneNodeType::eBvh; ) {
    auto& bvhData = m_bvhData[uint32_t(ref.index)];

    for (uint32_t i = 0; i < bvhData.childCount; i++)
      childNodes.push_back(bvhData.childNodes[i]);

    ref = bvhData.chainedBvh;
  }

  // If the BVH node has children, sort the child nodes by node type
  // so that processing child nodes is more efficient on the GPU.
  if (childNodes.size() > GfxSceneBvhInfo::MaxChildCount) {
    std::sort(childNodes.begin(), childNodes.end(),
      [this] (uint32_t a, uint32_t b) {
        return uint8_t(m_nodeData[a].nodeRef.type) < uint8_t(m_nodeData[b].nodeRef.type);
      });
  }

  // Rewrite the child node list in the most compact way possible,
  // and discard any chained nodes that would end up with no children.
  uint32_t count = childNodes.size();
  uint32_t first = 0;

  while (bvh.type == GfxSceneNodeType::eBvh) {
    auto& bvhData = m_bvhData[uint32_t(bvh.index)];

    bvhData.childCount = std::min<uint32_t>(count - first, bvhData.childNodes.size());

    for (uint32_t i = 0; i < bvhData.childCount; i++) {
      uint32_t childNode = childNodes[first + i];
      bvhData.childNodes[i] = childNode;

      auto& childHostData = m_hostData[childNode];
      childHostData.parentBvhNode = bvh;
      childHostData.childIndex = i;
    }

    first += bvhData.childCount;

    if (first == count)
      break;

    bvh = bvhData.chainedBvh;
  }

  // Destroy all chained nodes that are no longer useful. This will
  // implicitly iterate over the chained nodes of the node to destroy.
  if (bvh.type == GfxSceneNodeType::eBvh) {
    auto& bvhData = m_bvhData[uint32_t(bvh.index)];
    destroyBvhNode(std::exchange(bvhData.chainedBvh, GfxSceneNodeRef()), frameId);
  }
}


void GfxSceneNodeManager::resizeGpuBuffer(
  const GfxContext&                   context,
        uint32_t                      frameId) {
  GfxSceneNodeBufferDesc desc;
  desc.nodeCount = m_nodeAllocator.getCount();
  desc.bvhCount = m_bvhAllocator.getCount();

  GfxBuffer oldBuffer = m_gpuResources.resizeBuffer(context, desc);

  if (oldBuffer)
    m_gpuBuffers.insert({ frameId, std::move(oldBuffer) });
}


void GfxSceneNodeManager::removeFromNodeMap(
        GfxSceneNodeRef               reference) {
  if (reference.type != GfxSceneNodeType::eNone)
    m_nodeMap[uint32_t(reference.type)].erase(uint32_t(reference.index));
}


void GfxSceneNodeManager::insertIntoNodeMap(
        GfxSceneNodeRef               reference,
        uint32_t                      index) {
  if (reference.type != GfxSceneNodeType::eNone)
    m_nodeMap[uint32_t(reference.type)].emplace(uint32_t(reference.index), index);
}

}
