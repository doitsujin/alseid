// Residency status of assets and BVH nodes
#define RESIDENCY_STATUS_NONE           (0u)
#define RESIDENCY_STATUS_PARTIAL        (1u)
#define RESIDENCY_STATUS_FULL           (2u)

#define RESIDENCY_FLAG_REQUEST_STREAM   (1u << 2)
#define RESIDENCY_FLAG_REQUEST_EVICT    (1u << 3)

#define RESIDENCY_FLAG_MASK             (0xc)


// Node types as encoded in node references
#define NODE_TYPE_NONE                  (0u)
#define NODE_TYPE_BVH                   (1u)
#define NODE_TYPE_LIGHT                 (2u)
#define NODE_TYPE_INSTANCE              (3u)


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


// Absolute transform of a node. The frame ID is used
// to track and commit updates to the transform.
struct SceneNodeTransform {
  Transform absoluteTransform;
  uint32_t  updateFrameId;
};


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

