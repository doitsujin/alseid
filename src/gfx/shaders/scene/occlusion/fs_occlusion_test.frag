// Fragment shader for occlusion testing.
//
// 
#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_mesh_shader : enable

#include "../../as_include_head.glsl"

#include "as_occlusion_test.glsl"

#define FS_MAIN fsMain

// Need to explicitly opt into early fragment tests since
// the shader has side effects.
layout(early_fragment_tests) in;


void fsUpdateVisibility(FsUniform fsUniform, uint32_t index) {
  PassGroupBvhVisibilityBufferCoherent bvhVisibility = PassGroupBvhVisibilityBufferCoherent(fsUniform.bvhBufferVa);
  uint32_t visibility = bvhVisibility.bvhs[index].testFailMask;
  uint32_t passBit = 1u << fsUniform.passIndex;

  if ((visibility & passBit) == passBit)
    bvhVisibility.bvhs[index].testFailMask = visibility - passBit;
}


void fsMain(FsUniform fsUniform) {
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
