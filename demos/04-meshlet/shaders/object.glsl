#version 460

#extension GL_GOOGLE_include_directive : enable

#include "as_include_head.glsl"

#include "pass.glsl"

// Use existing template for mesh/vertex shader
#define MS_MAIN msMain
#define VS_MAIN vsMain

// We implement the full FS here, but the app
// could provide its own set of FS templates
#define FS_MAIN fsMain

#define MS_NO_UNIFORM_OUTPUT

// Data structures shared between FS and VS/MS
#define FS_INPUT                                  \
  FS_INPUT_VAR((location = 0), vec3, normal)      \
  FS_INPUT_VAR((location = 1), vec2, texcoord)    \
  FS_INPUT_VAR((location = 2) flat, uint, meshlet)

FS_DECLARE_INPUT(FS_INPUT);


////////////////////////////
//    FRAGMENT SHADER     //
////////////////////////////
#ifdef STAGE_FRAG

layout(location = 0) out vec4 fsColor;

void fsMain(in FsInput fsInput) {
  float factor = 0.5f + 0.5f * dot(normalize(fsInput.normal), vec3(0.0f, 1.0f, 0.0f));

  vec3 color = 0.25f + 0.125f * vec3(
    float((fsInput.meshlet & 0x007) >> 0),
    float((fsInput.meshlet & 0x031) >> 3),
    float((fsInput.meshlet & 0x1c0) >> 6));

  fsColor = vec4(color * factor, 1.0f);
}

////////////////////////////
//  VERTEX / MESH SHADER  //
////////////////////////////
#else

struct MsVertexIn {
  f16vec3   position;
};


struct MsShadingIn {
  uint32_t  normal;
  uint32_t  texcoord;
};


struct MsMorphIn {
  f16vec3   position;
  float16_t pad;
  uint32_t  normal;
};


struct MsVertexOut {
  vec4 position;
};


struct FsUniform {
  float frog;
};


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

  Transform finalTransform = Transform(
    vec4(payload.instance.transform),
    vec3(payload.instance.translate));

  finalTransform = transChain(jointTransform, finalTransform);
  finalTransform = transChain(payload.modelViewTransform, finalTransform);
  vec3 vertexPos = transApply(finalTransform, vec3(vertex.position.xyz));

  result.position = projApply(scene.projection, vertexPos);

  rotations[vertexIndex] = f16vec4(finalTransform.rot);
  return result;
}


FsInput msComputeFsInput(in MsContext context, uint vertexIndex, in MsVertexIn vertex, in MsVertexOut vertexOut, in MsShadingIn shading) {
  vec4 transform = vec4(rotations[vertexIndex]);

  FsInput result;
  result.normal = normalize(quatApplyNorm(transform, unpackSnorm3x10(shading.normal)));
  result.texcoord = unpackUnorm2x16(shading.texcoord);
  result.meshlet = payload.meshletIndices[gl_WorkGroupID.x];
  return result;
}


FsUniform msComputeFsUniform(in MsContext context) {
  FsUniform result;
  result.frog = 1.0f;
  return result;
}


float msLoadMorphTargetWeight(in MsContext context, uint index) {
  return weights[index];
}

#endif

#include "as_include_tail.glsl"
