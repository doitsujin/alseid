#version 460

#extension GL_GOOGLE_include_directive : enable

#include "as_include_head.glsl"

#ifdef STAGE_MESH
#include "render/ms_render_common.glsl"
#endif

// Use existing template for mesh/vertex shader
#define MS_MAIN msMain

// We implement the full FS here, but the app
// could provide its own set of FS templates
#define FS_MAIN fsMain

struct MsUniformOut {
  uint meshlet;
};


// Data structures shared between FS and VS/MS
#define FS_INPUT                                  \
  FS_INPUT_VAR((location = 0), vec3, normal)      \
  FS_INPUT_VAR((location = 1), vec3, currFramePos)\
  FS_INPUT_VAR((location = 2), vec3, prevFramePos)

FS_DECLARE_INPUT(FS_INPUT);

#define FS_UNIFORM                                \
  FS_INPUT_VAR((location = 3), uint, meshlet)

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

void fsMain(in FsInput fsInput, in FsUniform fsUniform) {
  float factor = 0.5f + 0.5f * dot(normalize(fsInput.normal), vec3(0.0f, 1.0f, 0.0f));

  vec2 currPos = fsInput.currFramePos.xy / fsInput.currFramePos.z;
  vec2 prevPos = fsInput.prevFramePos.xy / fsInput.prevFramePos.z;

  vec2 motion = 0.5f + 0.5f * (50.0f * (currPos - prevPos));
  vec3 color = vec3(motion, 1.0f - dot(motion, motion));

  fsColor = vec4(color * factor, 1.0f);
}

#endif


////////////////////////////
//       MESH SHADER      //
////////////////////////////
#ifdef STAGE_MESH

struct MsContext {
  MsInvocationInfo  invocation;
  uint32_t          flags;
};


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
  vec4 position;
};


MsContext msGetInstanceContext() {
  MsContext context;
  context.invocation = msGetInvocationInfo(globals.instanceVa, globals.frameId);
  context.flags = MS_CULL_FACE_CW_BIT;

  if (asGetMirrorMode(context.invocation.meshInstance.extra) != MESH_MIRROR_NONE)
    context.flags |= MS_FLIP_FACE_BIT;

  if ((PassInfoBufferIn(globals.passInfoVa).passes[context.invocation.passIndex].flags & PASS_IGNORE_OCCLUSION_TEST_BIT) != 0u)
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


shared f16vec4 rotations[MAX_VERT_COUNT];


MsVertexOut msComputeVertexOutput(
  in    MsContext                     context,
        uint                          vertexIndex,
  in    MsVertexIn                    vertex,
  in    Transform                     jointTransform,
        bool                          currFrame) {
  MsVertexOut result;

  PassInfoBufferIn passInfoBuffer = PassInfoBufferIn(globals.passInfoVa);

  Transform finalTransform = Transform(
    vec4(context.invocation.meshInstance.transform),
    vec3(context.invocation.meshInstance.translate));

  Transform passTransform = currFrame
    ? passInfoBuffer.passes[context.invocation.passIndex].currTransform.transform
    : passInfoBuffer.passes[context.invocation.passIndex].prevTransform.transform;

  Transform nodeTransform = msLoadNodeTransform(currFrame);

  finalTransform = transChain(jointTransform, finalTransform);
  finalTransform = transChain(nodeTransform, finalTransform);
  finalTransform = transChainNorm(passTransform, finalTransform);
  vec3 vertexPos = transApply(finalTransform, vec3(vertex.position.xyz));

  result.position = projApply(passInfoBuffer.passes[context.invocation.passIndex].projection, vertexPos);

  if (currFrame)
    rotations[vertexIndex] = f16vec4(finalTransform.rot);

  return result;
}


FsInput msComputeFsInput(
  in    MsContext                     context,
        uint                          vertexIndex,
  in    MsVertexIn                    vertexIn,
  in    MsVertexOut                   vertexOut,
        vec3                          vertexPosOld,
  in    MsShadingIn                   shadingIn,
  in    MsUniformOut                  uniformOut) {
  vec4 transform = vec4(rotations[vertexIndex]);

  FsInput result;
  result.normal = normalize(quatApplyNorm(transform, unpackSnorm3x10(shadingIn.normal)));
  result.prevFramePos = vertexPosOld;
  result.currFramePos = vertexOut.position.xyw;
  return result;
}


MsUniformOut msComputeUniformOut(
  in    MsContext                     context) {
  MsUniformOut result;
  result.meshlet = uint32_t(context.invocation.meshletVa) >> 4;
  return result;
}

#include "render/ms_render_instance.glsl"

#endif

#include "as_include_tail.glsl"
