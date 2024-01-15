#ifndef AS_PASS_H
#define AS_PASS_H

#include "../../as_quaternion.glsl"

#include "../as_scene.glsl"

#define RENDER_PASS_IS_CUBE_MAP_BIT               (1u << 0)
#define RENDER_PASS_USES_MIRROR_PLANE_BIT         (1u << 1)
#define RENDER_PASS_USES_VIEWPORT_REGION_BIT      (1u << 2)

#define RENDER_PASS_ENABLE_LIGHTING_BIT           (1u << 6)

#define RENDER_PASS_PERFORM_OCCLUSION_TEST_BIT    (1u << 8)
#define RENDER_PASS_IGNORE_OCCLUSION_TEST_BIT     (1u << 9)

#define RENDER_PASS_KEEP_METADATA_BIT             (1u << 16)
#define RENDER_PASS_KEEP_PROJECTION_BIT           (1u << 17)
#define RENDER_PASS_KEEP_VIEW_TRANSFORM_BIT       (1u << 18)
#define RENDER_PASS_KEEP_MIRROR_PLANE_BIT         (1u << 19)
#define RENDER_PASS_KEEP_VIEWPORT_LAYER_INDEX_BIT (1u << 20)
#define RENDER_PASS_KEEP_VIEWPORT_REGION_BIT      (1u << 21)
#define RENDER_PASS_KEEP_VIEW_DISTANCE_BIT        (1u << 22)

#define RENDER_PASS_SPECIAL_FLAG_MASK             (RENDER_PASS_IGNORE_OCCLUSION_TEST_BIT)

// View space transform of a render pass. Padded by an extra
// dword for alignment, and to match host structures.
struct PassTransform {
  Transform     transform;
  uint32_t      reserved;
};


// Render pass info. 
struct PassInfo {
  uint32_t      flags;
  uint32_t      passTypeMask;
  uint32_t      dirtyFrameId;
  uint32_t      updateFrameId;
  Projection    projection;
  PassTransform relativeTransform;
  PassTransform currTransform;
  PassTransform prevTransform;
  f32vec4       relativeMirrorPlane;
  f32vec4       currMirrorPlane;
  f32vec4       prevMirrorPlane;
  int32_t       cameraNode;
  int32_t       cameraJoint;
  int32_t       mirrorNode;
  int32_t       mirrorJoint;
  float32_t     viewDistanceLimit;
  float32_t     lodDistanceScale;
  uint32_t      layerIndex;
  uint32_t      viewportIndex;
  u32vec2       viewportOffset;
  u32vec2       viewportExtent;
  ViewFrustum   frustum;
};


// Render pass info buffer. Stores all render pass properties,
// as well as global information that may apply to all passes.
layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer PassInfoBufferIn {
  vec4          cubeFaceRotations[6];
  PassInfo      passes[];
};


// Read-write variant of render pass info buffer.
layout(buffer_reference, buffer_reference_align = 16, scalar)
buffer PassInfoBuffer {
  vec4          cubeFaceRotations[6];
  PassInfo      passes[];
};

#endif /* AS_PASS_H */
