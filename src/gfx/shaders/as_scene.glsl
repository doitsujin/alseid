// Residency status of assets and BVH nodes
#define RESIDENCY_STATUS_NONE           (0u)
#define RESIDENCY_STATUS_PARTIAL        (1u)
#define RESIDENCY_STATUS_FULL           (2u)

#define RESIDENCY_FLAG_REQUEST_STREAM   (1u << 2)
#define RESIDENCY_FLAG_REQUEST_EVICT    (1u << 3)

#define RESIDENCY_FLAG_MASK             (0xc)


// Node types as encoded in node references
#define NODE_TYPE_ABSTRACT              (0u)
#define NODE_TYPE_BVH                   (1u)


#define NODE_TYPE_BUILTIN_COUNT         (2u)
#define NODE_TYPE_COUNT                 (32u + NODE_TYPE_BUILTIN_COUNT)

// Node info. Stores the relative transform
// and the node's place within the hierarchy.
struct SceneNode {
  Transform transform;
  uint32_t  updateFrameId;
  int32_t   parentNode;
  int32_t   parentTransform;
  uint32_t  parentNodeRef;
  uint32_t  nodeRef;
};

layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer SceneNodeInfoBuffer {
  SceneNode nodeInfos[];
};


// Absolute transform of a node. The frame ID is used
// to track and commit updates to the transform.
struct SceneNodeTransform {
  Transform absoluteTransform;
  uint32_t  updateFrameId;
};

layout(buffer_reference, buffer_reference_align = 16, scalar)
queuefamilycoherent buffer SceneNodeTransformBufferOut {
  SceneNodeTransform nodeTransforms[];
};

layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer SceneNodeTransformBufferIn {
  SceneNodeTransform nodeTransforms[];
};


// Node residency buffer. Stores one byte for each node. The frame
// ID of when the node was last used should be taken from the node
// transform array.
layout(buffer_reference, buffer_reference_align = 4, scalar)
writeonly buffer SceneNodeResidencyBufferOut {
  uint8_t nodeResidency[];
};

layout(buffer_reference, buffer_reference_align = 4, scalar)
buffer SceneNodeResidencyBuffer {
  uint32_t nodeResidency[];
};


// Atomically sets streaming or eviction flag for a given node, and
// returns the previous residency status from before the operation.
// This is useful to only submit stream requests once per node.
uint32_t setNodeResidencyFlags(SceneNodeResidencyBuffer nodes, uint32_t nodeIndex, uint32_t flags) {
  uint32_t dword = nodeIndex / 4;
  uint32_t byte  = nodeIndex % 4;

  uint32_t prev = atomicOr(nodes.nodeResidency[dword], flags << (8 * byte));
  return bitfieldExtract(prev, 8 * int32_t(byte), 8);
}


// Reads node residency status for a given node.
uint32_t getNodeResidency(SceneNodeResidencyBuffer nodes, uint32_t nodeIndex) {
  uint32_t dword = nodeIndex / 4;
  uint32_t byte  = nodeIndex % 4;

  return bitfieldExtract(nodes.nodeResidency[dword], 8 * int32_t(byte), 8);
}


// Sets residency status, including flags, for a given node. Must
// not be used in conjunction with atomically setting status flags.
void setNodeResidency(SceneNodeResidencyBufferOut nodes, uint32_t nodeIndex, uint32_t residency) {
  nodes.nodeResidency[nodeIndex] = uint8_t(residency);
}


// Extracts node type from a node reference.
uint32_t getNodeTypeFromRef(uint32_t nodeRef) {
  return bitfieldExtract(nodeRef, 0, 8);
}


// Extracts object index from a node reference. The given index
// is not the same as the actual node index, and instead points
// into the type-specific object array.
uint32_t getNodeIndexFromRef(uint32_t nodeRef) {
  return bitfieldExtract(nodeRef, 8, 24);
}


// Builds node reference from a type and index pair.
uint32_t makeNodeRef(uint32_t type, uint32_t index) {
  return type | (index << 8u);
}


// BVH node info
struct SceneBvhNode {
  int32_t   nodeIndex;
  Aabb16    aabb;
  float16_t maxDistance;
  uint16_t  childCount;
  int32_t   chainedNode;
  uint32_t  childNodes[26];
};

layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer SceneBvhNodeBuffer {
  SceneBvhNode nodes[];
};


// Scene buffer info. Stores a bunch of offsets to various node lists
// that can be both written and read by various compute passes.
layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer SceneHeader {
  uint32_t  nodeParameterOffset;
  uint32_t  nodeTransformOffset;
  uint32_t  nodeResidencyOffset;
  uint32_t  bvhOffset;
};
