#ifndef MS_INSTANCE_COMMON_H
#define MS_INSTANCE_COMMON_H

#include "../draw/as_draw.glsl"

#include "../group/as_group.glsl"

#include "../instance/as_instance.glsl"

#include "../renderpass/as_renderpass.glsl"

#include "as_instance_payload.glsl"

// Let the backend decide the optimal workgroup
// size for us, depending on driver preferences.
layout(local_size_x_id = SPEC_CONST_ID_MESH_SHADER_WORKGROUP_SIZE) in;

layout(constant_id = SPEC_CONST_ID_MESH_SHADER_WORKGROUP_SIZE)
const uint MsWorkgroupSize = 128u;


// Flip face orientation. This is generally required for mirrored
// mesh instances in order to maintain a consistent front face.
#define MS_FLIP_FACE_BIT                (1u << 0)

// Face culling flags. This does not take the front face into account,
// and instead only looks at the final primitive winding order.
#define MS_CULL_FACE_CW_BIT             (1u << 1)
#define MS_CULL_FACE_CCW_BIT            (1u << 2)
#define MS_CULL_FACE_ANY                (MS_CULL_FACE_CW_BIT | MS_CULL_FACE_CCW_BIT)

// If set, motion vectors are set to 0. Generally depends on the
// state of the current render pass.
#define MS_NO_MOTION_VECTORS_BIT        (1u << 3)


// Decodes meshlet payload stored in the task shader payload.
uvec2 msDecodeMeshletPayload(uint32_t payload) {
  return uvec2(
    bitfieldExtract(payload, 0, 6),   // View mask
    bitfieldExtract(payload, 6, 10)); // First workgroup
}


// Scans task shader payload for the meshlet offset and local
// view index to process in the current workgroup.
shared uint msWorkgroupPayloadIndexShared;

uvec2 msGetMeshletInfoForWorkgroup() {
  uint32_t workgroupIndex = gl_WorkGroupID.x;

  // The shared payload index will always be written by exactly
  // one invocation, there is no need to initialize it.
  MS_LOOP_WORKGROUP(i, TsWorkgroupSize, TsWorkgroupSize) {
    uvec2 payload = msDecodeMeshletPayload(tsPayload.meshletPayloads[i]);

    if (workgroupIndex >= payload.y &&
        workgroupIndex < payload.y + bitCount(payload.x))
      msWorkgroupPayloadIndexShared = i;
  }

  barrier();

  // Load meshlet offset and compute the local view index
  uint32_t payloadIndex = msWorkgroupPayloadIndexShared;
  uvec2 payload = msDecodeMeshletPayload(tsPayload.meshletPayloads[payloadIndex]);

  uint32_t viewIndex = asFindIndexOfSetBitCooperative(
    payload.x, workgroupIndex - payload.y, 6u);

  return uvec2(tsPayload.meshletOffsets[payloadIndex], viewIndex);
}


// Mesh shader invocation info. All properties are uniform within
// the workgroup.
struct MsInvocationInfo {
  MeshInstance    meshInstance;
  InstanceNode    instanceNode;
  InstanceHeader  instanceInfo;
  uint32_t        frameId;
  uint32_t        passIndex;
  uint32_t        viewIndex;
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
  uvec2 meshletInfo = msGetMeshletInfoForWorkgroup();
  result.viewIndex = meshletInfo.y;
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

#endif // MS_INSTANCE_COMMON_H
