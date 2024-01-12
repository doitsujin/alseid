#ifndef AS_ANIMATION_H
#define AS_ANIMATION_H

// High-level keyframe data structure.
struct Keyframe {
  // Time stamp for the current key frame, in no particular unit.
  float timestamp;
  // Index of keyframe data. If nextCount is 0, this node is a leaf node
  // and the index points into the joint and weight data arrays for the
  // current animation group. Otherwise, the index points to an array of
  // nextCount child nodes which the workgroup should process next.
  uint nextIndex;
  // Number of child nodes, or 0 if this is a leaf node.
  uint nextCount;
};


// Packed keyframe data. Compact representation of a keyframe
// as it is stored in the animation buffer, see above.
struct KeyframePacked {
  float timestamp;
  uint32_t nextIndexAndCount;
};


// Unpacks keyframe data
Keyframe keyframeUnpack(in KeyframePacked data) {
  Keyframe result;
  result.timestamp = data.timestamp;
  result.nextIndex = bitfieldExtract(data.nextIndexAndCount,  0, 24);
  result.nextCount = bitfieldExtract(data.nextIndexAndCount, 24,  8);
  return result;
}


// Buffer reference type for keyframe data
layout(buffer_reference, buffer_reference_align = 8, scalar)
readonly buffer KeyframeRef {
  KeyframePacked keyframes[];
};


// Animation joint data. Stores a relative transform, and is
// padded to a size of 32 bytes for memory alignment reasons.
struct AnimationJoint {
  Transform transform;
  float     reserved;
};


// Buffer reference type for joint data
layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer AnimationJointRef {
  AnimationJoint joints[];
};


// Buffer reference type for morph target weights
layout(buffer_reference, buffer_reference_align = 4, scalar)
readonly buffer AnimationMorphTargetRef {
  float weights[];
};


// Animation group header. Describes an animation for up to 32 joints
// and morph targets using the same set of key frame time stamps. These
// limits are chosen to simplify shader-based animation processing.
//
// Interpolation between keyframes is linear. If the SLERP bit is set
// on the animation group, rotation quaternions must be interpolated
// using slerp, otherwise a regular linear interpolation with
// normalization is sufficient.
//
// Quaternions have to be decomposed into a scaling component and a
// normalized rotation component in order to correctly compute the
// result.
#define ANIMATION_GROUP_SLERP_BIT (1u << 0)

struct AnimationGroup {
  uint            flags;
  float           duration;
  uint            keyframeIndex;
  uint            keyframeCount;
  uint            morphTargetWeightIndex;
  uint            morphTargetCount;
  uint            jointTransformIndex;
  uint            jointCount;
  uint16_t        jointIndices[32];
  uint8_t         morphTargetIndices[32];
};


// Animation buffer header. Stores the data layout of the animation
// buffer, and is immediately followed by an array of animation groups.
struct AnimationBuffer {
  uint      groupCount;
  uint      keyframeDataOffset;
  uint      jointDataOffset;
  uint      weightDataOffset;
};


// Buffer reference type for animations
layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer AnimationBufferRef {
  AnimationBuffer header;
  AnimationGroup groups[];
};


KeyframeRef animationGetKeyframeBuffer(
        uint64_t                      animationVa,
  in    AnimationBuffer               info) {
  return KeyframeRef(animationVa + info.keyframeDataOffset);
}


AnimationJointRef animationGetJointBuffer(
        uint64_t                      animationVa,
  in    AnimationBuffer               info) {
  return AnimationJointRef(animationVa + info.jointDataOffset);
}


AnimationMorphTargetRef animationGetMorphTargetBuffer(
        uint64_t                      animationVa,
  in    AnimationBuffer               info) {
  return AnimationMorphTargetRef(animationVa + info.weightDataOffset);
}

#endif //AS_ANIMATION_H
