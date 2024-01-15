#ifndef AS_GROUP_VISIBILITY_H
#define AS_GROUP_VISIBILITY_H

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


layout(buffer_reference, buffer_reference_align = 16, scalar)
buffer PassGroupBvhVisibilityBufferCoherent {
  queuefamilycoherent
  PassGroupBvhVisibility bvhs[];
};

#endif // AS_GROUP_VISIBILITY_H
