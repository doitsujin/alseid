// Joint animation compute shader.
//
// Given a set of animation groups, this will compute relative
// joint transforms and morph offset weights by interpolating
// between keyframe data that is stored in the geometry buffer.
//
//
// The interface is defined as follows:
//
// Context object that is passed through to various functions.
// This stores information required to fetch data from the
// correct places.
//
//    struct CsContext {
//      AnimationBufferRef  animation;      /* mandatory */
//      uint                animationGroup; /* mandatory */
//      float               timestamp;      /* mandatory */
//      ...
//    };


// Use a fixed workgroup size of 32 threads since the animation
// group data structures are built around that.
layout(local_size_x = 32) in;


// For the optimal subgroup path, we need not only to run with just
// a single subgroup but we also require that the subgroup is full,
// so that lane indices meaningfully map to a specific invocation.
bool IsSingleSubgroup = (gl_NumSubgroups == 1);
bool IsPackedSubgroup = (gl_NumSubgroups == 1 && gl_SubgroupSize == gl_WorkGroupSize.x);


// Keyframe pair. Stores the data indices of two keyframes to
// interpolate between, as well as the interpolation coefficient.
struct CsKeyframePair {
  uint  loIndex;
  uint  hiIndex;
  float t;
};


// Loads joint transform for a given keyframe.
Transform csLoadKeyframeJoint(
  in    AnimationJointRef             jointBuffer,
  in    AnimationGroup                group,
        uint                          keyframeIndex,
        uint                          localIndex) {
  uint absoluteIndex = group.jointTransformIndex + group.jointCount * keyframeIndex + localIndex;
  return jointBuffer.joints[absoluteIndex].transform;
}


// Interpolates joint transform between two keyframes.
Transform csInterpolateJoint(
  in    AnimationJointRef             jointBuffer,
  in    AnimationGroup                group,
  in    CsKeyframePair                keyframePair,
        uint                          localIndex) {
  Transform loTransform = csLoadKeyframeJoint(jointBuffer, group, keyframePair.loIndex, localIndex);
  Transform hiTransform = csLoadKeyframeJoint(jointBuffer, group, keyframePair.hiIndex, localIndex);

  // Decompose rotation quaternions into a rotation and scaling
  // component, so that we can interpolate each appropriately.
  float loScale = quatGetScale(loTransform.rot);
  float hiScale = quatGetScale(hiTransform.rot);

  loTransform.rot = normalize(loTransform.rot);
  hiTransform.rot = normalize(hiTransform.rot);

  // Interpolate rotation quaterion. Depending on animation
  // group parameters, we may have to use the expensive slerp.
  vec4 rotation;

  if ((group.flags & ANIMATION_GROUP_SLERP_BIT) != 0u)
    rotation = quatSlerp(loTransform.rot, hiTransform.rot, keyframePair.t);
  else
    rotation = quatNlerp(loTransform.rot, hiTransform.rot, keyframePair.t);

  Transform result;
  result.rot = rotation * sqrt(mix(loScale, hiScale, keyframePair.t));
  result.pos = mix(loTransform.pos, hiTransform.pos, keyframePair.t);
  return result;
}


// Loads morph target weight for a given keyframe.
float csLoadKeyframeMorphTargetWeight(
  in    AnimationMorphTargetRef       weightBuffer,
  in    AnimationGroup                group,
        uint                          keyframeIndex,
        uint                          localIndex) {
  uint absoluteIndex = group.morphTargetWeightIndex + group.morphTargetCount * keyframeIndex + localIndex;
  return weightBuffer.weights[absoluteIndex];
}


// Interpolates morph target weight between two keyframes.
float csInterpolateMorphTargetWeight(
  in    AnimationMorphTargetRef       weightBuffer,
  in    AnimationGroup                group,
  in    CsKeyframePair                keyframePair,
        uint                          localIndex) {
  return mix(
    csLoadKeyframeMorphTargetWeight(weightBuffer, group, keyframePair.loIndex, localIndex),
    csLoadKeyframeMorphTargetWeight(weightBuffer, group, keyframePair.hiIndex, localIndex),
    keyframePair.t);
}


// Helper function to read keyframe data from another thread.
// This is useful because subgroupShuffle for some reason only
// supports basic types, unlike subgroupBroadcast* functions.
KeyframePacked csShuffleKeyframe(
        KeyframePacked                keyframe,
        uint                          wantedIndex) {
  return KeyframePacked(
    subgroupShuffle(keyframe.timestamp,         wantedIndex),
    subgroupShuffle(keyframe.nextIndexAndCount, wantedIndex));
}


// Function to cooperatively find the correct pair of keyframes
// for the given timestamp. This works by cooperatively traversing
// a search tree, and therefore this function must be called from
// within uniform control flow and uniform arguments.
shared KeyframePacked csKeyframesShared[2];

shared uint csPickIndexShared;

CsKeyframePair csFindKeyframe(
  in    KeyframeRef                   keyframeBuffer,
  in    AnimationGroup                group,
        float                         timestamp) {
  // For the optimized subgroup path we want to map the subgroup
  // invocation index to a specific set of keyframe data
  uint localIndex = IsPackedSubgroup ? gl_SubgroupInvocationID : gl_LocalInvocationIndex;

  // The group keyframe data essentially serves as a root node
  // for the keyframe search tree.
  uint nextIndex = 0;
  uint nextCount = group.keyframeCount;

  Keyframe loFrame;
  Keyframe hiFrame;

  do {
    // Check whether the current 
    KeyframePacked keyframe;
    bool pick = false;

    if (localIndex < nextCount) {
      keyframe = keyframeBuffer.keyframes[group.keyframeIndex + nextIndex + localIndex];
      pick = timestamp >= keyframe.timestamp;
    }

    // Since keyframes are ordered, the number of threads passing
    // the test is equivalent to the second keyframe we need to
    // interpolate with. This is true for both code paths.
    uint pickIndex = subgroupBallotBitCount(subgroupBallot(pick));

    if (!IsSingleSubgroup) {
      if (gl_LocalInvocationIndex == 0)
        csPickIndexShared = 0u;

      barrier();

      if (subgroupElect() && pickIndex != 0)
        atomicAdd(csPickIndexShared, pickIndex);

      barrier();

      // We don't need a barrier after this read since the
      // keyframe broadcast code below will emit one anyway.
      pickIndex = csPickIndexShared;
    }

    // Ensure that we do not access invalid keyframes, and also avoid
    // loading the same keyframe twice in order to simplify some math.
    uint hiIndex = clamp(pickIndex, 1u, nextCount - 1u);
    uint loIndex = hiIndex - 1u;

    // Broadcast selected keyframes to the entire workgroup. On the
    // optimized subgroup path, we can do so simply by reading the
    // correct lanes.
    if (IsPackedSubgroup) {
      loFrame = keyframeUnpack(csShuffleKeyframe(keyframe, loIndex));
      hiFrame = keyframeUnpack(csShuffleKeyframe(keyframe, hiIndex));
    } else {
      uint sharedIndex = localIndex - loIndex;

      if (sharedIndex <= 1)
        csKeyframesShared[sharedIndex] = keyframe;

      barrier();

      loFrame = keyframeUnpack(csKeyframesShared[0]);
      hiFrame = keyframeUnpack(csKeyframesShared[1]);
      barrier();
    }

    // Prepare next iteration
    bool hi = timestamp >= hiFrame.timestamp;

    nextIndex = hi ? hiFrame.nextIndex : loFrame.nextIndex;
    nextCount = hi ? hiFrame.nextCount : loFrame.nextCount;
  } while (nextCount != 0u);

  // Found a leaf node, store the data indices and compute the
  // interpolation step value based on the keyframe timestamps.
  // The returned step value may not be in the [0.0, 1.0] range.
  CsKeyframePair result;
  result.loIndex = loFrame.nextIndex;
  result.hiIndex = hiFrame.nextIndex;
  result.t = clamp((timestamp - loFrame.timestamp) /
    (hiFrame.timestamp - loFrame.timestamp), 0.0f, 1.0f);
  return result;
}


void csAnimateJoints(in CsContext context) {
  AnimationBuffer animation = context.animation.header;
  AnimationGroup animationGroup = context.animation.groups[context.animationGroup];

  // Find the pair of keyframes to interpolate between
  KeyframeRef keyframeBuffer = animationGetKeyframeBuffer(
    context.animation, animation);

  CsKeyframePair keyframePair = csFindKeyframe(
    keyframeBuffer, animationGroup, context.timestamp);

  // No more subgroup shenanigans after this
  uint localIndex = gl_LocalInvocationIndex;

  // Process joints. The animation group stores a list of
  // absolute joint indices affected by this animation.
  AnimationJointRef jointBuffer =
    animationGetJointBuffer(context.animation, animation);

  if (localIndex < animationGroup.jointCount) {
    Transform jointTransform = csInterpolateJoint(
      jointBuffer, animationGroup, keyframePair, localIndex);

    uint jointIndex = uint(context.animation
      .groups[context.animationGroup]
      .jointIndices[localIndex]);

    csStoreOutputJoint(jointIndex, jointTransform);
  }

  // Process morph targets in the same manner.
  AnimationMorphTargetRef weightBuffer =
    animationGetMorphTargetBuffer(context.animation, animation);

  if (localIndex < animationGroup.morphTargetCount) {
    float weight = csInterpolateMorphTargetWeight(
      weightBuffer, animationGroup, keyframePair, localIndex);

    uint targetIndex = uint(context.animation
      .groups[context.animationGroup]
      .morphTargetIndices[localIndex]);

    csStoreOutputWeight(targetIndex, weight);
  }
}


// Entry point for this compute shader
void csAnimateJointsMain() {
  CsContext context = csGetContext();
  csAnimateJoints(context);
}
