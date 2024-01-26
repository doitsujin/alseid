#ifndef AS_GROUP_VISIBILITY_H
#define AS_GROUP_VISIBILITY_H

#include "../as_scene.glsl"

#define PASS_GROUP_PASS_COUNT                     (32u)
#define PASS_GROUP_WORKGROUP_SIZE                 (128u)

#define BVH_DISPATCHED_CHILD_NODES_BIT (1u << 0)

// Number of BVH nodes to process in a single workgroup
#define CS_OCCLUSION_BOX_COUNT (64u)
#define MS_OCCLUSION_BOX_COUNT (10u)

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
  uint32_t  bvhOcclusionOffset;
  u32vec3   reserved;
  uint16_t  passIndices[PASS_GROUP_PASS_COUNT];
  PassTypedNodeListOffsets nodeListOffsets[];
};

layout(buffer_reference, buffer_reference_align = 16, scalar)
buffer PassGroupBufferOut {
  uint32_t  passCount;
  uint32_t  ignoreOcclusionTestMask;
  uint32_t  bvhListOffset;
  uint32_t  bvhVisibilityOffset;
  uint32_t  bvhOcclusionOffset;
  u32vec3   reserved;
  uint16_t  passIndices[PASS_GROUP_PASS_COUNT];
  PassTypedNodeListOffsets nodeListOffsets[];
};


// BVH visibility status.
struct PassGroupBvhVisibility {
  // Frame ID of when the visibility status has last been updated Used to
  // discard outdated occlusion test results.
  uint32_t updateFrameId;
  // BVH visibility flags. Stores whether child nodes have been dispatched
  // for traversal in the current frame. If the node is made visible with
  // the flag not set, it must be dispatched for traversal.
  uint32_t flags;
  // Visibility mask for the BVH node, including results from occlusion
  // testing. May be updated during a frame if occlusion tests succeed.
  uint32_t visibilityMask;
  // Bit mask of passes for which occlusion testing has to be performed
  // in the current frame.
  uint32_t occlusionTestMask;
};


// BVH visibility buffer type. Stores persistent visibility information
// for each BVH node within the scene for the given pass group.
layout(buffer_reference, buffer_reference_align = 16, scalar)
buffer PassGroupBvhVisibilityBuffer {
  queuefamilycoherent
  PassGroupBvhVisibility bvhs[];
};

layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer PassGroupBvhVisibilityBufferIn {
  PassGroupBvhVisibility bvhs[];
};


// BVH list dispatch arguments. Also stores the index at which the
// first BVH node for the current dispath is stored within the list.
struct PassGroupBvhListArgs {
  u32vec3   dispatchTraverse;
  u32vec3   dispatchReset;
  uint32_t  entryCount;
  uint32_t  entryIndex;
};


// BVH list header. Stores two sets  of dispatch arguments so that
// the traversal shader can consume one while producing the other.
struct PassGroupBvhListHeader {
  uint32_t                totalNodeCount;
  PassGroupBvhListArgs    args[2];
};


// BVH list item. Stores the BVH node reference and the visibility
// mask of the parent BVH node.
struct PassGroupBvhListItem {
  uint32_t                nodeRef;
  uint32_t                visibilityMask;
};


// BVH list buffer type.
layout(buffer_reference, buffer_reference_align = 16, scalar)
buffer PassGroupBvhList {
  queuefamilycoherent
  PassGroupBvhListHeader  header;
  PassGroupBvhListItem    items[];
};

layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer PassGroupBvhListIn {
  PassGroupBvhListHeader  header;
  PassGroupBvhListItem    items[];
};


// Convenience function to mark a BVH node as visible and append it
// to the traversal list as necessary. Can be used from any shader
// stage, but does not commit dispatch arguments or change the total
// node count in order to not mess with the current pass.
void bvhMarkVisible(
        PassGroupBvhList                list,
        PassGroupBvhVisibilityBuffer    visibilityBuffer,
  in    PassGroupBvhVisibility          visibility,
        uint32_t                        passBit,
        PassGroupBvhListItem            item) {
  // Set the pass bit in the visibility mask
  if (asTest(visibility.visibilityMask, passBit))
    return;

  uint32_t bvhIndex = getNodeIndexFromRef(item.nodeRef);
  atomicOr(visibilityBuffer.bvhs[bvhIndex].visibilityMask, passBit);

  // If the node has not been fully dispatched yet, set the flag now
  if (asTest(visibility.flags, BVH_DISPATCHED_CHILD_NODES_BIT))
    return;

  uint32_t flags = atomicOr(visibilityBuffer.bvhs[bvhIndex].flags, BVH_DISPATCHED_CHILD_NODES_BIT);

  // If the dispatch flag was previously not set, append
  // the node to the traversal list for the next pass.
  if (asTest(flags, BVH_DISPATCHED_CHILD_NODES_BIT))
    return;

  uint32_t index = list.header.args[0].entryIndex +
    atomicAdd(list.header.args[0].entryCount, 1u);

  // Abuse the node type bit here to denote that this BVH
  // node already was in the dispatch list once
  item.nodeRef = makeNodeRef(0u, bvhIndex);
  list.items[index] = item;
}

#endif // AS_GROUP_VISIBILITY_H
