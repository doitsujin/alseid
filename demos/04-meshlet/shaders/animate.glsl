#version 460

#extension GL_GOOGLE_include_directive : enable

#define CS_MAIN csAnimateJointsMain

#include "as_include_head.glsl"

layout(push_constant)
uniform push_t {
  AnimationBufferRef animationBuffer;
  uint firstGroup;
  float timestamp;
} push;


layout(set = 0, binding = 0, scalar)
writeonly buffer JointTransformOut {
  AnimationJoint outJoints[];
};


layout(set = 0, binding = 1, scalar)
writeonly buffer MorphTargetWeightOut {
  float outWeights[];
};


layout(set = 0, binding = 2, scalar)
writeonly buffer feedback_t {
  uvec4       frog;
  vec4        debug;
};


struct CsContext {
  AnimationBufferRef  animation;
  uint                animationGroup;
  float               timestamp;
};


void csStoreOutputJoint(uint index, in Transform transform) {
  outJoints[index] = AnimationJoint(transform, 0.0f);
}


void csStoreOutputWeight(uint index, float weight) {
  outWeights[index] = weight;
}


CsContext csGetContext() {
  CsContext context;
  context.animation = push.animationBuffer;
  context.animationGroup = push.firstGroup + gl_GlobalInvocationID.y;
  context.timestamp = push.timestamp;
  return context;
}

#include "cs_joint_animation.glsl"
#include "as_include_tail.glsl"
