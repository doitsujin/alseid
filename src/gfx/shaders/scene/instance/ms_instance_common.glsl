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


// Hard-coded output counts, since there's no way to do this
// at runtime without patching the shader and we currently do
// not have the means to do so.
layout(triangles,
  max_vertices    = 1,
  max_primitives  = 1) out;


// Disable uniform output if no per-primitive outputs are used.
#if !defined(FS_UNIFORM) && !defined(MS_EXPORT_LAYER) && !defined(MS_EXPORT_VIEWPORT)
  #define MS_NO_UNIFORM_OUTPUT 1
#endif


// Disable shading data entirely if we do not
// produce any data for a fragment shader
#ifndef MS_NO_SHADING_DATA
  #ifndef FS_INPUT
  #define MS_NO_SHADING_DATA 1
  #endif // FS_INPUT
#endif // MS_NO_SHADING_DATA


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


struct MsWorkgroupInfo {
  MsMeshletPayload  payload;
  uint32_t          payloadIndex;
  uint32_t          viewIndex;
  uint32_t          groupIndex;
};


// Scans task shader payload for the meshlet index and local
// view index to process in the current workgroup.
shared uint16_t msWorkgroupPayloadIndicesShared[TsWorkgroupSize];
shared uint32_t msWorkgroupPayloadIndexShared;
shared uint32_t msWorkgroupPayloadWorkgroupShared;

MsWorkgroupInfo msGetWorkgroupInfo() {
  uint32_t workgroupIndex = gl_WorkGroupID.x;

  MsWorkgroupInfo result = { };

  if (IsFullSubgroup && gl_SubgroupSize * 4u >= TsWorkgroupSize) {
    // Fast path where each subgroup finds out what to work on on its own.
    // We generally expect to run a single iteration on Nvidia and AMD in
    // wave64 mode, and two iterations on AMD in wave32 mode.
    uint32_t sid = gl_SubgroupInvocationID;
    uint32_t first = 0u;

    for (uint32_t i = 0u; i < TsWorkgroupSize; i += gl_SubgroupSize) {
      uint32_t index = i + sid;

      if (index < TsWorkgroupSize)
        result.payload = msDecodeMeshletPayload(tsPayload.meshletPayloads[index]);

      uint32_t count = msPayloadComputeWorkgroupCount(result.payload);
      uint32_t scan = subgroupInclusiveAdd(count) + first;

      if (subgroupAny(workgroupIndex < scan)) {
        uint32_t lsb = subgroupBallotFindLSB(subgroupBallot(workgroupIndex < scan));

        result.payloadIndex = i + lsb;
        result.payload.offset = subgroupBroadcast(result.payload.offset, lsb);
        result.payload.groups = subgroupBroadcast(result.payload.groups, lsb);
        result.payload.viewMask = subgroupBroadcast(result.payload.viewMask, lsb);

        workgroupIndex -= subgroupBroadcast(scan - count, lsb);
        break;
      }

      first = subgroupBroadcast(scan, gl_SubgroupSize - 1u);
    }
  } else {
    uint32_t tid = gl_LocalInvocationIndex;

    // Fallback path that goes entirely through LDS. Also consider the
    // possibility that the mesh shader may run with smaller workgroups
    // than the task shader.
    uint32_t first = 0u;
    uint32_t istep = min(TsWorkgroupSize, gl_WorkGroupSize.x);

    for (uint32_t i = 0u; i < TsWorkgroupSize; i += istep) {
      uint32_t index = i + tid;

      if (index < TsWorkgroupSize)
        result.payload = msDecodeMeshletPayload(tsPayload.meshletPayloads[index]);

      uint32_t count = msPayloadComputeWorkgroupCount(result.payload);

      if (tid < TsWorkgroupSize)
        msWorkgroupPayloadIndicesShared[tid] = uint16_t(count);

      barrier();

      for (uint32_t j = 1u; j < istep; j += j) {
        uint16_t read;

        if (tid >= j && tid < TsWorkgroupSize)
          read = msWorkgroupPayloadIndicesShared[tid] + msWorkgroupPayloadIndicesShared[tid - j];

        barrier();

        if (tid >= j && tid < TsWorkgroupSize)
          msWorkgroupPayloadIndicesShared[tid] = read;

        barrier();
      }

      uint32_t incl = msWorkgroupPayloadIndicesShared[min(tid, istep - 1u)] + first;
      uint32_t excl = incl - count;

      if (workgroupIndex >= excl && workgroupIndex < incl) {
        msWorkgroupPayloadIndexShared = index;
        msWorkgroupPayloadWorkgroupShared = excl;
      }

      first += msWorkgroupPayloadIndicesShared[istep - 1u];
      barrier();
    }

    result.payloadIndex = msWorkgroupPayloadIndexShared;
    result.payload = msDecodeMeshletPayload(tsPayload.meshletPayloads[result.payloadIndex]);

    workgroupIndex -= msWorkgroupPayloadWorkgroupShared;
  }

  // Compute group and view index from the local workgroup index
  u32vec2 local = approxIdiv(workgroupIndex, result.payload.groups);
  result.groupIndex = local.y * MeshletGroupsPerWorkgroup;
  result.viewIndex = asFindIndexOfSetBitCooperative(result.payload.viewMask, local.x, 6u);

  return result;
}


// Mesh shader invocation info. All properties are uniform within
// the workgroup.
struct MsInvocationInfo {
  InstanceNode    instanceNode;
  InstanceHeader  instanceInfo;
  MeshInstance    meshInstance;
  uint64_t        nodeTransformVa;
  u32vec2         nodeTransformIndices;
  uint32_t        frameId;
  uint32_t        passIndex;
  uint32_t        drawIndex;
  uint32_t        viewIndex;
  uint32_t        firstGroup;
  uint32_t        groupCount;
  uint64_t        meshletVa;
  uint64_t        skinningVa;
};


// Helper function to retrieve shading info, using both the
// task shader payload and the instance node buffer as inputs.
MsInvocationInfo msGetInvocationInfo(
        uint64_t                      drawListVa,
        uint64_t                      instanceNodeVa,
        uint64_t                      sceneVa,
        uint32_t                      frameId) {
  DrawListBufferIn drawList = DrawListBufferIn(drawListVa);
  DrawInstanceInfoBufferIn drawInfos = DrawInstanceInfoBufferIn(drawListVa + drawList.header.drawInfoOffset);

  SceneHeader scene = SceneHeaderIn(sceneVa).header;

  MsWorkgroupInfo workgroupInfo = msGetWorkgroupInfo();

  uint32_t meshletIndex = workgroupInfo.payloadIndex;

  u32vec2 localDrawThread = csComputeLocalDrawThreadIndex(
    tsPayload.threadDrawMask, tsPayload.firstThread, meshletIndex);

  uint32_t drawInfoIndex = tsPayload.firstDraw + localDrawThread.x;
  DrawInstanceInfo draw = drawInfos.draws[drawInfoIndex];

  u32vec3 drawIndices = csGetDrawSubIndicesForInvocation(draw, localDrawThread.y);

  MsInvocationInfo result;

  // Load instance node and instance properties from the buffer.
  uint32_t instanceIndex = csGetPackedInstanceIndexFromDraw(draw.instanceIndexAndLod);

  result.instanceNode = InstanceNodeBufferIn(instanceNodeVa).nodes[instanceIndex];
  result.instanceInfo = InstanceDataBufferIn(result.instanceNode.propertyBuffer).header;
  result.nodeTransformVa = sceneVa + scene.nodeTransformOffset;
  result.nodeTransformIndices = nodeComputeTransformIndices(
    result.instanceNode.nodeIndex, scene.nodeCount, frameId);

  GeometryRef geometry = GeometryRef(result.instanceInfo.geometryVa);
  uint32_t instanceDataOffset = geometry.meshes[draw.meshIndex].instanceDataOffset;
  uint32_t skinDataOffset = geometry.meshes[draw.meshIndex].skinDataOffset;

  // Load mesh instance data from geometry buffer
  result.meshInstance = initMeshInstance();

  if (instanceDataOffset != 0u) {
    MeshInstanceRef instances = MeshInstanceRef(result.instanceInfo.geometryVa + instanceDataOffset);
    result.meshInstance = instances.instances[drawIndices.y];
  }

  // Copy some basic parameters
  result.frameId = frameId;
  result.passIndex = drawIndices.z;
  result.drawIndex = draw.instanceDrawIndex;

  // Decode meshlet payload and compute the address of the meshlet header.
  result.viewIndex = workgroupInfo.viewIndex;
  result.firstGroup = workgroupInfo.groupIndex;
  result.groupCount = workgroupInfo.payload.groups;
  result.meshletVa = draw.meshletBufferAddress + workgroupInfo.payload.offset;

  // Compute address of joint indices used for skinning, if present.
  result.skinningVa = skinDataOffset != 0u
    ? result.instanceInfo.geometryVa + skinDataOffset
    : 0ul;

  return result;
}


// Draw parameters. Contains offsets to relevant parameter
// structures, relative to the instance property buffer.
struct MsDrawParameters {
  uint32_t shadingDataOffset;
  uint32_t materialDataOffset;
};


// Convenience method to load relevant local draw parameters
MsDrawParameters msGetDrawParameters(in MsInvocationInfo invocationInfo) {
  InstanceDrawBuffer drawBuffer = InstanceDrawBuffer(
    invocationInfo.instanceNode.propertyBuffer +
    invocationInfo.instanceInfo.drawOffset);

  InstanceDraw draw = drawBuffer.draws[invocationInfo.drawIndex];

  MsDrawParameters result;
  result.shadingDataOffset = invocationInfo.instanceInfo.parameterOffset;
  result.materialDataOffset = draw.materialParameterOffset;
  return result;
}


// Input passed to the application-provided mesh shader functions
// to compute the vertex position and related outputs.
struct MsVertexPerFrame {
  MsVertexIn        vertexData;
  Transform         node;
#ifndef MS_NO_SKINNING
  Transform         joint;
#endif // MS_NO_SKINNING
};

struct MsVertexParameters {
  MsVertexPerFrame  currFrame;
#ifndef MS_NO_MOTION_VECTORS
  MsVertexPerFrame  prevFrame;
#endif // MS_NO_MOTION_VECTORS
};


// Input passed to the application-provided mesh shader functions
// to compute per-vertex fragment shader inputs.
#ifdef FS_INPUT

struct MsShadingParameters {
#ifndef MS_NO_VERTEX_DATA
#define MS_NO_DUMMY_INPUT
  MsVertexOut       vertexData;
#endif // MS_NO_VERTEX_DATA

#ifndef MS_NO_SHADING_DATA
#define MS_NO_DUMMY_INPUT
  MsShadingIn       shadingData;
#endif // MS_NO_SHADING_DATA

#ifndef MS_NO_UNIFORM_OUTPUT
#define MS_NO_DUMMY_INPUT
  MsUniformOut      uniformData;
#endif // MS_NO_UNIFORM_OUTPUT

#ifndef MS_NO_DUMMY_INPUT
  // Dummy input if nothing else was defined
  uint32_t          dummy;
#endif // MS_NO_DUMMY_INPUT
};

#endif // FS_INPUT

#endif // MS_INSTANCE_COMMON_H
