#ifndef MS_RENDER_COMMON_H
#define MS_RENDER_COMMON_H

#include "ts_render_payload.glsl"

#include "../scene/as_pass.glsl"
#include "../scene/as_scene_draw.glsl"
#include "../scene/as_scene_instance.glsl"

// Let the backend decide the optimal workgroup
// size for us, depending on driver preferences.
layout(local_size_x_id = SPEC_CONST_ID_MESH_SHADER_WORKGROUP_SIZE) in;

layout(constant_id = SPEC_CONST_ID_MESH_SHADER_WORKGROUP_SIZE)
const uint MsWorkgroupSize = 128u;


// Flip face orientation. This is generally required for mirrored
// mesh instances in order to maintain a consistent front face.
#define MS_FLAG_FLIP_FACE               (1u << 0)

// Face culling flags. This does not take the front face into account,
// and instead only looks at the final primitive winding order.
#define MS_FLAG_CULL_FACE_CW            (1u << 1)
#define MS_FLAG_CULL_FACE_CCW           (1u << 2)
#define MS_FLAG_CULL_FACE_ANY           (MS_FLAG_CULL_FACE_CW | MS_FLAG_CULL_FACE_CW)

// If set, motion vectors are set to 0. Generally depends on the
// state of the current render pass.
#define MS_FLAG_NO_MOTION_VECTORS       (1u << 3)


// Decodes meshlet payload. Returns the offset of the meshlet header
// relative to the meshlet buffer in the first component, and the
// arbitrary payload in the second component.
uvec2 msDecodeMeshlet(uint32_t meshlet) {
  return meshlet & uvec2(0xfffffff0u, 0xfu);
}


// Mesh shader invocation info. All properties are uniform within
// the workgroup.
struct MsInvocationInfo {
  MeshInstance    meshInstance;
  InstanceNode    instanceNode;
  InstanceHeader  instanceInfo;
  uint32_t        frameId;
  uint32_t        passIndex;
  uint32_t        meshletPayload;
  uint64_t        meshletVa;
  uint64_t        skinningVa;
  uint64_t        materialParameterVa;
  uint64_t        instanceParameterVa;
};


// Helper function to retrieve shading info, using both the
// task shader payload and the instance node buffer as inputs.
MsInvocationInfo msGetInvocationInfo(
        uint64_t                      instanceNodeVa,
        uint32_t                      frameId) {
  MsInvocationInfo result;
  result.meshInstance = tsPayload.meshInstance;

  // Load instance node and instance properties from the buffer.
  result.instanceNode = InstanceNodeBufferIn(instanceNodeVa).nodes[tsPayload.instanceIndex];
  result.instanceInfo = InstanceDataBufferIn(result.instanceNode.propertyBuffer).header;

  // Copy some basic parameters
  result.frameId = frameId;
  result.passIndex = tsPayload.passIndex;

  // Decode meshlet payload and compute the address of the meshlet header.
  uvec2 meshletInfo = msDecodeMeshlet(tsPayload.meshlets[gl_WorkGroupID.x]);
  result.meshletPayload = meshletInfo.y;
  result.meshletVa = tsPayload.meshletBuffer + meshletInfo.x;

  // Compute address of joint indices used for skinning, if present.
  result.skinningVa = tsPayload.skinningDataOffset != 0u
    ? result.instanceNode.geometryBuffer + tsPayload.skinningDataOffset
    : uint64_t(0u);

  // Compute address of per-draw material properties, if present.
  result.materialParameterVa = tsPayload.shadingDataOffset != 0u
    ? result.instanceNode.propertyBuffer + tsPayload.shadingDataOffset
    : uint64_t(0u);

  // Compute address of common instance properties, if present.
  result.instanceParameterVa = result.instanceInfo.parameterOffset != 0u
    ? result.instanceNode.propertyBuffer + result.instanceInfo.parameterOffset
    : uint64_t(0u);

  return result;
}


// Convenience method to load node transforms from the task shader payload.
Transform msLoadNodeTransform(bool currFrame) {
  return tsPayload.transforms[currFrame ? 0u : 1u].absoluteTransform;
}

#endif // MS_COMMON_H
