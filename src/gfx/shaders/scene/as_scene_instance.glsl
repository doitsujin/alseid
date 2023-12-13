#ifndef AS_SCENE_INSTANCE_H
#define AS_SCENE_INSTANCE_H

#define INSTANCE_FLAG_STATIC            (1u << 0)
#define INSTANCE_FLAG_DEFORM            (1u << 1)
#define INSTANCE_FLAG_NO_MOTION_VECTORS (1u << 2)

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
  uint64_t        propertyBuffer;
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
        uint64_t                      instanceBuffer,
        uint32_t                      jointIndex) {
  InstanceDataBufferIn instanceData = InstanceDataBufferIn(instanceBuffer);
  InstanceJointBufferIn jointBuffer = InstanceJointBufferIn(instanceBuffer + instanceData.header.jointRelativeOffset);
  return jointBuffer.joints[jointIndex].transform;
}


// Loads an absolute joint transform from the instance buffer.
// The joint set should be either 0 or 1, and will return the
// transform for the current or previous frame, respectively.
Transform instanceLoadJoint(
        uint64_t                      instanceBuffer,
        uint32_t                      jointSet,
        uint32_t                      jointIndex) {
  InstanceDataBufferIn instanceData = InstanceDataBufferIn(instanceBuffer);
  InstanceJointBufferIn jointBuffer = InstanceJointBufferIn(instanceBuffer + instanceData.header.jointAbsoluteOffset);
  return jointBuffer.joints[jointIndex + jointSet * instanceData.header.jointCount].transform;
}


// Stores an asbolute joint transform in the instance buffer.
// Indexing works the same way as it does for loading.
void instanceStoreJoint(
        uint64_t                      instanceBuffer,
        uint32_t                      jointSet,
        uint32_t                      jointIndex,
  in    Transform                     transform) {
  InstanceDataBuffer instanceData = InstanceDataBuffer(instanceBuffer);
  InstanceJointBuffer jointBuffer = InstanceJointBuffer(instanceBuffer + instanceData.header.jointAbsoluteOffset);
  jointBuffer.joints[jointIndex + jointSet * instanceData.header.jointCount] = InstanceJoint(transform, 0);
}


// Loads a morph target weight. The first two sets store weights
// for the current and previous frames, and the last set contains
// updated morph target weights.
int16_t instanceLoadMorphTargetWeight(
        uint64_t                      instanceBuffer,
        uint32_t                      weightSet,
        uint32_t                      weightIndex) {
  InstanceDataBufferIn instanceData = InstanceDataBufferIn(instanceBuffer);
  InstanceWeightBufferIn weightBuffer = InstanceWeightBufferIn(instanceBuffer + instanceData.header.weightOffset);
  return weightBuffer.weights[weightIndex + weightSet * instanceData.header.weightCount];
}


// Loads a morph target weight. The first two sets store weights
// for the current and previous frames, and the third set contains
// updated morph target weights.
void instanceStoreMorphTargetWeight(
        uint64_t                      instanceBuffer,
        uint32_t                      weightSet,
        uint32_t                      weightIndex,
        int16_t                       weightValue) {
  InstanceDataBuffer instanceData = InstanceDataBuffer(instanceBuffer);
  InstanceWeightBuffer weightBuffer = InstanceWeightBuffer(instanceBuffer + instanceData.header.weightOffset);
  weightBuffer.weights[weightIndex + weightSet * instanceData.header.weightCount] = weightValue;
}


// Stores bounding box for a given instance.
void instanceStoreAabb(
        uint64_t                      instanceBuffer,
  in    Aabb16                        aabb) {
  InstanceDataBuffer instanceData = InstanceDataBuffer(instanceBuffer);
  instanceData.header.aabb = aabb;
}

#endif /* AS_SCENE_INSTANCE_H */
