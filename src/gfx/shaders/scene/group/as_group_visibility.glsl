#ifndef AS_GROUP_VISIBILITY_H
#define AS_GROUP_VISIBILITY_H

// Number of BVH nodes to process in a single workgroup
#define CS_OCCLUSION_BOX_COUNT (64u)
#define MS_OCCLUSION_BOX_COUNT (10u)


// BVH visibility status
struct PassGroupBvhVisibility {
  uint32_t  prevFrameId;
  uint32_t  testFailMask;
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
