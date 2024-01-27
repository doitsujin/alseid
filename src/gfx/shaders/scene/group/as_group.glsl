#ifndef AS_GROUP_H
#define AS_GROUP_H

#include "../as_scene.glsl"

#include "as_group_visibility.glsl"

// Convenience method to remap a group-local pass index to a global
// index using the lookup table from the pass group header.
uint32_t passGroupGetPassIndex(
        uint64_t                      passGroupVa,
        uint32_t                      index) {
  return PassGroupBuffer(passGroupVa).passIndices[index];
}


// Node list entry. Stores the node reference as well as visibility
// masks that influence culling and rendering behaviour for each pass.
struct PassGroupNodeListItem {
  uint32_t  nodeRef;
  uint32_t  bvhRef;
  uint32_t  visibilityMask;
  uint32_t  renderPassMask;
};


// Node list header. Simple structure containing indirect dispatch
// parameters, an actual node count, and an index into the flattened
// node list array.
struct PassGroupNodeListHeader {
  u32vec3   fullDispatch;
  u32vec3   localDispatch;
  uint32_t  localEntryIndex;
  uint32_t  localEntryCount;
};


// Computes number of local entries in the node list.
uint32_t nodeListGetTotalEntryCount(
  in    PassGroupNodeListHeader       listHeader) {
  return listHeader.localEntryIndex + listHeader.localEntryCount;
}


// Node list buffer type. Generated by the BVH traversal shader.
layout(buffer_reference, buffer_reference_align = 16, scalar)
buffer PassGroupNodeList {
  queuefamilycoherent
  PassGroupNodeListHeader header;
  PassGroupNodeListItem   items[];
};


// Update list header. Simple structure containing indirect dispatch
// parameters to process the entire update list in one dispatch.
struct PassGroupUpdateListHeader {
  u32vec3   dispatch;
  uint32_t  entryCount;
};


// Update list buffer type. Generated by shaders that iterate over
// each visible node of a given type if preprocessing is required.
layout(buffer_reference, buffer_reference_align = 16, scalar)
buffer PassGroupUpdateList {
  PassGroupUpdateListHeader header;
  uint32_t                  nodeRefs[];
};


// Initializes node list header with an entry count of zero and
// appropriate defaults for the indirect dispatch arguments.
void nodeListInit(
        uint64_t                        groupBuffer,
        uint32_t                        tid) {
  PassGroupBuffer group = PassGroupBuffer(groupBuffer);

  if (tid < (NODE_TYPE_COUNT - NODE_TYPE_BUILTIN_COUNT)) {
    PassTypedNodeListOffsets offsets = group.nodeListOffsets[tid];

    if (offsets.nodeList != 0u) {
      PassGroupNodeList list = PassGroupNodeList(groupBuffer + offsets.nodeList);
      list.header = PassGroupNodeListHeader(u32vec3(0u, 1u, 1u), u32vec3(0u, 1u, 1u), 0u, 0u);
    }

    if (offsets.updateList != 0u) {
      PassGroupUpdateList list = PassGroupUpdateList(groupBuffer + offsets.updateList);
      list.header = PassGroupUpdateListHeader(u32vec3(0u, 1u, 1u), 0u);
    }
  }
}


// Computes full dispatch parameters for a given local list based on
// the local parameters.
void nodeListFinalize(
        uint64_t                        groupBuffer,
        uint32_t                        tid) {
  PassGroupBuffer group = PassGroupBuffer(groupBuffer);

  if (tid < (NODE_TYPE_COUNT - NODE_TYPE_BUILTIN_COUNT)) {
    PassTypedNodeListOffsets offsets = group.nodeListOffsets[tid];

    if (offsets.nodeList != 0u) {
      PassGroupNodeList list = PassGroupNodeList(groupBuffer + offsets.nodeList);

      list.header.fullDispatch.x = asComputeWorkgroupCount1D(
        list.header.localEntryIndex + list.header.localEntryCount,
        PASS_GROUP_WORKGROUP_SIZE);
    }
  }
}


// Resets only the update list portion of the node list header
// so that subsequent passes can reuse it as necessary.
void nodeListResetUpdate(
        uint64_t                        groupBuffer,
        uint32_t                        tid) {
  PassGroupBuffer group = PassGroupBuffer(groupBuffer);

  if (tid < (NODE_TYPE_COUNT - NODE_TYPE_BUILTIN_COUNT)) {
    PassTypedNodeListOffsets offsets = group.nodeListOffsets[tid];

    if (offsets.updateList != 0u) {
      PassGroupUpdateList list = PassGroupUpdateList(groupBuffer + offsets.updateList);
      list.header = PassGroupUpdateListHeader(u32vec3(0u, 1u, 1u), 0u);
    }
  }
}


// Moves the local node range to the end of the full node range,
// so that subsequent passes will only process newly added nodes.
void nodeListResetLocal(
        uint64_t                        groupBuffer,
        uint32_t                        tid) {
  PassGroupBuffer group = PassGroupBuffer(groupBuffer);

  if (tid < (NODE_TYPE_COUNT - NODE_TYPE_BUILTIN_COUNT)) {
    PassTypedNodeListOffsets offsets = group.nodeListOffsets[tid];

    if (offsets.nodeList != 0u) {
      PassGroupNodeList list = PassGroupNodeList(groupBuffer + offsets.nodeList);

      list.header.localDispatch.x = 0u;
      list.header.localEntryIndex += list.header.localEntryCount;
      list.header.localEntryCount = 0u;
    }
  }
}


// Adds node entry to the list. Requries the header to be zero-initialized
// before adding the first item, and initializes the dispatch argument with
// the required workgroup count as the entrycount increases. The workgroup
// size must be a power of two.
//
// Must be executed within subgroup-uniform control flow.
void nodeListAddItem(
        uint64_t                        groupBuffer,
  in    PassGroupNodeListItem           item,
        uint32_t                        workgroupSize) {
  uint32_t nodeType = getNodeTypeFromRef(item.nodeRef);

  // Exit early if either all node types are invalid, or if
  // there is no node list for the given node types.
  PassGroupBuffer group = PassGroupBuffer(groupBuffer);
  uint32_t offset = 0u;

  if (nodeType >= NODE_TYPE_BUILTIN_COUNT)
    offset = group.nodeListOffsets[nodeType - NODE_TYPE_BUILTIN_COUNT].nodeList;

  if (subgroupAll(offset == 0u))
    return;

  // For each unique node type within a subgroup, compute the
  // number of nodes and pick an invoaction to do the atomic.
  uint32_t localFirst;
  uint32_t localCount;
  uint32_t localIndex;

  SUBGROUP_SCALARIZE(offset) {
    u32vec4 ballot = subgroupBallot(true);
    localCount = subgroupBallotBitCount(ballot);
    localIndex = subgroupBallotExclusiveBitCount(ballot);
    localFirst = subgroupBallotFindLSB(ballot);
  }

  // Add the node item to the given list. The atomic increment
  // will only be performed once per unique valid node type.
  PassGroupNodeList list = PassGroupNodeList(groupBuffer + offset);

  uint32_t entry = 0u;
  uint32_t first = list.header.localEntryIndex;

  if (offset != 0u && localIndex == 0u)
    entry = atomicAdd(list.header.localEntryCount, localCount);

  entry = subgroupShuffle(entry, localFirst) + localIndex;

  if (offset != 0u) {
    list.items[first + entry] = item;

    if ((entry % workgroupSize) == 0u) {
      // This generally only happens once per unique node type per
      // subgroup since the workgroup size is expected to be large,
      // so do not try to scalarize this any further.
      atomicMax(list.header.localDispatch.x, (entry / workgroupSize) + 1u);
    }
  }
}


// Enqueues a node for an update. This assumes that the node type
// and workgroup size are uniform within the workgroup.
//
// May be executed from non-uniform control flow.
void nodeListAddUpdate(
        uint64_t                        groupBuffer,
        uint32_t                        nodeRef,
        uint32_t                        payload,
        uint32_t                        workgroupSize) {
  // Exit early if the node type is not valid or does not have a list
  uint32_t nodeType = getNodeTypeFromRef(subgroupBroadcastFirst(nodeRef));

  if (nodeType < NODE_TYPE_BUILTIN_COUNT)
    return;

  PassGroupBuffer group = PassGroupBuffer(groupBuffer);
  uint32_t offset = group.nodeListOffsets[nodeType - NODE_TYPE_BUILTIN_COUNT].updateList;

  if (offset == 0u)
    return;

  // Add node reference to the given update list. Let the driver deal
  // with optimizing this atomic, the address is known to be uniform
  // but control flow may not be, so manual subgroup optimizations are
  // invalid and may result in undefined behaviour.
  PassGroupUpdateList list = PassGroupUpdateList(groupBuffer + offset);

  uint32_t entry = atomicAdd(list.header.entryCount, 1u);
  list.nodeRefs[entry] = bitfieldInsert(nodeRef, payload, 0, 8);

  // Update the dispatch workgroup count as necessary. The workgroup
  // size is expected to be small, so only do this from one thread.
  if ((entry % workgroupSize) == 0u) {
    uint32_t maxEntry = subgroupMax(entry);

    if (subgroupElect()) {
      uvec2 workgroupCount = asGetWorkgroupCount2D((maxEntry / workgroupSize) + 1u);

      atomicMax(list.header.dispatch.x, workgroupCount.x);
      atomicMax(list.header.dispatch.y, workgroupCount.y);
    }
  }
}


// Node list buffer type. Used as input when generating draw lists.
layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer PassGroupNodeListIn {
  PassGroupNodeListHeader header;
  PassGroupNodeListItem   items[];
};


// Update list buffer type. Used as input when updating nodes.
layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer PassGroupUpdateListIn {
  PassGroupUpdateListHeader header;
  uint32_t                  nodeRefs[];
};

PassGroupUpdateListIn getNodeTypeUpdateList(
        uint64_t                        groupBuffer,
        uint32_t                        nodeType) {
  PassGroupBuffer group = PassGroupBuffer(groupBuffer);

  return PassGroupUpdateListIn(groupBuffer +
    group.nodeListOffsets[nodeType - NODE_TYPE_BUILTIN_COUNT].updateList);
}

// BVH occlusion test data.
layout(buffer_reference, buffer_reference_align = 16, scalar)
buffer PassGroupBvhOcclusionTestBuffer {
  u32vec3     csDispatch;
  u32vec3     msDispatch;
  uint32_t    msNodeCount;
  uint32_t    nodeRefs[];
};

layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer PassGroupBvhOcclusionTestBufferIn {
  u32vec3     csDispatch;
  u32vec3     msDispatch;
  uint32_t    msNodeCount;
  uint32_t    nodeRefs[];
};


// Initializes all dispatch info for occlusion test processing.
void bvhOcclusionTestInit(uint64_t passGroupVa) {
  PassGroupBuffer groupBuffer = PassGroupBuffer(passGroupVa);

  PassGroupBvhOcclusionTestBuffer occlusionTest = PassGroupBvhOcclusionTestBuffer(passGroupVa + groupBuffer.bvhOcclusionOffset);
  occlusionTest.csDispatch = u32vec3(0u, 1u, 1u);
  occlusionTest.msDispatch = u32vec3(0u, 1u, 1u);
  occlusionTest.msNodeCount = 0u;
}


// Resets mesh shader dispatch info for occlusion test processing.
void bvhOcclusionTestReset(uint64_t passGroupVa) {
  PassGroupBuffer groupBuffer = PassGroupBuffer(passGroupVa);

  PassGroupBvhOcclusionTestBuffer occlusionTest = PassGroupBvhOcclusionTestBuffer(passGroupVa + groupBuffer.bvhOcclusionOffset);
  occlusionTest.msDispatch = u32vec3(0u, 1u, 1u);
  occlusionTest.msNodeCount = 0u;
}


// Adds BVH node reference to occlusion test list
void bvhOcclusionTestAddNode(uint64_t passGroupVa, uint32_t bvhRef) {
  PassGroupBuffer groupBuffer = PassGroupBuffer(passGroupVa);
  PassGroupBvhOcclusionTestBuffer occlusionTest = PassGroupBvhOcclusionTestBuffer(passGroupVa + groupBuffer.bvhOcclusionOffset);

  u32vec4 ballot = subgroupBallot(bvhRef != 0u);

  uint32_t entryCount = subgroupBallotBitCount(ballot);
  uint32_t entryIndex = subgroupBallotExclusiveBitCount(ballot);

  if (entryCount == 0u)
    return;

  uint32_t offset;

  if (subgroupElect())
    offset = atomicAdd(occlusionTest.msNodeCount, entryCount);

  offset = subgroupBroadcastFirst(offset);
  entryIndex += offset;

  if (bvhRef != 0u)
    occlusionTest.nodeRefs[entryIndex] = bvhRef;

  // Have the thread with the highest index update the dispatch
  if (gl_SubgroupInvocationID == subgroupBallotFindMSB(ballot)) {
    atomicMax(occlusionTest.msDispatch.x,
      (entryIndex / MS_OCCLUSION_BOX_COUNT) + 1u);
  }
}


// Initializes BVH list. Sets the entry count for the root layer to the
// number of root items, and initializes dispatch parameters as necessary.
void bvhListInit(
        PassGroupBvhList                list,
        uint32_t                        rootCount,
        uint32_t                        tid) {
  if (tid < 2u) {
    uint32_t entryCount = tid == 0u ? rootCount : 0u;

    list.header.args[tid] = PassGroupBvhListArgs(
      u32vec3(entryCount, 1u, 1u),
      u32vec3(0u, 1u, 1u),
      entryCount, 0u);
  }

  if (tid == 0u)
    list.header.totalNodeCount = rootCount;
}


// Adds an item to the BVH node list. This allocates a node for every BVH
// node passed to the function, and offsets the write index by the node
// index for the current BVH layer, so that a flattened array is generated.
// Does not affect the dispatch arguments in any way since those are written
// later by the traversal shader.
//
// Must be executed within subgroup-uniform control flow.
void bvhListAddItem(
        PassGroupBvhList                list,
        uint32_t                        bvhLayer,
  in    PassGroupBvhListItem            item) {
  uint32_t nodeType = getNodeTypeFromRef(item.nodeRef);

  u32vec4 ballot = subgroupBallot(nodeType == NODE_TYPE_BVH);
  uint32_t localCount = subgroupBallotBitCount(ballot);
  uint32_t localIndex = subgroupBallotExclusiveBitCount(ballot);

  if (localCount == 0u)
    return;

  uint32_t nextIndex = (bvhLayer & 1u) ^ 1u;
  uint32_t entry;

  if (subgroupElect())
    entry = atomicAdd(list.header.args[nextIndex].entryCount, localCount);

  entry  = subgroupBroadcastFirst(entry) + localIndex;
  entry += list.header.totalNodeCount;

  if (nodeType == NODE_TYPE_BVH)
    list.items[entry] = item;
}


// Computes final dispatch arguments for the next iteration of BVH traversal.
// Must only be called from a single thread at the end of the shader, after
// all prior writes have completed.
void bvhListCommitArgs(
        PassGroupBvhList                list,
        PassGroupBvhOcclusionTestBuffer occlusion,
        uint32_t                        bvhLayer) {
  uint32_t currIndex = bvhLayer & 1u;
  uint32_t remaining = atomicAdd(list.header.args[currIndex].entryCount, -1u,
    gl_ScopeQueueFamily, gl_StorageSemanticsBuffer, gl_SemanticsAcquireRelease) - 1u;

  if (remaining == 0u) {
    uint32_t nextIndex = currIndex ^ 1u;

    uint32_t entryCount = list.header.args[nextIndex].entryCount;
    uint32_t entryIndex = list.header.totalNodeCount;

    list.header.args[nextIndex].dispatchTraverse.x = entryCount;
    list.header.args[nextIndex].dispatchReset.x = entryCount == 0u ? 1u : 0u;
    list.header.args[nextIndex].entryIndex = entryIndex;

    list.header.totalNodeCount = entryIndex + entryCount;

    uint32_t csDispatchSize = asComputeWorkgroupCount1D(entryIndex + entryCount, CS_OCCLUSION_BOX_COUNT);
    occlusion.csDispatch = u32vec3(csDispatchSize, 1u, 1u);
  }
}


// Resets dispatch arguments for the next BVH traversal iteration.
// Must only be called from one single thread at the end of a shader.
void bvhListResetArgs(
        PassGroupBvhList                list,
        uint32_t                        bvhLayer) {
  uint32_t currIndex = bvhLayer & 1u;
  uint32_t nextIndex = currIndex ^ 1u;

  list.header.args[nextIndex].dispatchTraverse.x = 0u;
  list.header.args[nextIndex].dispatchReset.x = 1u;
}

#endif /* AS_GROUP_H */
