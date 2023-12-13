#ifndef AS_PASS_H
#define AS_PASS_H

#define PASS_TYPE_FLAT    (0u)
#define PASS_TYPE_MIRROR  (1u)
#define PASS_TYPE_CUBE    (2u)

#define PASS_COUNT_PER_GROUP (32u)

// Render pass info
struct PassInfo {
  Projection  projection;
  Transform   viewTransform;
  uint32_t    passType;
  uint32_t    distanceCullingPass;
  float32_t   viewDistanceLimit;
  float32_t   viewDistanceScale;
  float32_t   lodDistanceScale;
  f32vec4     mirrorPlane;
  f32vec4     frustumOrFaceRotations[6];
};


// Render pass info buffer reference. Useful when access to pass
// infos is highly divergent, otherwise this should be passed in
// as a uniform buffer.
layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer PassInfoBuffer {
  PassInfo passes[];
};


// Pair of node list offsets within the pass group
struct PassTypedNodeListOffsets {
  uint32_t  nodeList;
  uint32_t  updateList;
};


// Pass group buffer type. Used as input for BVH traversal.
layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer PassGroupBuffer {
  uint32_t  passCount;
  uint32_t  ignoreOcclusionTestMask;
  uint32_t  bvhListOffset;
  uint32_t  bvhVisibilityOffset;
  uint16_t  passIndices[PASS_COUNT_PER_GROUP];
  PassTypedNodeListOffsets nodeListOffsets[];
};


// Node list entry. Stores the node reference as well as visibility
// masks that influence culling and rendering behaviour for each pass.
struct PassGroupNodeListItem {
  uint32_t  nodeRef;
  uint32_t  partialVisibilityMask;
  uint32_t  fullVisibilityMask;
};


// Node list header. Simple structure containing indirect dispatch
// parameters, an actual node count, and an index into the flattened
// node list array.
struct PassGroupNodeListHeader {
  u32vec3   dispatch;
  uint32_t  entryCount;
};


// Node list buffer type. Generated by the BVH traversal shader.
layout(buffer_reference, buffer_reference_align = 16, scalar)
buffer PassGroupNodeList {
  queuefamilycoherent
  PassGroupNodeListHeader header;
  PassGroupNodeListItem   items[];
};


// Update list buffer type. Generated by shaders that iterate over
// each visible node of a given type if preprocessing is required.
layout(buffer_reference, buffer_reference_align = 16, scalar)
buffer PassGroupUpdateList {
  PassGroupNodeListHeader header;
  uint32_t                nodeRefs[];
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
      list.header = PassGroupNodeListHeader(u32vec3(0u, 1u, 1u), 0u);
    }

    if (offsets.updateList != 0u) {
      PassGroupUpdateList list = PassGroupUpdateList(groupBuffer + offsets.updateList);
      list.header = PassGroupNodeListHeader(u32vec3(0u, 1u, 1u), 0u);
    }
  }
}


// Adds node entry to the list. Requries the header to be zero-initialized
// before adding the first item, and initializes the dispatch argument with
// the required workgroup count as the entrycount increases. The workgroup
// count must be a power of two.
void nodeListAddItem(
        uint64_t                        groupBuffer,
  in    PassGroupNodeListItem           item,
        uint32_t                        workgroupSize) {
  // Exit early if the node type is not valid
  uint32_t nodeType = getNodeTypeFromRef(item.nodeRef);

  if (nodeType < NODE_TYPE_BUILTIN_COUNT)
    return;

  // Exit early if there is no list for the given node type
  PassGroupBuffer group = PassGroupBuffer(groupBuffer);
  uint32_t offset = group.nodeListOffsets[nodeType - NODE_TYPE_BUILTIN_COUNT].nodeList;

  if (offset == 0u)
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

  // Add the node item to the given list
  PassGroupNodeList list = PassGroupNodeList(groupBuffer + offset);

  uint32_t entry;

  if (localIndex == 0u)
    entry = atomicAdd(list.header.entryCount, localCount);

  entry = subgroupShuffle(entry, localFirst) + localIndex;
  list.items[entry] = item;

  // Update the dispatch workgroup count as necessary
  if ((entry % workgroupSize) == 0u)
    atomicMax(list.header.dispatch.x, (entry / workgroupSize) + 1u);
}


// Enqueues a node for an update. This assumes that the node type
// and workgroup size are uniform within the workgroup.
void nodeListAddUpdate(
        uint64_t                        groupBuffer,
        uint32_t                        nodeRef,
        uint32_t                        workgroupSize) {
  // Exit early if the node type is not valid
  uint32_t nodeType = getNodeTypeFromRef(subgroupBroadcastFirst(nodeRef));

  if (nodeType < NODE_TYPE_BUILTIN_COUNT)
    return;

  PassGroupBuffer group = PassGroupBuffer(groupBuffer);
  uint32_t offset = group.nodeListOffsets[nodeType - NODE_TYPE_BUILTIN_COUNT].updateList;

  if (offset == 0u)
    return;

  // Add node reference to the given update list
  PassGroupUpdateList list = PassGroupUpdateList(groupBuffer + offset);

  u32vec4 ballot = subgroupBallot(true);
  uint32_t localCount = subgroupBallotBitCount(ballot);
  uint32_t localIndex = subgroupBallotExclusiveBitCount(ballot);

  uint32_t entry;

  if (subgroupElect())
    entry = atomicAdd(list.header.entryCount, localCount);

  entry = subgroupBroadcastFirst(entry) + localIndex;
  list.nodeRefs[entry] = nodeRef;

  // Update the dispatch workgroup count as necessary
  if ((entry % workgroupSize) == 0u)
    atomicMax(list.header.dispatch.x, (entry / workgroupSize) + 1u);
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
  PassGroupNodeListHeader header;
  uint32_t                nodeRefs[];
};

PassGroupUpdateListIn getNodeTypeUpdateList(
        uint64_t                        groupBuffer,
        uint32_t                        nodeType) {
  PassGroupBuffer group = PassGroupBuffer(groupBuffer);

  return PassGroupUpdateListIn(groupBuffer +
    group.nodeListOffsets[nodeType - NODE_TYPE_BUILTIN_COUNT].updateList);
}

// BVH list dispatch arguments. Also stores the index at which the
// first BVH node for the current dispath is stored within the list.
struct PassGroupBvhListArgs {
  u32vec3   dispatchTraverse;
  u32vec3   dispatchReset;
  uint32_t  entryCount;
  uint32_t  entryIndex;
};


// BVH list header. Stores two sets of dispatch arguments so that
// the traversal shader can consume one while producing the other.
struct PassGroupBvhListHeader {
  uint32_t  totalNodeCount;
  PassGroupBvhListArgs args[2];
};


// BVH list buffer type.
layout(buffer_reference, buffer_reference_align = 16, scalar)
buffer PassGroupBvhList {
  queuefamilycoherent
  PassGroupBvhListHeader  header;
  PassGroupNodeListItem   items[];
};


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


// Adds an item to the BVH node list. This allocates a node for every active
// invocation and offsets the write index by the node index for the current
// BVH layer, so that a flattened array is generated. Does not affect the
// dispatch arguments in any way since those are written later.
void bvhListAddItem(
        PassGroupBvhList                list,
        uint32_t                        bvhLayer,
  in    PassGroupNodeListItem           item) {
  uint32_t nextIndex = (bvhLayer & 1u) ^ 1u;

  u32vec4 ballot = subgroupBallot(true);
  uint32_t localCount = subgroupBallotBitCount(ballot);
  uint32_t localIndex = subgroupBallotExclusiveBitCount(ballot);

  uint32_t entry;

  if (subgroupElect())
    entry = atomicAdd(list.header.args[nextIndex].entryCount, localCount);

  entry  = subgroupBroadcastFirst(entry) + localIndex;
  entry += list.header.totalNodeCount;

  list.items[entry] = item;
}


// Computes final dispatch arguments for the next iteration of BVH traversal.
// Must only be called from a single thread at the end of the shader, after
// all prior writes have completed.
void bvhListCommitArgs(
        PassGroupBvhList                list,
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


// BVH list buffer type. Used as input when generating a tight
// list of BVH nodes to perform occlusion testing on.
layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer PassGroupBvhListIn {
  PassGroupBvhListHeader  header;
  PassGroupNodeListItem   items[];
};


// BVH visibility status
struct PassGroupBvhVisibility {
  uint32_t  prevFrameOcclusionTestPerformedMask;
  uint32_t  prevFrameOcclusionTestPassedMask;
};


// BVH visibility buffer type. Stores persistent visibility information
// for each BVH node within the scene for the given pass group.
layout(buffer_reference, buffer_reference_align = 16, scalar)
buffer PassGroupBvhVisibilityBuffer {
  PassGroupBvhVisibility bvhs[];
};


// BVH occlusion test buffer type. Used when generating a
// tight list of BVH nodes to perform occlusion testing on.
layout(buffer_reference, buffer_reference_align = 16, scalar)
buffer PassGroupBvhOcclusionTestBuffer {
  PassGroupNodeListHeader header;
  uint32_t                nodeRefs[];
};


// Adds a node to the occlusion test buffer. Assumes that the occlusion
// test mesh shader can process 10 bounding volumes per workgroup.
void bvhOcclusionTestListAddNode(
        PassGroupBvhOcclusionTestBuffer list,
        uint32_t                        nodeRef) {
  u32vec4 ballot = subgroupBallot(true);
  uint32_t localCount = subgroupBallotBitCount(ballot);
  uint32_t localIndex = subgroupBallotExclusiveBitCount(ballot);

  uint32_t entry;

  if (subgroupElect())
    entry = atomicAdd(list.header.entryCount, localCount);

  entry = subgroupBroadcastFirst(entry) + localIndex;

  if (entry == 0u)
    list.header.dispatch.yz = u32vec2(1u);

  uint32_t workgroupCount = entry / 10u;

  if ((entry - 10u * workgroupCount) == 0u)
    atomicMax(list.header.dispatch.x, workgroupCount + 1u);

  list.nodeRefs[entry] = nodeRef;
}


// BVH occlusion test buffer type. Used as input when
// drawing bounding boxes via a mesh shader.
layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer PassGroupBvhOcclusionTestBufferIn {
  PassGroupNodeListHeader header;
  uint32_t                nodeRefs[];
};

#endif /* AS_PASS_H */
