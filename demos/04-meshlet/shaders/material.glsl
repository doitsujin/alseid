#version 460

#extension GL_GOOGLE_include_directive : enable

// #define MS_NO_MORPH_DATA
// #define MS_NO_MOTION_VECTORS
// #define MS_NO_SKINNING

#include "as_include_head.glsl"

#include "scene/instance/as_instance.glsl"

// Use existing template for mesh/vertex shader
#define MS_MAIN msMain

// We implement the full FS here, but the app
// could provide its own set of FS templates
#define FS_MAIN fsMain

struct MsUniformOut {
  uvec2 instanceVa;
  uint drawIndex;
};


// Data structures shared between FS and VS/MS
#define FS_INPUT                                  \
  FS_INPUT_VAR((location = 0), vec3, normal)      \
  FS_INPUT_VAR((location = 1), vec2, texcoord)    \
  FS_INPUT_VAR((location = 2), vec3, currFramePos)\
  FS_INPUT_VAR((location = 3), vec3, prevFramePos)

FS_DECLARE_INPUT(FS_INPUT);

#define FS_UNIFORM                                               \
  FS_INPUT_VAR((location = 4, component = 0), uvec2, instanceVa) \
  FS_INPUT_VAR((location = 4, component = 2), uint, drawIndex)

FS_DECLARE_UNIFORM(FS_UNIFORM);


layout(push_constant)
uniform Globals {
  uint64_t          drawListVa;
  uint64_t          passInfoVa;
  uint64_t          passGroupVa;
  uint64_t          instanceVa;
  uint64_t          sceneVa;
  uint32_t          drawGroup;
  uint32_t          frameId;
} globals;


////////////////////////////
//    FRAGMENT SHADER     //
////////////////////////////
#ifdef STAGE_FRAG

layout(location = 0) out vec4 fsColor;

layout(buffer_reference, scalar)
readonly buffer ResourceArgs {
  uint32_t textureIndex;
  uint32_t samplerIndex;
};

layout(set = 0, binding = 0)
uniform sampler g_samplers[];

layout(set = 1, binding = 0)
uniform texture2D g_textures[];

void fsMain(in FsInput fsInput, in FsUniform fsUniform) {
  float factor = 0.5f + 0.5f * dot(normalize(fsInput.normal), vec3(0.0f, 1.0f, 0.0f));

  vec3 texColor = vec3(0.0f);

  SUBGROUP_SCALARIZE(fsUniform.instanceVa + uvec2(fsUniform.drawIndex, 0u)) {
    uint64_t instanceVa = packUint2x32(subgroupBroadcastFirst(fsUniform.instanceVa));
    InstanceDraw draw = instanceLoadDraw(instanceVa, subgroupBroadcastFirst(fsUniform.drawIndex));

    ResourceArgs resources = ResourceArgs(instanceVa + draw.resourceParameterOffset);
    texColor = texture(sampler2D(g_textures[resources.textureIndex], g_samplers[resources.samplerIndex]), fsInput.texcoord).xyz;
  }

  vec2 currPos = fsInput.currFramePos.xy / fsInput.currFramePos.z;
  vec2 prevPos = fsInput.prevFramePos.xy / fsInput.prevFramePos.z;

  vec2 motion = 0.25f + 0.25f * (50.0f * (currPos - prevPos));
  vec3 color = vec3(motion, 1.0f - dot(motion, motion)) + texColor;

  fsColor = vec4(color * factor, 1.0f);
}

#endif


////////////////////////////
//       MESH SHADER      //
////////////////////////////
#ifdef STAGE_MESH

struct MsVertexIn {
  f16vec4   position;
};


struct MsShadingIn {
  uint32_t  normal;
  uint32_t  texcoord;
};


struct MsMorphIn {
  f16vec4   position;
  uint32_t  normal;
};


struct MsVertexOut {
  vec4      position;
#ifndef MS_NO_MOTION_VECTORS
  vec3      oldPosition;
#endif // MS_NO_MOTION_VECTORS
  f16vec4   jointRotation;
};


#include "scene/instance/ms_instance_common.glsl"


struct MsContext {
  MsInvocationInfo  invocation;
  uint32_t          flags;
};


MsContext msGetInstanceContext() {
  MsContext context;
  context.invocation = msGetInvocationInfo(globals.drawListVa,
    globals.instanceVa, globals.sceneVa, globals.frameId);
  context.flags = MS_CULL_FACE_CW_BIT;

  if (asGetMirrorMode(context.invocation.meshInstance.extra) != MESH_MIRROR_NONE)
    context.flags |= MS_FLIP_FACE_BIT;

  if ((PassInfoBufferIn(globals.passInfoVa).passes[context.invocation.passIndex].flags & RENDER_PASS_IGNORE_OCCLUSION_TEST_BIT) != 0u)
    context.flags |= MS_NO_MOTION_VECTORS_BIT;

  return context;
}


void msMorphVertex(
  inout MsVertexIn                    vertex,
  in    MsMorphIn                     morph,
        float                         weight) {
  vertex.position += morph.position * float16_t(weight);
}


void msMorphShading(
  inout MsShadingIn                   shading,
  in    MsMorphIn                     morph,
        float                         weight) {
  shading.normal = packSnorm3x10(normalize(
    unpackSnorm3x10(shading.normal) +
    unpackSnorm3x10(morph.normal) * weight));
}


MsVertexOut msComputeVertexOutput(
  in    MsContext                     context,
  in    MsVertexParameters            args) {
  PassInfoBufferIn passInfoBuffer = PassInfoBufferIn(globals.passInfoVa);

  Transform currPassTransform = passInfoBuffer.passes[context.invocation.passIndex].currTransform.transform;

  Transform worldTransform = Transform(
    vec4(context.invocation.meshInstance.transform),
    vec3(context.invocation.meshInstance.translate));

#ifndef MS_NO_SKINNING
  worldTransform = transChain(args.currFrame.joint, worldTransform);
#endif // MS_NO_SKINNING
  worldTransform = transChain(args.currFrame.node, worldTransform);

  Transform currTransform = transChainNorm(currPassTransform, worldTransform);

  vec3 currVertexPos = transApply(currTransform,
    vec3(args.currFrame.vertexData.position.xyz));

  MsVertexOut result;
  result.position = projApply(passInfoBuffer.passes[context.invocation.passIndex].projection, currVertexPos);
  result.jointRotation = f16vec4(currTransform.rot);

#ifndef MS_NO_MOTION_VECTORS
  result.oldPosition = result.position.xyw;

  if (!asTest(context.flags, MS_NO_MOTION_VECTORS_BIT)) {
    Transform prevPassTransform = passInfoBuffer.passes[context.invocation.passIndex].prevTransform.transform;

    if (!asTest(context.invocation.instanceNode.flags, INSTANCE_NO_MOTION_VECTORS_BIT)) {
      worldTransform = Transform(
        vec4(context.invocation.meshInstance.transform),
        vec3(context.invocation.meshInstance.translate));

#ifndef MS_NO_SKINNING
      worldTransform = transChain(args.prevFrame.joint, worldTransform);
#endif // MS_NO_SKINNING
      worldTransform = transChain(args.prevFrame.node, worldTransform);
    }

    Transform prevTransform = transChainNorm(prevPassTransform, worldTransform);

    vec3 prevVertexPos = transApply(prevTransform,
      vec3(args.prevFrame.vertexData.position.xyz));

    result.oldPosition = projApply(passInfoBuffer.passes[context.invocation.passIndex].projection, prevVertexPos).xyw;
  }
#endif // MS_NO_MOTION_VECTORS

  return result;
}


FsInput msComputeFsInput(
  in    MsContext                     context,
  in    MsShadingParameters           args) {
  FsInput result;
  result.normal = normalize(quatApply(args.vertexData.jointRotation,
    unpackSnorm3x10(args.shadingData.normal)));
  result.texcoord = unpackUnorm2x16(args.shadingData.texcoord);
#ifndef MS_NO_MOTION_VECTORS
  result.prevFramePos = args.vertexData.oldPosition;
#else
  result.prevFramePos = args.vertexData.position.xyw;
#endif
  result.currFramePos = args.vertexData.position.xyw;
  return result;
}


MsUniformOut msComputeUniformOut(
  in    MsContext                     context) {
  MsUniformOut result;
  result.instanceVa = unpackUint2x32(context.invocation.instanceNode.propertyBuffer);
  result.drawIndex = context.invocation.drawIndex;
  return result;
}

#include "scene/instance/ms_instance_render.glsl"

#endif

#include "as_include_tail.glsl"
