#ifndef AS_OCCLUSION_TEST_H
#define AS_OCCLUSION_TEST_H

#include "../group/as_group_visibility.glsl"

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


#define FS_UNIFORM                                                    \
  FS_INPUT_VAR((location = 0, component = 0), uint64_t, bvhBufferVa)  \
  FS_INPUT_VAR((location = 0, component = 2), uint32_t, bvhIndex)     \
  FS_INPUT_VAR((location = 0, component = 3), uint32_t, passIndex)

FS_DECLARE_UNIFORM(FS_UNIFORM);

#endif // AS_OCCLUSION_TEST_H
