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
  FS_INPUT_VAR((location = 1), vec2, texcoord)

FS_DECLARE_INPUT(FS_INPUT);

#define FS_UNIFORM                                \
  FS_INPUT_VAR((location = 2), uint, meshlet)

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

  vec3 color = 0.25f + 0.125f * vec3(
    float((fsUniform.meshlet & 0x007) >> 0),
    float((fsUniform.meshlet & 0x031) >> 3),
    float((fsUniform.meshlet & 0x1c0) >> 6));

  fsColor = vec4(color * factor, 1.0f);
}

#endif


////////////////////////////
//       MESH SHADER      //
////////////////////////////
#ifdef STAGE_MESH

struct MsContext {
  MsInvocationInfo  invocation;
  MsRenderState     renderState;
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
  uvec2 meshlet = msDecodeMeshlet(tsPayload.meshlets[gl_WorkGroupID.x]);

  MsContext context;
  context.invocation = msGetInvocationInfo(globals.instanceVa, globals.frameId);
  context.renderState.cullMode = FACE_CULL_MODE_CW;
  context.renderState.faceFlip = false;
  return context;
}


void msMorphVertex(inout MsVertexIn vertex, in MsMorphIn morph, float weight) {
  vertex.position += morph.position * float16_t(weight);
}


void msMorphShading(inout MsShadingIn shading, in MsMorphIn morph, float weight) {
  shading.normal = packSnorm3x10(normalize(
    unpackSnorm3x10(shading.normal) +
    unpackSnorm3x10(morph.normal) * weight));
}


shared f16vec4 rotations[MAX_VERT_COUNT];


MsVertexOut msComputeVertexPos(in MsContext context, uint vertexIndex, in MsVertexIn vertex, in Transform jointTransform) {
  MsVertexOut result;

  PassInfoBufferIn passInfoBuffer = PassInfoBufferIn(globals.passInfoVa);

  Transform finalTransform = Transform(
    vec4(context.invocation.meshInstance.transform),
    vec3(context.invocation.meshInstance.translate));

  finalTransform = transChain(jointTransform, finalTransform);
  finalTransform = transChain(msLoadNodeTransform(0), finalTransform);
  finalTransform = transChain(passInfoBuffer.passes[context.invocation.passIndex].currTransform.transform, finalTransform);
  vec3 vertexPos = transApply(finalTransform, vec3(vertex.position.xyz));

  result.position = projApply(passInfoBuffer.passes[context.invocation.passIndex].projection, vertexPos);

  rotations[vertexIndex] = f16vec4(finalTransform.rot);
  return result;
}


FsInput msComputeFsInput(in MsContext context, uint vertexIndex, in MsVertexIn vertex, in MsVertexOut vertexOut, in MsShadingIn shading, in MsUniformOut uniformOut) {
  vec4 transform = vec4(rotations[vertexIndex]);

  FsInput result;
  result.normal = normalize(quatApplyNorm(transform, unpackSnorm3x10(shading.normal)));
  result.texcoord = unpackUnorm2x16(shading.texcoord);
  return result;
}


MsUniformOut msComputeUniformOut(in MsContext context) {
  MsUniformOut result;
  result.meshlet = tsPayload.meshlets[gl_WorkGroupID.x];
  return result;
}

#include "render/ms_render_instance.glsl"

#endif

#include "as_include_tail.glsl"
