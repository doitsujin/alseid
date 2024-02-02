#ifndef TS_INSTANCE_COMMON_H
#define TS_INSTANCE_COMMON_H

#include "../draw/as_draw.glsl"
#include "../draw/as_draw_list_search_tree_traverse.glsl"

#include "../group/as_group.glsl"

#include "../renderpass/as_renderpass.glsl"

#include "as_instance.glsl"
#include "as_instance_payload.glsl"

// Task shader invocation info. Pulls data from the draw properties
// and assigns semantic meanings to the various indices. Except for
// the meshlet index, all these values are uniform.
struct TsInvocationInfo {
  uint32_t    firstDraw;
  uint32_t    firstThread;
  uint64_t    threadDrawMask;
  uint64_t    meshletBuffer;
  uint32_t    instanceIndex;
  uint32_t    localMeshInstance;
  uint32_t    lodIndex;
  uint32_t    lodMeshletIndex;
  uint32_t    passIndex;
  uint32_t    drawIndex;
  bool        isValid;
};


// Helper function to reduce and broadcast a 64-bit thread
// mask across the entire workgroup.
shared u32vec2 tsThreadMaskShared;

uint64_t tsCombineThreadMask(
        uint32_t                      tid,
        uint64_t                      threadMask) {
  u32vec2 unpacked = unpackUint2x32(threadMask);

  unpacked.x = subgroupOr(unpacked.x);
  unpacked.y = subgroupOr(unpacked.y);

  if (!IsSingleSubgroup) {
    if (tid < 2u)
      tsThreadMaskShared[tid] =0u;

    barrier();

    if (subgroupElect()) {
      atomicOr(tsThreadMaskShared.x, unpacked.x);
      atomicOr(tsThreadMaskShared.y, unpacked.y);
    }

    barrier();

    unpacked = tsThreadMaskShared;
  }

  return packUint2x32(unpacked);
}


// Retrieves information for the current task shader informations from
// built-in variables as well as the draw info buffer.
TsInvocationInfo tsGetInvocationInfo(
        uint64_t                      drawListVa,
        uint64_t                      passGroupVa,
        uint32_t                      drawGroup) {
  uint32_t tid = gl_LocalInvocationIndex;
  DrawListBufferIn drawList = DrawListBufferIn(drawListVa);

  // Look up first draw info index for the global task shader thread ID
  uint32_t drawInfoIndex = drawList.drawGroups[drawGroup].drawIndex;
  uint32_t taskShaderThreadCount = drawList.drawGroups[drawGroup].taskShaderThreadCount;
  uint32_t layer0Offset = drawList.drawGroups[drawGroup].searchTreeLayerOffsets[0u];

  uint32_t lookupThreadId = TsWorkgroupSize *
    (gl_DrawID * DRAW_LIST_TS_WORKGROUPS_PER_DRAW + gl_WorkGroupID.x);

  CsSearchResult searchResult = csTraverseSearchTree(drawList, drawGroup, lookupThreadId);

  // Based on the search result, work out which draw to process for each
  // individual thread in the workgroup. This exploits the fact that we
  // will never dispatch task shader workgroups with more than 64 threads.
  DrawSearchTreeLayerIn layer0 = DrawSearchTreeLayerIn(drawListVa + layer0Offset);

  uint32_t threadCount = layer0.threadCount[searchResult.nextOffset + tid];

  if (tid == 0u)
    threadCount -= searchResult.nextThreadId;

  uint32_t firstThreadsForDraw = csWorkgroupInclusiveAdd(tid, threadCount);

  uint64_t threadMask = 0u;

  if (firstThreadsForDraw < TsWorkgroupSize)
    threadMask |= 1ul << firstThreadsForDraw;

  threadMask = tsCombineThreadMask(tid, threadMask);

  u32vec2 localDrawThread = csComputeLocalDrawThreadIndex(
    threadMask, searchResult.nextThreadId, tid);

  // Set up results
  TsInvocationInfo result;
  result.firstDraw = searchResult.nextOffset + drawInfoIndex;
  result.firstThread = searchResult.nextThreadId;
  result.threadDrawMask = threadMask;
  result.isValid = lookupThreadId + tid < taskShaderThreadCount;

  if (result.isValid) {
    DrawInstanceInfoBufferIn drawInfos = DrawInstanceInfoBufferIn(
      drawListVa + drawList.header.drawInfoOffset);

    DrawInstanceInfo draw = drawInfos.draws[result.firstDraw + localDrawThread.x];

    u32vec3 subIndices = csGetDrawSubIndicesForInvocation(draw, localDrawThread.y);

    result.meshletBuffer = draw.meshletBufferAddress;
    result.instanceIndex = csGetPackedInstanceIndexFromDraw(draw.instanceIndexAndLod);
    result.localMeshInstance = subIndices.y;
    result.lodIndex = csGetPackedLodFromDraw(draw.instanceIndexAndLod);
    result.lodMeshletIndex = draw.meshletIndex + subIndices.x;
    result.passIndex = passGroupGetPassIndex(passGroupVa,
      asFindIndexOfSetBitCooperative(draw.passMask, subIndices.z, PASS_GROUP_PASS_COUNT));
    result.drawIndex = draw.instanceDrawIndex;
  } else {
    // Default-initialize everything, but mark the invocation
    // as invalid. Any memory accesses must be skipped.
    result.meshletBuffer = 0ul;
    result.instanceIndex = 0u;
    result.localMeshInstance = 0u;
    result.lodIndex = 0u;
    result.lodMeshletIndex = 0u;
    result.passIndex = 0u;
    result.drawIndex = 0u;
  }

  return result;
}


// Initializes task payload from invocation info. 
void tsPayloadInit(
  in    TsInvocationInfo              invocationInfo) {
  if (gl_LocalInvocationIndex == 0u) {
    tsPayload.firstDraw = invocationInfo.firstDraw;
    tsPayload.firstThread = invocationInfo.firstThread;
    tsPayload.threadDrawMask = invocationInfo.threadDrawMask;
  }
}


// Adds a meshlet to the payload. This must be called once per
// thread in a workgroup, or the payload will be undefined.
void tsPayloadAddMeshlet(uint32_t tid, in MsMeshletPayload payload) {
  tsPayload.meshletPayloads[tid] = tsEncodeMeshletPayload(payload);
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

#endif // TS_INSTANCE_COMMON_H
