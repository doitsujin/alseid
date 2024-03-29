#ifndef AS_SCENE_H
#define AS_SCENE_H

// Node types as encoded in node references
#define NODE_TYPE_ABSTRACT              (0u)
#define NODE_TYPE_BVH                   (1u)

#define NODE_TYPE_INSTANCE              (2u)

#define NODE_TYPE_BUILTIN_COUNT         (2u)
#define NODE_TYPE_COUNT                 (32u)

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

uvec2 nodeComputeTransformIndices(
        uint32_t                      nodeIndex,
        uint32_t                      nodeCount,
        uint32_t                      frameId) {
  uint32_t curr = nodeCount * (frameId & 1u);
  uint32_t prev = nodeCount - curr;
  return uvec2(curr, prev) + nodeIndex;
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
struct SceneHeader {
  uint32_t  nodeParameterOffset;
  uint32_t  nodeTransformOffset;
  uint32_t  nodeCount;
  uint32_t  bvhOffset;
  uint32_t  bvhCount;
};

layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer SceneHeaderIn {
  SceneHeader header;
};

#endif /* AS_SCENE_H */
