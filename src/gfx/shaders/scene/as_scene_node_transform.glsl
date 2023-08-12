#define MAX_NODE_DEPTH (16u)

layout(set = 0, binding = 0, scalar)
readonly buffer SceneNodeBuffer {
  SceneNode nodeInfos[];
};

layout(set = 0, binding = 1, scalar)
queuefamilycoherent buffer SceneNodeTransformBuffer {
  SceneNodeTransform nodeTransforms[];
};


// Helper function to load a sub-transform for a given node.
// Takes the node reference rather than the actual node index.
Transform csLoadSubTransform(uint32_t nodeRef, int32_t index) {
  // TODO implement properly
  return transIdentity();
}


// Node index stack. Needed to resolve recursive node updates appropriately.
// Stores the unsigned node index in the lower 24 bits, and the subgroup
// invocation index for the absolute parent transform in the upper 8 bits.
shared uint32_t csNodeIndicesShared[MAX_NODE_DEPTH * CS_WORKGROUP_SIZE];

void csPushNode(uint32_t depth, u32vec2 node) {
  uint32_t packed = bitfieldInsert(node.x, node.y, 24, 8);
  csNodeIndicesShared[depth * CS_WORKGROUP_SIZE + gl_LocalInvocationIndex] = packed;
}

u32vec2 csPopNode(uint32_t depth) {
  uint32_t packed = csNodeIndicesShared[depth * CS_WORKGROUP_SIZE + gl_LocalInvocationIndex];
  return uvec2(bitfieldExtract(packed,  0, 24), bitfieldExtract(packed, 24, 8));
}


// Selects invocations to process a given node. Essentially de-duplicates
// nodes on a subgroup level in order to avoid situations where a large
// number of invocations redundantly write the same transforms. Returns
// the subgroup invocation index to use as input for shuffles.
uint32_t csPickInvocationForNode(int32_t node) {
  uint32_t result;

  SUBGROUP_SCALARIZE(node) {
    result = subgroupBallotFindLSB(subgroupBallot(true));
  }

  return result;
}


// Computes and updates the absolute transform of a node using a
// known parent transform as a starting point. Other invocations
// will be able to observe the updated transform afterwards. 
Transform csUpdateNodeTransform(uint32_t node, uint32_t frameId, Transform parentTransform) {
  // Compute the relative node transform, including the sub-transform
  Transform relativeTransform = nodeInfos[node].transform;
  int32_t subTransformIndex = nodeInfos[node].parentTransform;

  if (subTransformIndex >= 0) {
    Transform subTransform = csLoadSubTransform(nodeInfos[node].parentNodeRef, subTransformIndex);
    relativeTransform = transChain(subTransform, relativeTransform);
  }

  // Apply parent transform to compute the absolute transform
  Transform absoluteTransform = transChain(parentTransform, relativeTransform);

  // Write back absolute transform to the node array, and update the frame
  // ID to commit the change and make it visible to other workgroups that
  // may access the same set of nodes.
  nodeTransforms[node].absoluteTransform = absoluteTransform;

  atomicStore(nodeTransforms[node].updateFrameId, frameId,
    gl_ScopeQueueFamily, gl_StorageSemanticsBuffer,
    gl_SemanticsRelease | gl_SemanticsMakeAvailable);

  return absoluteTransform;
}


// Recursively computes absolute transform for a given node. This
// actively tries to reduce the number of invocations writing to
// each node in order to avoid redundant memory accesses.
Transform csComputeNodeTransform(uint32_t nodeIndex, uint32_t frameId) {
  // We want the node index to be signed so that we can easily
  // check for the root node, which is generally encoded as -1.
  int32_t node = int32_t(nodeIndex);

  // Flag that indicates whether the current invocation needs to
  // read the parent transform for its node from another invocation
  bool needsParentTransform = false;

  // Recursion depth for the current invocation
  uint32_t curDepth = 0;

  // Frame ID of when the current node has last been updated, and use
  // appropriate memory semantics to ensure we can correctly read the
  // updated absolute transform if the frame ID is already up to date.
  uint32_t updateFrameId = atomicLoad(nodeTransforms[node].updateFrameId,
    gl_ScopeQueueFamily, gl_StorageSemanticsBuffer,
    gl_SemanticsAcquire | gl_SemanticsMakeVisible);

  while (updateFrameId < frameId && node >= 0 && curDepth <= MAX_NODE_DEPTH) {
    int32_t parent = nodeInfos[node].parentNode;

    // Pick an invocation to update the parent node should it be necessary,
    // and write the node + parent invocation pair to the stack.
    uint32_t invocation = csPickInvocationForNode(parent);
    csPushNode(curDepth++, uvec2(node, invocation));

    // Exit early if a different invocation is responsible for the parent.
    if (invocation != gl_SubgroupInvocationID) {
      needsParentTransform = true;
      break;
    }

    if (parent >= 0) {
      updateFrameId = atomicLoad(nodeTransforms[parent].updateFrameId,
        gl_ScopeQueueFamily, gl_StorageSemanticsBuffer,
        gl_SemanticsAcquire | gl_SemanticsMakeVisible);
    }

    node = parent;
  }

  // Find maximum depth within the subgroup, so that we can
  // update individual node transforms in the correct order.
  uint32_t maxDepth = subgroupMax(curDepth);

  // For all active invocations who do not depend on a parent
  // transform, we can just load the absolute transform from
  // memory since it is known to be up to date.
  Transform absoluteTransform = transIdentity();

  if (node >= 0 && !needsParentTransform)
    absoluteTransform = nodeTransforms[node].absoluteTransform;

  for (uint32_t i = 1; i <= maxDepth; i++) {
    uint32_t layer = maxDepth - i;

    if (layer < curDepth) {
      u32vec2 next = csPopNode(layer);

      // The parent transform is well-defined for the subgroup
      // invocation index we pulled from the stack at this point.
      absoluteTransform.rot = subgroupShuffle(absoluteTransform.rot, next.y);
      absoluteTransform.pos = subgroupShuffle(absoluteTransform.pos, next.y);

      // Compute and store the absolute transform of the current node
      absoluteTransform = csUpdateNodeTransform(next.x, frameId, absoluteTransform);
    }
  }

  return absoluteTransform;
}
