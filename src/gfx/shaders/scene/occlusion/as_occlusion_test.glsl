#ifndef AS_OCCLUSION_TEST_H
#define AS_OCCLUSION_TEST_H

#include "../group/as_group_visibility.glsl"

// Process enough bounding boxes per mesh shader workgroup to
// achieve reasonably high occupancy with 128 threads.
#define BOX_COUNT_PER_WORKGROUP (10u)

#if defined(STAGE_MESH) || defined(STAGE_TASK)
struct TsPayload {
  // Number of valid BVH indices
  uint32_t bvhCount;
  // BVH indices to render bounding boxes for.
  uint32_t bvhIndices[TsWorkgroupSize];
};

taskPayloadSharedEXT TsPayload tsPayload;
#endif


// Shader arguments, mostly containing buffer pointers.
// Not used in the fragment shader, instead we'll pass
// the resolved buffer pointer in from the mesh shader.
#ifndef STAGE_FRAG
layout(push_constant)
uniform PushData {
  uint64_t passInfoVa;
  uint64_t passGroupVa;
  uint64_t sceneVa;
  uint32_t passIndex;
  uint32_t frameId;
} globals;
#endif // STAGE_FRAG


#ifndef STAGE_TASK
#define FS_UNIFORM                                                    \
  FS_INPUT_VAR((location = 0, component = 0), uint64_t, bvhBufferVa)  \
  FS_INPUT_VAR((location = 0, component = 2), uint32_t, bvhIndex)     \
  FS_INPUT_VAR((location = 0, component = 3), uint32_t, passIndex)

FS_DECLARE_UNIFORM(FS_UNIFORM);
#endif // STAGE_TAST

#endif // AS_OCCLUSION_TEST_H