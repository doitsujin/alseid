#ifndef AS_DRAW_LIST_BUILD_SEARCH_TREE_H
#define AS_DRAW_LIST_BUILD_SEARCH_TREE_H

#include "as_draw.glsl"

// Helper function to reduce a layer using subgroup
// operations and, if necessary, LDS.
shared uint32_t csCountShared[TsWorkgroupSize];

uint32_t csWorkgroupAdd(uint32_t value) {
  uint32_t result = subgroupAdd(value);

  if (!IsSingleSubgroup) {
    // Use LDS to further reduce the subgroup results
    uint32_t tid = gl_LocalInvocationIndex;

    if (subgroupElect())
      csCountShared[gl_SubgroupID] = result;

    barrier();

    for (uint32_t i = gl_NumSubgroups >> 1u; i > 0u; i >>= 1u) {
      if (tid < i)
        csCountShared[tid] += csCountShared[tid + i];

      barrier();
    }

    result = csCountShared[0u];
  }

  return result;
}

#endif // AS_DRAW_LIST_BUILD_SEARCH_TREE_H
