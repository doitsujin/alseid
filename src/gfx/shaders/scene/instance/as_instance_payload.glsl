#ifndef AS_INSTANCE_PAYLOAD_H
#define AS_INSTANCE_PAYLOAD_H

#include "../instance/as_instance.glsl"

#include "../as_scene.glsl"

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

  // List of meshlets. This encodes a byte offset to the meshlet
  // header within the meshlet buffer, in addition to extra data.
  uint32_t            meshletOffsets[TsWorkgroupSize];
  // Meshlet payloads. Encodes a 6-bit view mask in the lower bits,
  // and a 10-bit workgroup index that denotes the first workgroup
  // that can work on the meshlet at the given index.
  uint16_t            meshletPayloads[TsWorkgroupSize];
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
