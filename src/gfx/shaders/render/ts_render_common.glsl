#ifndef TS_RENDER_COMMON_H
#define TS_RENDER_COMMON_H

#include "../scene/as_scene_draw.glsl"
#include "../scene/as_scene_instance.glsl"

#include "ts_render_payload.glsl"

// Encodes meshlet data offset and a four-bit payload as a single
// 32-bit integer. This is possible because the buffer offset is
// aligned.
uint32_t tsEncodeMeshlet(uint32_t offset, uint32_t payload) {
  return offset | (payload & 0xfu);
}


// Task shader invocation info. Pulls data from the draw properties
// and assigns semantic meanings to the various indices. Except for
// the meshlet index, all these values are uniform.
struct TsInvocationInfo {
  uint32_t    instanceIndex;
  uint32_t    meshIndex;
  uint32_t    meshInstance;
  uint32_t    lodIndex;
  uint32_t    lodMeshletIndex;
  int32_t     passIndex;
  uint32_t    shadingDataOffset;
};


// Finds the index of the set bit within the pass mask that corresponds
// to the given invocation pass index.
int32_t tsComputePassIndex(uint32_t passMask, uint32_t passIndex) {
  uint32_t resultMask = 0u;

  // Use subgroup operations to quickly scan the pass mask. With a
  // subgroup size of 32 or more, only one iteration will be necessary.
  uint32_t bitsPerIteration = min(PASS_COUNT_PER_GROUP, gl_SubgroupSize);

  for (uint32_t i = 0; i < PASS_COUNT_PER_GROUP; i += bitsPerIteration) {
    int32_t bitIndex = int32_t(i + gl_SubgroupInvocationID);

    uint32_t passMaskLocal = bitfieldExtract(passMask, 0, bitIndex);
    uint32_t passCountLocal = bitCount(passMaskLocal);

    resultMask |= subgroupBallot(passCountLocal == passIndex).x << i;
  }

  return findLSB(resultMask);
}


// Retrieves information for the current task shader informations from
// built-in variables as well as the draw info buffer.
TsInvocationInfo tsGetInvocationInfo(uint64_t drawListVa, uint32_t drawGroup) {
  DrawListBufferIn drawList = DrawListBufferIn(drawListVa);
  DrawInstanceInfoBufferIn drawInfos = DrawInstanceInfoBufferIn(drawListVa + drawList.header.drawInfoOffset);

  uint32_t drawIndex = drawList.drawGroups[drawGroup].drawIndex + gl_DrawID;

  DrawInstanceInfo draw = drawInfos.draws[drawIndex];
  uvec2 unpacked = drawUnpackInstanceAndLod(draw.instanceAndLod);

  TsInvocationInfo result;
  result.instanceIndex = unpacked.x;
  result.meshIndex = draw.meshIndex;
  result.meshInstance = draw.meshInstance + gl_GlobalInvocationID.y;
  result.lodIndex = unpacked.y;
  result.lodMeshletIndex = gl_GlobalInvocationID.x;
  result.passIndex = tsComputePassIndex(draw.passMask, gl_GlobalInvocationID.z);
  result.shadingDataOffset = draw.shadingDataOffset;
  return result;
}


// Initializes task payload from invocation info. 
void tsPayloadInit(
  in    TsInvocationInfo              invocationInfo,
  in    Mesh                          mesh,
  in    MeshInstance                  meshInstance,
        uint64_t                      meshletBuffer) {
  if (gl_LocalInvocationIndex == 0u) {
    tsPayload.instanceIndex = invocationInfo.instanceIndex;
    tsPayload.passIndex = invocationInfo.passIndex;
    tsPayload.skinningDataOffset = mesh.skinDataOffset;
    tsPayload.shadingDataOffset = invocationInfo.shadingDataOffset;
    tsPayload.meshInstance = meshInstance;
    tsPayload.meshletBuffer = meshletBuffer;
  }
}


// Adds meshlet info to the payload. Indices must be pre-allocated and valid.
void tsPayloadAddMeshlet(uint32_t index, uint32_t offset, uint32_t payload) {
  tsPayload.meshlets[index] = tsEncodeMeshlet(offset, payload);
}


// Stores absolute node transform with the payload.
void tsPayloadSetTransform(uint32_t set, in Transform transform) {
  tsPayload.transforms[set] = SceneNodeTransform(transform, 0u);
}


// Pre-processed meshlet culling info. All coordinates are given in local
// mesh space, with mirroring already applied. The transform must be chained
// with a model to view space transform for culling purposes.
struct TsMeshletCullingInfo {
  uint32_t  flags;
  f32vec3   sphereCenter;
  float32_t sphereRadius;
  f32vec3   coneOrigin;
  f32vec3   coneAxis;
  float32_t coneCutoff;
  Transform transform;
};


// Loads local mesh instance data, if provided by the mesh.
// Otherwise, returns a default instance object.
MeshInstance tsLoadMeshInstance(
  in    GeometryRef                   geometry,
  in    Mesh                          mesh,
        uint                          instanceIndex) {
  if (mesh.instanceCount == 0)
    return initMeshInstance();

  MeshInstanceRef instances = meshGetInstanceData(geometry, mesh);
  return instances.instances[instanceIndex];
}

#endif // TS_PAYLOAD_H
