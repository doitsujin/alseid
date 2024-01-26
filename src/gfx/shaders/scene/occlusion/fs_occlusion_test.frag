// Fragment shader for occlusion testing.
#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_mesh_shader : enable

#include "../../as_include_head.glsl"

#include "as_occlusion_test.glsl"

#define FS_MAIN fsMain

layout(set = 0, binding = 0)
uniform texture2D rHizImage;


void fsUpdateVisibility(FsUniform fsUniform) {
  PassGroupBuffer passGroup = PassGroupBuffer(globals.passGroupVa);
  PassGroupBvhList bvhList = PassGroupBvhList(globals.passGroupVa + passGroup.bvhListOffset);
  PassGroupBvhVisibilityBuffer bvhVisibilityBuffer = PassGroupBvhVisibilityBuffer(globals.passGroupVa + passGroup.bvhVisibilityOffset);
  PassGroupBvhVisibility bvhVisibility = bvhVisibilityBuffer.bvhs[fsUniform.bvhIndex];

  PassGroupBvhListItem bvhItem;
  bvhItem.nodeRef = makeNodeRef(NODE_TYPE_BVH, fsUniform.bvhIndex);
  bvhItem.visibilityMask = (2u << (passGroup.passCount - 1u)) - 1u;

  bvhMarkVisible(bvhList, bvhVisibilityBuffer, bvhVisibility,
    1u << fsUniform.passIndex, bvhItem);
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
      fsUpdateVisibility(fsUniform);
  }
}

#include "../../as_include_tail.glsl"
