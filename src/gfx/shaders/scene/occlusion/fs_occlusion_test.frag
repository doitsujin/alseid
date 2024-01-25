// Fragment shader for occlusion testing.
#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_mesh_shader : enable

#include "../../as_include_head.glsl"

#include "as_occlusion_test.glsl"

#define FS_MAIN fsMain

layout(set = 0, binding = 0)
uniform texture2D rHizImage;

void fsUpdateVisibility(FsUniform fsUniform, uint32_t index) {
  PassGroupBvhVisibilityBufferCoherent bvhVisibility = PassGroupBvhVisibilityBufferCoherent(fsUniform.bvhBufferVa);
  uint32_t visibility = bvhVisibility.bvhs[index].visibilityMask;
  uint32_t passBit = 1u << fsUniform.passIndex;

  if (!asTest(visibility, passBit))
    bvhVisibility.bvhs[index].visibilityMask = visibility | passBit;
}


void fsMain(FsUniform fsUniform) {
  float minZ = texelFetch(rHizImage, ivec2(gl_FragCoord.xy), 0).x;

  if (gl_FragCoord.z < minZ)
    discard;

  if (!helperInvocationEXT()) {
    bool elected = false;

    SUBGROUP_SCALARIZE(fsUniform.bvhIndex) {
      elected = subgroupElect();
    }

    if (elected)
      fsUpdateVisibility(fsUniform, fsUniform.bvhIndex);
  }
}

#include "../../as_include_tail.glsl"
