#ifndef AS_GROUP_VISIBILITY_H
#define AS_GROUP_VISIBILITY_H

// Number of BVH nodes to process in a single workgroup
#define CS_OCCLUSION_BOX_COUNT (64u)
#define MS_OCCLUSION_BOX_COUNT (10u)


#define BVH_DISPATCHED_CHILD_NODES_BIT (1u << 0)


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
  PassGroupBvhVisibility bvhs[];
};


layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer PassGroupBvhVisibilityBufferIn {
  PassGroupBvhVisibility bvhs[];
};


layout(buffer_reference, buffer_reference_align = 16, scalar)
buffer PassGroupBvhVisibilityBufferCoherent {
  queuefamilycoherent
  PassGroupBvhVisibility bvhs[];
};

#endif // AS_GROUP_VISIBILITY_H
