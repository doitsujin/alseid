#ifndef AS_INSTANCE_PAYLOAD_H
#define AS_INSTANCE_PAYLOAD_H

#include "../instance/as_instance.glsl"

#include "../as_scene.glsl"

// Task shader payload. Stores uniform information about the render
// pass, instance, and meshlets to render.
struct TsPayload {
  // Index into the instance node buffer
  uint32_t            instanceIndex;
  // Absolute render pass index, taken from the workgroup ID
  // in combination with the render pass mask and pass group.
  uint32_t            passIndex;
  // Draw index within the instance. Used to pull some data
  // such as the material parameter offset within the buffer.
  uint32_t            drawIndex;
  // Offset of joint indices relative to the geometry buffer.
  uint32_t            skinningDataOffset;

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
  uint32_t            meshletOffsets[TsWorkgroupSize];
  // Meshlet payloads. Encodes a 6-bit view mask in the lower bits,
  // and a 10-bit workgroup index that denotes the first workgroup
  // that can work on the meshlet at the given index.
  uint16_t            meshletPayloads[TsWorkgroupSize];
};

taskPayloadSharedEXT TsPayload tsPayload;

#endif // AS_INSTANCE_PAYLOAD_H
