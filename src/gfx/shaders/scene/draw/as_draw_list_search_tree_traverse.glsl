#ifndef AS_DRAW_LIST_TRAVERSE_SEARCH_TREE_H
#define AS_DRAW_LIST_TRAVERSE_SEARCH_TREE_H

#include "as_draw.glsl"

// Structure returned by the traversal function. Can be fed directly into
// the next iteration. for the last iteration, this will contain the draw
// index and the relative thread ID within the selected draw.
struct CsSearchResult {
  uint32_t nextOffset;
  uint32_t nextThreadId;
};


// Helper function to perform an inclusive scan across the entire
// workgroup. Must be called within uniform control flow.
shared uint32_t csInclusiveScanShared[TsWorkgroupSize];

uint32_t csWorkgroupInclusiveAdd(uint32_t tid, uint32_t value) {
  uint32_t scan = subgroupInclusiveAdd(value);

  if (!IsSingleSubgroup) {
    uint32_t sum;

    if (IsFullSubgroup)
      sum = subgroupBroadcast(scan, gl_SubgroupSize - 1u);
    else
      sum = subgroupAdd(value);

    if (subgroupElect())
      csInclusiveScanShared[gl_SubgroupID] = sum;

    barrier();

    // Perform an inclusive scan over the subgroup results using LDS.
    for (uint32_t i = 1u; i < gl_NumSubgroups; i += i) {
      uint32_t read = 0u;

      if (tid >= i && tid < gl_NumSubgroups)
        read = csInclusiveScanShared[tid] + csInclusiveScanShared[tid - i];

      barrier();

      if (tid >= i && tid < gl_NumSubgroups)
        csInclusiveScanShared[tid] = read;

      barrier();
    }

    // Adjust the scan value based on the exclusive subgroup scan
    if (gl_SubgroupID > 0u)
      scan += csInclusiveScanShared[gl_SubgroupID - 1u];

    barrier();
  }

  return scan;
}


// Helper function to find the first invocation within the
// workgroup for which a given condition is true.
shared uint32_t csInvocationIndexShared;

uint32_t csWorkgroupFindFirstInvocation(uint32_t tid, bool value) {
  if (IsFullSubgroup) {
    uint32_t result = subgroupBallotFindLSB(subgroupBallot(value));

    if (!IsSingleSubgroup) {
      result += gl_SubgroupID * gl_SubgroupSize;
      // On the single subgroup path we can assume that the
      // condition is true for one thread, but here we may
      // need to discard invalid results.
      result = subgroupAny(value) ? result : TsWorkgroupSize;

      if (tid == 0u)
        csInvocationIndexShared = TsWorkgroupSize;

      barrier();

      if (subgroupElect())
        atomicMin(csInvocationIndexShared, result);

      barrier();
      result = csInvocationIndexShared;
    }

    return result;
  } else {
    uint32_t result = subgroupMin(value ? tid : TsWorkgroupSize);

    if (tid == 0u)
      csInvocationIndexShared = TsWorkgroupSize;

    barrier();

    if (subgroupElect())
      atomicMin(csInvocationIndexShared, result);

    barrier();
    return csInvocationIndexShared;
  }
}


// Helper function to broadcast a value across the entire workgroup.
shared uint32_t csBroadcastShared;

uint32_t csWorkgroupBroadcast(
        uint32_t                      tid,
        uint32_t                      value,
        uint32_t                      index) {
  if (IsSingleSubgroup) {
    return subgroupBroadcast(value, index);
  } else {
    if (tid == index)
      csBroadcastShared = value;

    barrier();

    value = csBroadcastShared;
    barrier();

    return value;
  }
}


// Helper function to process a single layer of the search tree
CsSearchResult csScanSearchTreeLayer(
  in    DrawSearchTreeLayerIn         layer,
        uint32_t                      offset,
        uint32_t                      searchId) {
  uint32_t tid = IsFullSubgroup
    ? gl_SubgroupInvocationID + gl_SubgroupID * gl_SubgroupSize
    : gl_LocalInvocationIndex;

  // The offset is fed in from the previous iteration
  offset *= TsWorkgroupSize;
  uint32_t count = 0u;

  if (tid < TsWorkgroupSize)
    count = layer.threadCount[offset + tid];

  uint32_t scan = csWorkgroupInclusiveAdd(tid, count);
  uint32_t pick = csWorkgroupFindFirstInvocation(tid, searchId < scan);

  // Assume that the input was valid and that we picked a valid entry.
  CsSearchResult result;
  result.nextOffset = offset + pick;
  result.nextThreadId = searchId - csWorkgroupBroadcast(tid, scan - count, pick);
  return result;
}


CsSearchResult csTraverseSearchTree(
  in    DrawListBufferIn              drawListBuffer,
        uint32_t                      drawGroup,
        uint32_t                      searchId) {
  uint32_t layerCount = drawListBuffer.drawGroups[drawGroup].searchTreeDepth;

  CsSearchResult result;
  result.nextOffset = 0u;
  result.nextThreadId = searchId;

  while (layerCount > 0u) {
    uint32_t layerOffset = drawListBuffer.drawGroups[drawGroup].searchTreeLayerOffsets[layerCount - 1u];

    DrawSearchTreeLayerIn layer = DrawSearchTreeLayerIn(uint64_t(drawListBuffer) + layerOffset);
    result = csScanSearchTreeLayer(layer, result.nextOffset, result.nextThreadId);

    layerCount -= 1u;
  }

  return result;
}


#endif // AS_DRAW_LIST_BUILD_SEARCH_TREE_H
