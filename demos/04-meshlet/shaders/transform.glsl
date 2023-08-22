#version 460

#extension GL_GOOGLE_include_directive : enable

#define CS_MAIN csTransformJointsMain

#include "as_include_head.glsl"

layout(push_constant)
uniform push_t {
  GeometryRef geometryBuffer;
} push;


struct JointTransform {
  vec4 rot;
  vec4 pos;
};


layout(set = 0, binding = 0, scalar)
readonly buffer JointTransformIn {
  JointTransform inTransforms[];
};


layout(set = 0, binding = 1, scalar)
workgroupcoherent buffer JointTransformOut {
  JointTransform outTransforms[];
};


layout(set = 0, binding = 2, scalar)
writeonly buffer feedback_t {
  uvec4       frog;
  vec4        debug;
};


struct CsContext {
  GeometryRef geometry;
};


Transform csLoadInputJoint(in CsContext context, uint index) {
  return Transform(
    vec4(inTransforms[index].rot),
    vec3(inTransforms[index].pos));
}


Transform csLoadOutputJoint(in CsContext context, uint index) {
  return Transform(
    vec4(outTransforms[index].rot),
    vec3(outTransforms[index].pos));
}


void csStoreOutputJoint(in CsContext context, uint index, in Transform transform) {
  outTransforms[index] = JointTransform(
    vec4(transform.rot),
    vec4(transform.pos, 0.0f));
}


void csStoreBoundingVolume(in CsContext context, in Aabb aabb) {

}


CsContext csGetContext() {
  CsContext context;
  context.geometry = push.geometryBuffer;
  return context;
}

#include "cs_joint_transform.glsl"
#include "as_include_tail.glsl"
