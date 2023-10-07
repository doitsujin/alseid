#ifndef AS_SCENE_STREAM_H
#define AS_SCENE_STREAM_H

// Type identifier for stream requests
#define STREAM_TYPE_UNDEFINED           (0u)
#define STREAM_TYPE_ASSET               (1u)
#define STREAM_TYPE_NODE                (2u)


// Stream request type
#define STREAM_REQUEST_UNDEFINED        (0u)
#define STREAM_REQUEST_STREAM           (1u)
#define STREAM_REQUEST_EVICT            (2u)


// Stream request buffer type. Consists of a header that stores the capacity
// as well as the allocator position within the buffer, and an array of
// stream requests encoded as 32-bit integers.
layout(buffer_reference, buffer_reference_align = 4, scalar)
buffer StreamRequestBuffer {
  uint32_t maxRequestCount;
  uint32_t curRequestCount;
  uint32_t requests[];
};


// Encodes stream request into a single 32-bit unsigned integer
uint32_t packStreamRequest(uint32_t request, uint32_t type, uint32_t index, uint32_t lod) {
  uint32_t result = index;
  result = bitfieldInsert(result, lod, 24, 4);
  result = bitfieldInsert(result, type, 28, 2);
  result = bitfieldInsert(result, request, 30, 2);
  return result;
}


// Convenience method to encode a node stream request
uint32_t packStreamRequestForNode(uint32_t request, uint32_t index) {
  return packStreamRequest(request, STREAM_TYPE_NODE, index, 0u);
}


// Convenience method to encode an asset stream request
uint32_t packStreamRequestForAsset(uint32_t request, uint32_t index, uint32_t lod) {
  return packStreamRequest(request, STREAM_TYPE_ASSET, index, lod);
}


// Adds stream request to the global stream request buffer. Returns false if
// the request buffer is full, in which case any changes setting stream request
// bits on the original object must be rolled back.
bool submitStreamRequest(StreamRequestBuffer requestBuffer, uint32_t request) {
  uvec4 ballot = subgroupBallot(true);

  uint32_t localIndex = subgroupBallotExclusiveBitCount(ballot);
  uint32_t localCount = subgroupBallotBitCount(ballot);

  uint32_t globalIndex;

  if (subgroupElect())
    globalIndex = atomicAdd(requestBuffer.curRequestCount, localCount);

  globalIndex = subgroupBroadcastFirst(globalIndex) + localIndex;

  if (globalIndex >= requestBuffer.maxRequestCount)
    return false;

  requestBuffer.requests[globalIndex] = request;
  return true;
}

#endif /* AS_SCENE_STREAM_H */
