#ifndef AS_INSTANCE_H
#define AS_INSTANCE_H

#define INSTANCE_STATIC_BIT             (1u << 0)
#define INSTANCE_DEFORM_BIT             (1u << 1)
#define INSTANCE_ANIMATION_BIT          (1u << 2)
#define INSTANCE_NO_MOTION_VECTORS_BIT  (1u << 3)

struct InstanceJoint {
  Transform       transform;
  uint32_t        reserved;
};

struct InstanceNode {
  uint32_t        nodeIndex;
  uint32_t        flags;
  uint32_t        dirtyFrameId;
  uint32_t        updateFrameId;
  uint64_t        geometryBuffer;
  uint64_t        animationBuffer;
  uint64_t        propertyBuffer;
  uint64_t        reserved;
};

struct InstanceHeader {
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
  u32vec2         reserved;
  Aabb16          aabb;
};

struct InstanceDraw {
  uint16_t        materialIndex;
  uint16_t        meshIndex;
  uint16_t        meshInstanceIndex;
  uint16_t        meshInstanceCount;
  uint32_t        shadingParameterOffset;
  uint32_t        shadingParameterSize;
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
buffer InstanceJointBuffer {
  InstanceJoint   joints[];
};

layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer InstanceJointBufferIn {
  InstanceJoint   joints[];
};

layout(buffer_reference, buffer_reference_align = 16, scalar)
buffer InstanceWeightBuffer {
  int16_t         weights[];
};

layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer InstanceWeightBufferIn {
  int16_t         weights[];
};


// Loads an input joint from the instance buffer.
Transform instanceLoadRelativeJoint(
        uint64_t                      instanceVa,
        uint32_t                      jointSet,
        uint32_t                      jointIndex) {
  InstanceDataBufferIn instanceData = InstanceDataBufferIn(instanceVa);
  InstanceJointBufferIn jointBuffer = InstanceJointBufferIn(instanceVa + instanceData.header.jointRelativeOffset);
  return jointBuffer.joints[jointIndex + jointSet * instanceData.header.jointCount].transform;
}


// Stores a relative joint in the instance buffer.
void instanceStoreRelativeJoint(
        uint64_t                      instanceVa,
        uint32_t                      jointSet,
        uint32_t                      jointIndex,
  in    Transform                     transform) {
  InstanceDataBuffer instanceData = InstanceDataBuffer(instanceVa);
  InstanceJointBuffer jointBuffer = InstanceJointBuffer(instanceVa + instanceData.header.jointRelativeOffset);
  jointBuffer.joints[jointIndex + jointSet * instanceData.header.jointCount] = InstanceJoint(transform, 0);
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


// Stores an asbolute joint transform in the instance buffer.
// Indexing works the same way as it does for loading.
void instanceStoreJoint(
        uint64_t                      instanceVa,
        uint32_t                      jointSet,
        uint32_t                      jointIndex,
  in    Transform                     transform) {
  InstanceDataBuffer instanceData = InstanceDataBuffer(instanceVa);
  InstanceJointBuffer jointBuffer = InstanceJointBuffer(instanceVa + instanceData.header.jointAbsoluteOffset);
  jointBuffer.joints[jointIndex + jointSet * instanceData.header.jointCount] = InstanceJoint(transform, 0);
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


// Loads a morph target weight. The first two sets store weights
// for the current and previous frames, and the third set contains
// updated morph target weights.
void instanceStoreMorphTargetWeight(
        uint64_t                      instanceVa,
        uint32_t                      weightSet,
        uint32_t                      weightIndex,
        int16_t                       weightValue) {
  InstanceDataBuffer instanceData = InstanceDataBuffer(instanceVa);
  InstanceWeightBuffer weightBuffer = InstanceWeightBuffer(instanceVa + instanceData.header.weightOffset);
  weightBuffer.weights[weightIndex + weightSet * instanceData.header.weightCount] = weightValue;
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

#endif /* AS_INSTANCE_H */
