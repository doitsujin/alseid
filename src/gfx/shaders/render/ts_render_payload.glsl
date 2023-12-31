#ifndef TS_RENDER_PAYLOAD_H
#define TS_RENDER_PAYLOAD_H

#include "../scene/as_scene.glsl"
#include "../scene/as_scene_instance.glsl"

layout(constant_id = SPEC_CONST_ID_TASK_SHADER_WORKGROUP_SIZE)
const uint32_t TsWorkgroupSize = 128;

// Assume one single meshlet per thread, unless the task shader
// explicitly requests more, e.g. for shadow map rendering.
// Must be consistent with the mesh shader reading the payload.
#ifndef TS_MAX_MESHLET_COUNT
#define TS_MAX_MESHLET_COUNT (1u)
#endif // TS_MAX_MESHLET_COUNT


// Task shader payload. Stores uniform information about the render
// pass, instance, and meshlets to render.
struct TsPayload {
  // Index into the instance node buffer
  uint32_t            instanceIndex;
  // Absolute render pass index, taken from the workgroup ID
  // in combination with the render pass mask and pass group.
  uint32_t            passIndex;
  // Offset of joint indices relative to the geometry buffer.
  uint32_t            skinningDataOffset;
  // Offset of this draw's shading data relative to the the
  // instance property buffer.
  uint32_t            shadingDataOffset;

  // Mesh instance properties, read from the geometry buffer
  // based on the task shader workgroup ID.
  MeshInstance        meshInstance;

  // Node transform for the current frame (index 0) and the
  // previous frame (index 1). The task shader ensures that
  // both transforms are valid.
  SceneNodeTransform  transforms[2];

  // Meshlet data buffer for the selected LOD
  uint64_t            meshletBuffer;
  // List of meshlets. This encodes a byte offset to the meshlet
  // header within the meshlet buffer, in addition to extra data.
  uint32_t            meshlets[TS_MAX_MESHLET_COUNT * TsWorkgroupSize];
};

taskPayloadSharedEXT TsPayload tsPayload;

#endif // TS_PAYLOAD_H
