#ifndef AS_ASSET_GROUP_H
#define AS_ASSET_GROUP_H

#define ASSET_LIST_STATUS_NON_RESIDENT  (0u)
#define ASSET_LIST_STATUS_RESIDENT      (1u)

#define ASSET_LIST_INVALID_HANDLE       (~0u)

// Asset list buffer header. Stores a frame ID of when the list has
// last changed to allow for dirty tracking, as well as a frame ID
// of when the asset list has last been used for rendering.
struct AssetListHeader {
  uint32_t        handle;
  uint32_t        reserved;
  uint32_t        updateFrameId;
  uint32_t        accessFrameId;
};

// Asset list reference type. Stores an asset list header, followed
// by a tightly packed dword array 
layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer AssetListBufferIn {
  AssetListHeader header;
  uint32_t        dwords[];
};

layout(buffer_reference, buffer_reference_align = 16, scalar)
writeonly buffer AssetListBufferOut {
  AssetListHeader header;
  uint32_t        dwords[];
};

layout(buffer_reference, buffer_reference_align = 16, scalar)
queuefamilycoherent buffer AssetListBuffer {
  AssetListHeader header;
  uint32_t        dwords[];
};


// Stream request buffer for asset lists. Stores a list of asset
// groups that need to be made resident for rendering purposes.
layout(buffer_reference, buffer_reference_align = 4, scalar)
buffer AssetFeedbackBuffer {
  uint32_t        entryCount;
  uint32_t        entries[];
};


// Updates the access frame ID of the given asset list, and adds
// the asset list handle to the feedback buffer so that the host
// knows which asset lists are actively being used for rendering.
// Must be called from uniform control flow, and the feedback
// buffer address and frame ID must also be uniform.
void assetListNotifyAccess(
        uint64_t                      assetListVa,
        uint64_t                      assetFeedbackVa,
        uint32_t                      frameId) {
  uint32_t enqueueHandle = ASSET_LIST_INVALID_HANDLE;

  // It is very likely for instances within the same workgroup to share
  // asset lists. Scalarize here in order to reduce some atomic spam.
  for (bool processed = assetListVa == 0ul; !processed && subgroupAny(!processed); ) {
    u32vec2 addr = unpackUint2x32(assetListVa);
    u32vec2 first = subgroupBroadcastFirst(addr);

    if (processed = all(equal(addr, first)))
      assetListVa = subgroupElect() ? assetListVa : 0ul;
  }

  if (assetListVa != 0ul) {
    AssetListBuffer assetListBuffer = AssetListBuffer(assetListVa);
    AssetListHeader header = assetListBuffer.header;

    if (header.accessFrameId < frameId) {
      // Ensure only one active thread can add the asset list to the
      // feedback buffer
      bool cas = atomicCompSwap(assetListBuffer.header.accessFrameId,
        header.accessFrameId, frameId) == header.accessFrameId;

      if (cas)
        enqueueHandle = header.handle;
    }
  }

  // Can't enqueue much without a stream request buffer
  if (assetFeedbackVa == 0ul)
    return;

  // Submit stream requests for any non-resident asset list
  // that has been accessed this frame. Minimize atomic spam.
  bool enqueue = enqueueHandle != ASSET_LIST_INVALID_HANDLE;

  u32vec4 threadMask = subgroupBallot(enqueue);
  uint32_t threadCount = subgroupBallotBitCount(threadMask);
  uint32_t threadIndex = subgroupBallotExclusiveBitCount(threadMask);

  if (threadCount == 0u)
    return;

  AssetFeedbackBuffer feedbackBuffer = AssetFeedbackBuffer(assetFeedbackVa);

  uint32_t firstIndex;

  if (subgroupElect())
    firstIndex = atomicAdd(feedbackBuffer.entryCount, threadCount);

  threadIndex += subgroupBroadcastFirst(firstIndex);

  if (enqueue)
    feedbackBuffer.entries[threadIndex] = enqueueHandle;
}

#endif /* AS_ASSET_GROUP_H */
