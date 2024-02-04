#ifndef AS_INSTANCE_PAYLOAD_H
#define AS_INSTANCE_PAYLOAD_H

#include "../instance/as_instance.glsl"

#include "../as_scene.glsl"

// Number of meshlet groups processed in a single mesh shader
// workgroup. Depends on the mesh shader primitive output count.
#define MeshletGroupsPerWorkgroup (MsWorkgroupSize / MESHLET_GROUP_SIZE)


// Per-meshlet payload structure, and helper functions to encode
// and decode the structure using a 32-bit integer.
struct MsMeshletPayload {
  uint32_t offset;
  uint32_t groups;
  uint32_t viewMask;
};


uint32_t tsEncodeMeshletPayload(in MsMeshletPayload p) {
  return p.viewMask | (p.groups << 6u) | (p.offset << 2u);
}


MsMeshletPayload msDecodeMeshletPayload(uint32_t p) {
  MsMeshletPayload result;
  result.offset = bitfieldExtract(p, 10, 22) << 8u;
  result.groups = bitfieldExtract(p, 6, 4);
  result.viewMask = bitfieldExtract(p, 0, 6);
  return result;
}


uint32_t msPayloadComputeWorkgroupCount(in MsMeshletPayload p) {
  return bitCount(p.viewMask) * asComputeWorkgroupCount1D(p.groups, MeshletGroupsPerWorkgroup);
}


// Task shader payload. Stores uniform information about the render
// pass, instance, and meshlets to render.
struct TsPayload {
  // Absolute draw index for the first thread of the task shader
  // workgroup. Serves as a base offset for draw index calculations.
  uint32_t            firstDraw;
  // Relative index of the first task shader thread into the first
  // draw. Relevant for all mesh shader workgroups processing the
  // first draw.
  uint32_t            firstThread;
  // Bit mask of task shader threads that begin a new draw.
  uint64_t            threadDrawMask;

  // Meshlet payloads. Stores a packed meshlet offset (22 bits, as a
  // multiple of 256), the number of primitive groups in the meshlet
  // (4 bits, biased by 1), and the view mask (6 bits).
  uint32_t            meshletPayloads[TsWorkgroupSize];
};

taskPayloadSharedEXT TsPayload tsPayload;


// Helper function to compute meshlet index, local instance index, and
// relative render pass index for a given thread or invocation, in that
// order.
u32vec3 csGetDrawSubIndicesForInvocation(
  in    DrawInstanceInfo              draw,
        uint32_t                      invocation) {
  uint32_t meshletsPerInstance = uint32_t(draw.meshletCount);
  uint32_t meshletsPerPass = uint32_t(draw.meshInstanceCount) * meshletsPerInstance;

  u32vec2 div1 = approxIdiv(invocation, meshletsPerPass);
  u32vec2 div2 = approxIdiv(div1.y, meshletsPerInstance);

  return u32vec3(div2.y, div2.x, div1.x);
}


// Helper function to compute local draw index and local thread
// index for the given invocation.
u32vec2 csComputeLocalDrawThreadIndex(
        uint64_t                      threadMask,
        uint32_t                      firstThread,
        uint32_t                      tid) {
  u32vec2 localMask = unpackUint2x32(threadMask & ((2ul << tid) - 1ul));

  uint32_t localDraw = uint32_t(
    bitCount(localMask.x) + bitCount(localMask.y));
  uint32_t localThread = firstThread + tid;

  if (localDraw != 0u) {
    uint32_t msb = localMask.y != 0u
      ? findMSB(localMask.y) + 32u
      : findMSB(localMask.x);

    localThread = tid - msb;
  }

  return u32vec2(localDraw, localThread);
}

#endif // AS_INSTANCE_PAYLOAD_H
