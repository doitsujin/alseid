#ifndef AS_INSTANCE_H
#define AS_INSTANCE_H

#include "../../asset/as_asset_group.glsl"

#define INSTANCE_RESIDENT_BIT           (1u << 0)
#define INSTANCE_STATIC_BIT             (1u << 1)
#define INSTANCE_DEFORM_BIT             (1u << 2)
#define INSTANCE_ANIMATION_BIT          (1u << 3)
#define INSTANCE_NO_MOTION_VECTORS_BIT  (1u << 4)

#define INSTANCE_DIRTY_DEFORM_BIT       (1u << 24)
#define INSTANCE_DIRTY_ASSETS_BIT       (1u << 25)

#define INSTANCE_DIRTY_FLAGS            (INSTANCE_DIRTY_DEFORM_BIT | INSTANCE_DIRTY_ASSETS_BIT)
#define INSTANCE_DIRTY_SHIFT            (24)

struct InstanceJoint {
  Transform       transform;
  uint32_t        reserved;
};

struct InstanceNode {
  uint32_t        nodeIndex;
  uint32_t        flags;
  uint32_t        dirtyFrameId;
  uint32_t        updateFrameId;
  uint64_t        propertyBuffer;
  uint64_t        assetListBuffer;
};

struct InstanceHeader {
  uint64_t        geometryVa;
  uint32_t        parameterOffset;
  uint32_t        parameterSize;
  uint32_t        drawCount;
  uint32_t        drawOffset;
  uint32_t        jointCount;
  uint32_t        jointRelativeOffset;
  uint32_t        jointAbsoluteOffset;
  uint32_t        weightCount;
  uint32_t        weightOffset;
  uint32_t        animationCount;
  uint32_t        animationOffset;
  uint32_t        resourceCount;
  uint32_t        resourceOffset;
  uint32_t        indirectionCount;
  uint32_t        indirectionOffset;
  Aabb16          aabb;
};

struct InstanceDraw {
  uint16_t        materialIndex;
  uint16_t        meshIndex;
  uint16_t        meshInstanceIndex;
  uint16_t        meshInstanceCount;
  uint32_t        meshletCount;
  uint32_t        materialParameterOffset;
  uint32_t        materialParameterSize;
  uint32_t        resourceParameterOffset;
  uint32_t        resourceParameterSize;
  uint32_t        reserved;
};

#define AS_INSTANCE_MAX_ANIMATION_CHANNELS    (2u)

#define AS_INSTANCE_ANIMATION_BLEND_OP_NONE   (0u)
#define AS_INSTANCE_ANIMATION_BLEND_OP_CHAIN  (1u)
#define AS_INSTANCE_ANIMATION_BLEND_OP_SLERP  (2u)

struct InstanceAnimationHeader {
  uint32_t        animationCount;
};

struct InstanceAnimationParameters {
  uint8_t         blendOp;
  uint8_t         blendChannel;
  uint16_t        blendWeight;
  uint16_t        groupIndex;
  uint16_t        groupCount;
  float           timestamp;
};

layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer InstanceAnimationBufferIn {
  InstanceAnimationHeader     header;
  InstanceAnimationParameters params[];
};


layout(buffer_reference, buffer_reference_align = 16, scalar)
buffer InstanceNodeBuffer {
  InstanceNode    nodes[];
};

layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer InstanceNodeBufferIn {
  InstanceNode    nodes[];
};

layout(buffer_reference, buffer_reference_align = 16, scalar)
buffer InstanceDataBuffer {
  InstanceHeader  header;
};

layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer InstanceDataBufferIn {
  InstanceHeader  header;
};

layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer InstanceDrawBuffer {
  InstanceDraw    draws[];
};

layout(buffer_reference, buffer_reference_align = 16, scalar)
workgroupcoherent buffer InstanceJointBuffer {
  InstanceJoint   joints[];
};

layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer InstanceJointBufferIn {
  InstanceJoint   joints[];
};

layout(buffer_reference, buffer_reference_align = 16, scalar)
workgroupcoherent buffer InstanceWeightBuffer {
  int16_t         weights[];
};

layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer InstanceWeightBufferIn {
  int16_t         weights[];
};


// Loads draw parameters
InstanceDraw instanceLoadDraw(
        uint64_t                      instanceVa,
        uint32_t                      drawIndex) {
  InstanceDataBufferIn instanceData = InstanceDataBufferIn(instanceVa);
  InstanceDrawBuffer drawBuffer = InstanceDrawBuffer(instanceVa + instanceData.header.drawOffset);
  return drawBuffer.draws[drawIndex];
}


// Loads an absolute joint transform from the instance buffer.
// The joint set should be either 0 or 1, and will return the
// transform for the current or previous frame, respectively.
Transform instanceLoadJoint(
        uint64_t                      instanceVa,
        uint32_t                      jointSet,
        uint32_t                      jointIndex) {
  InstanceDataBufferIn instanceData = InstanceDataBufferIn(instanceVa);
  InstanceJointBufferIn jointBuffer = InstanceJointBufferIn(instanceVa + instanceData.header.jointAbsoluteOffset);
  return jointBuffer.joints[jointIndex + jointSet * instanceData.header.jointCount].transform;
}


// Loads a morph target weight. The first two sets store weights
// for the current and previous frames, and the last set contains
// updated morph target weights.
int16_t instanceLoadMorphTargetWeight(
        uint64_t                      instanceVa,
        uint32_t                      weightSet,
        uint32_t                      weightIndex) {
  InstanceDataBufferIn instanceData = InstanceDataBufferIn(instanceVa);
  InstanceWeightBufferIn weightBuffer = InstanceWeightBufferIn(instanceVa + instanceData.header.weightOffset);
  return weightBuffer.weights[weightIndex + weightSet * instanceData.header.weightCount];
}


// Stores bounding box for a given instance.
void instanceStoreAabb(
        uint64_t                      instanceVa,
  in    Aabb16                        aabb) {
  InstanceDataBuffer instanceData = InstanceDataBuffer(instanceVa);
  instanceData.header.aabb = aabb;
}


// Retrieves instance animation property buffer
InstanceAnimationBufferIn instanceGetAnimationProperties(
        uint64_t                      instanceVa) {
  InstanceDataBuffer instanceData = InstanceDataBuffer(instanceVa);
  return InstanceAnimationBufferIn(instanceVa + instanceData.header.animationOffset);
}


// Extracts the resource count and indirection count
// from the packed 32-bit integer field.
uvec2 instanceGetResourceCount(uint32_t packed) {
  return uvec2(
    bitfieldExtract(packed,  0, 16),
    bitfieldExtract(packed, 16, 16));
}


// Resource entry buffer reference type. The least significant bit
// in the x component of each entry signifies whether the entry is
// an asset list index or a plain descriptor index / address.
layout(buffer_reference, buffer_reference_align = 8, scalar)
readonly buffer InstanceResourceBufferIn {
  uvec2 entries[];
};


// Resource indirection type. Stores information on where to
// copy a descriptor index or buffer address.
#define INSTANCE_RESOURCE_TYPE_DESCRIPTOR_INDEX   (0u)
#define INSTANCE_RESOURCE_TYPE_BUFFER_ADDRESS     (1u)

#define INSTANCE_RESOURCE_OPTIONAL_BIT            (1u << 0u)

struct InstanceResourceIndirection {
  uint8_t  type;
  uint8_t  flags;
  uint16_t srcEntry;
  uint32_t dstOffset;
};


layout(buffer_reference, buffer_reference_align = 8, scalar)
readonly buffer InstanceResourceIndirectionBufferIn {
  InstanceResourceIndirection entries[];
};


layout(buffer_reference, buffer_reference_align = 16, scalar)
writeonly buffer InstanceRawBufferOut32 {
  uint32_t data[];
};


layout(buffer_reference, buffer_reference_align = 16, scalar)
writeonly buffer InstanceRawBufferOut64 {
  u32vec2 data[];
};

#endif /* AS_INSTANCE_H */
