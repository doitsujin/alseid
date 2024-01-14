#pragma once

#include "../../util/util_object_map.h"
#include "../../util/util_quaternion.h"

#include "../gfx.h"
#include "../gfx_geometry.h"
#include "../gfx_types.h"

#include "gfx_scene_common.h"
#include "gfx_scene_pipelines.h"

namespace as {

class GfxScenePassGroupBuffer;

/**
 * \brief Scene node info
 *
 * Stores the relative transform of a node, as well as links to its
 * parent node, which is used to compute absolute transforms within
 * the node hierarchy.
 */
struct GfxSceneNodeInfo {
  /** Rotation quaternion, relative to parent transform. Note that
   *  this is stored as a 16-bit float vector in order to save memory. */
  Vector<float, 4> rotation;
  /** Translation vector, relative to parent transform. */
  Vector<float, 3> translation;
  /** Frame ID of when this node has last been updated. This can be
   *  used to skip expensive recalculation of the absolute transform
   *  if the relative transform has not changed. */
  uint32_t updateFrameId;
  /** Parent node index, or -1 if the node is not attached to a parent. */
  int32_t parentNode;
  /** Index of the parent transform to use. If the parent is a geometry
   *  node, this refers to a joint index. Note that this cannot be used
   *  for BVH nodes since joint transforms are only resolved after fully
   *  traversing the BVH to perform initial coarse culling. */
  int32_t parentTransform;
  /** Reference to the parent node. */
  GfxSceneNodeRef parentNodeRef;
  /** Reference to the node itself. */
  GfxSceneNodeRef nodeRef;
};

static_assert(sizeof(GfxSceneNodeInfo) == 48);


/**
 * \brief Scene node transform
 *
 * Mirrors the data structure used on the GPU to store the absolute
 * transform of a node, as well as info on when it was last updated.
 */
struct GfxSceneNodeTransform {
  /** Absolute rotation quaternion of the node. */
  Vector<float, 4> rotation;
  /** Absolute translation vector of the node. */
  Vector<float, 3> translation;
  /** Frame ID of when the absolute transform has last been updated. If
   *  less than the current frame ID, shaders will need to compute the
   *  absolute transform recursively by applying the node's relative
   *  transform to the parent's absolute transform, and then update the
   *  frame ID to ensure the computation is not done redundantly. */
  uint32_t updateFrameId;
};

static_assert(sizeof(GfxSceneNodeTransform) == 32);


/**
 * \brief Node list header
 *
 * Provides a node count which shaders can use as a linear
 * allocator, and parameters for an indirect compute dispatch.
 */
struct GfxSceneNodeListHeader {
  /** Indirect dispatch parameters */
  GfxDispatchArgs dispatch;
  /** Number of list entries */
  uint32_t entryCount;
};

static_assert(sizeof(GfxSceneNodeListHeader) == 16);


/**
 * \brief Node list entry
 *
 * Stores the node reference, as well as the visibility status. During BVH
 * traversal the visibility masks will be initialized with the visibility
 * mask of the parent BVH node itself, a further pass over the node list
 * should then perform more fine-grained culling and update the masks
 * accordingly.
 */
struct GfxSceneNodeListEntry {
  /** Typed node reference. */
  GfxSceneNodeRef nodeRef;
  /** Mask of passes where the node is partially visible. On input,
   *  this is equal to the parent BVH node's partial visibility mask. */
  uint32_t partialVisibilityMask;
  /** Mask of passes where the node is fully visible. On input, this is
   *  equal to the parent BVH node's full visibility mask. Can be used to
   *  skip expensive computations for fully visible nodes. */
  uint32_t fullVisibilityMask;
};

static_assert(sizeof(GfxSceneNodeListEntry) == 12);


/**
 * \brief Node dirty flags
 */
enum class GfxSceneNodeDirtyFlag : uint32_t {
  /** Plain node info is dirty and must be updated. */
  eDirtyNode              = (1u << 0),
  /** Bounding volume properties are dirty. */
  eDirtyBvhNode           = (1u << 1),
  /** Child list and chained nodes are dirty. */
  eDirtyBvhChain          = (1u << 2),

  eFlagEnum               = 0
};

using GfxSceneNodeDirtyFlags = Flags<GfxSceneNodeDirtyFlag>;


/**
 * \brief Host data for nodes
 *
 * Stores the parent BVH node so that the node can be quickly detached
 * from it if necessary, as well as information on the state of the node.
 */
struct GfxSceneNodeHostData {
  /** Dirty flags for the node, including BVH-specific flags so that
   *  double tracking is avoided. If the BVH node structure is dirty,
   *  all chained nodes must be updated as well. */
  AtomicFlags<GfxSceneNodeDirtyFlag> dirtyFlags = { 0u };
  /** Parent BVH node. May point to a chained node. */
  GfxSceneNodeRef parentBvhNode = { };
  /** Child index within the parent BVH or chained node. Can be
   *  used to quickly remove the node from its parent. */
  uint32_t childIndex = 0u;
  /** Maximum depth for BVH nodes. Used to determine how many times
   *  to dispatch the BVH traversal shader. This will generally only
   *  increase when other BVH nodes are attached as child nodes, so
   *  this is an upper bound. */
  uint32_t childDepth = 0u;
};


/**
 * \brief Bounding volume node
 *
 * Stores a bounding box for coarse culling, as well as a list of
 * child nodes, which can be either futher BVH nodes, or renderable
 * nodes such as geometry instances or lights.
 */
struct GfxSceneBvhInfo {
  /** Maximum number of child nodes per BVH node */
  constexpr static uint32_t MaxChildCount = 26;

  /** Node index of where the transform is stored. */
  int32_t nodeIndex;
  /** Axis-aligned bounding box, relative to the node. If empty,
   *  no culling will be performed and this is always considered
   *  visible. */
  GfxAabb<float16_t> aabb;
  /** Maximum view distance. If all relevant views are further away
   *  from the center of the bounding volume than this, the entire
   *  node including all its children will be culled from all render
   *  passes. If this is 0, the effective view distance is infinite. */
  float16_t maxDistance;
  /** Number of child nodes for this BVH node. Note that this does not
   *  include the number of child nodes in any chained BVH node. */
  uint16_t childCount;
  /** Chained node. Points to an optional \c GfxSceneBvhInfo structure
   *  stored within the BVH node array, which only contains a list of
   *  additional child nodes. No node is chained if this is negative. */
  GfxSceneNodeRef chainedBvh;
  /** Array of child nodes for this BVH node. This is a fixed-size
   *  array in order to keep the data structure reasonably simple.
   *  If a BVH node has more than the maximum number of children,
   *  chained nodes must be used instead. */
  std::array<uint32_t, MaxChildCount> childNodes;
};

static_assert(sizeof(GfxSceneBvhInfo) == 128);


/**
 * \brief Bounding volume visibility info
 *
 * Stores persistent visibility information for a BVH node for a render
 * pass group. Should be packed into an array that can be indexed via
 * the BVH node index.
 */
struct GfxSceneBvhVisibility {
  /** Bit mask of passes for which the occlusion test has been performed
   *  in the previous frame. The node must be considered visible for any
   *  pass that has no valid occlusion test data. */
  uint32_t prevFrameOcclusionTestPerformedMask;
  /** Bit mask of passes that passed the occlusion test in the previous
   *  frame. If this is 0 for any valid pass, the node is not considered
   *  visible for that pass, but the occlusion test must be performed
   *  regardless so that the node can become visible again. */
  uint32_t prevFrameOcclusionTestPassedMask;
};

static_assert(sizeof(GfxSceneBvhVisibility) == 8);


/**
 * \brief Parent node info for nodes attached to a BVH node
 */
struct GfxSceneBvhParent {
  /** BVH node index. If negative, the node is not attached to
   *  any BVH node, which should only happen for top-level BVH
   *  nodes. */
  int32_t bvhNode = -1;
  /** Child index within the BVH node. This is useful to allow
   *  removing the node from its parent without scanning the
   *  entire child node array. */
  uint32_t childIndex = 0;
};


/**
 * \brief BVH node link info
 *
 * Stores info about the parent node as well as chained nodes.
 * This is mostly useful when manipulating the BVH structure
 * on the CPU.
 */
struct GfxSceneBvhLink {
  /** Parent node and child index. */
  GfxSceneBvhParent parent = { };
  /** Layer index, i.e. how deeply this node is nested in the hierarchy. Will be
   *  the layer index of the parent plus one, or 0 if the node has no parent.
   *  Used to keep track of the overall tree depth. */
  uint32_t layer = 0;
  /** Frame ID of when the BVH node has last been updated. */
  std::atomic<uint32_t> updateFrameId = { 0u };
};


/**
 * \brief BVH node description
 */
struct GfxSceneBvhDesc {
  /** Index of the transform node for this BVH node. The
   *  index is immutable and must be allocated beforehand. */
  uint32_t nodeIndex = 0;
  /** Axis-aligned bounding box, relative to the node transform. */
  GfxAabb<float16_t> aabb = { };
  /** Maximum view distance, or \c 0 for infinity. */
  float16_t maxDistance = 0.0_f16;
};


/**
 * \brief Scene buffer header
 *
 * Stores the data layout of the scene buffer.
 */
struct GfxSceneNodeHeader {
  /** Offset of node infos in bytes, relative to the start of the buffer.
   *  Points to an array of \c GfxSceneNodeInfo structures. */
  uint32_t nodeParameterOffset;
  /** Offset of absolute node transforms in bytes, relative to the start
   *  of the scene buffer. Points to an array of \c GfxSceneNodeTransform,
   *  with two transform entires per node. This is double-buffered in order
   *  to support motion vectors for dynamic instances. */
  uint32_t nodeTransformOffset;
  /** Maximum number of nodes in the buffer. Can be applied as an offset
   *  when indexing into double-buffered node transform arrays. */
  uint32_t nodeCount;
  /** Offset of BVH infos in bytes, relative to the start of the buffer.
   *  Points to an array of \c GfxSceneBvhInfo structures. */
  uint32_t bvhOffset;
  /** Maximum number of BVH nodes in the buffer. */
  uint32_t bvhCount;
};

static_assert(sizeof(GfxSceneNodeHeader) == 20);


/**
 * \brief Scene buffer description
 *
 * Stores capacities for all supported node types, which are
 * used to compute the buffer size and layout.
 */
struct GfxSceneNodeBufferDesc {
  /** Total number of generic nodes */
  uint32_t nodeCount = 0u;
  /** Total number of BVH nodes */
  uint32_t bvhCount = 0u;
};


/**
 * \brief Scene buffer
 *
 * Manages a GPU buffer that stores a representation of all resident
 * nodes in the scene. Note that this class does not manage any node
 * data itself, but provides helpers to update and upload nodes.
 */
class GfxSceneNodeBuffer {

public:

  explicit GfxSceneNodeBuffer(
          GfxDevice                     device);

  ~GfxSceneNodeBuffer();

  GfxSceneNodeBuffer             (const GfxSceneNodeBuffer&) = delete;
  GfxSceneNodeBuffer& operator = (const GfxSceneNodeBuffer&) = delete;

  /**
   * \brief Queries GPU address
   * \returns Buffer GPU address
   */
  uint64_t getGpuAddress() const {
    return m_buffer ? m_buffer->getGpuAddress() : 0ull;
  }

  /**
   * \brief Scene buffer header
   *
   * Valid after resizing the buffer. Useful to retrieve offsets of
   * various data arrays within the buffer.
   * \returns Scene buffer header
   */
  GfxSceneNodeHeader getHeader() const {
    return m_header;
  }

  /**
   * \brief Resizes buffer, preserving buffer contents
   *
   * The buffer must be ready to be used with transfer operations in
   * case a resize operation happens. If the buffer gets recreated,
   * previous buffer contents will be copied to the new buffer.
   * \param [in] context Context to use to perform copies on
   * \param [in] desc New buffer description
   * \returns Previous buffer if the buffer was replaced, which then
   *    must be kept alive until the current frame has completed. If
   *    the buffer was not replaced, \c nullptr will be returned.
   */
  GfxBuffer resizeBuffer(
    const GfxContext&                   context,
    const GfxSceneNodeBufferDesc&       desc);

private:

  GfxDevice                   m_device;
  GfxBuffer                   m_buffer;

  GfxSceneNodeHeader          m_header = { };
  GfxSceneNodeBufferDesc      m_desc;

  uint32_t                    m_version = 0u;

  static uint32_t allocStorage(
          uint32_t&                     allocator,
          size_t                        size);

};


/**
 * \brief Pass group parameters for BVH traversal
 */
struct GfxScenePassGroupInfo {
  /** Virtual address of where render pass
   *  parameters are stored. */
  uint64_t passBufferVa = 0ull;
  /** Number of root nodes for the pass group. */
  uint32_t rootNodeCount = 0u;
  /** Pointer to root node references. */
  const GfxSceneNodeRef* rootNodes = nullptr;
};


/**
 * \brief Node manager
 *
 * Manages GPU resources for plain node data as well as the BVH,
 * and provides methods to manipulate the node hierarchy.
 */
class GfxSceneNodeManager {

public:

  explicit GfxSceneNodeManager(
          GfxDevice                     device);

  ~GfxSceneNodeManager();

  /**
   * \brief Queries scene buffer address
   *
   * Needed for various scene processing steps.
   * \returns Scene buffer address
   */
  uint64_t getGpuAddress() const {
    return m_gpuResources.getGpuAddress();
  }

  /**
   * \brief Queires node reference for node
   *
   * \param [in] node Node index
   * \returns Typed node reference
   */
  GfxSceneNodeRef getNodeReference(
          uint32_t                      node) const {
    return m_nodeData[node].nodeRef;
  }

  /**
   * \brief Queries node index for node reference
   *
   * \param [in] reference Node reference. This \e must be a valid
   *    reference that is assigned to a node, otherwise calling
   *    this will result in undefined behaviour.
   * \returns Plain node index
   */
  uint32_t getNodeIndex(
          GfxSceneNodeRef               reference) const {
    return m_nodeMap[uint32_t(reference.type)][uint32_t(reference.index)];
  }

  /**
   * \brief Allocates a new node
   *
   * This only allocates a node and initializes it with an identity
   * transform, and otherwise no special parameters. The intention
   * is that callers will allocate the node, create the typed node
   * using the returned node index, and then update node properties
   * with calls to the various update methods.
   * \returns Index of the newly created node
   */
  uint32_t createNode();

  /**
   * \brief Destroys a node
   *
   * Frees a node, so that the same node index will become available
   * again to node allocations later. Must be called in tandem with
   * the typed node destruction method, or there will be stale node
   * pointers.
   *
   * This will also implicitly remove the node from any BVH
   * node's child node list.
   * \param [in] index Node index
   * \param [in] frameId Current frame ID
   */
  void destroyNode(
          uint32_t                      index,
          uint32_t                      frameId);

  /**
   * \brief Updates node reference
   *
   * Stores the node type and typed index of the node for the
   * purpose of creating typed node lists during BVH traversal.
   * \param [in] index Node index to update
   * \param [in] reference Typed node reference
   */
  void updateNodeReference(
          uint32_t                      index,
          GfxSceneNodeRef               reference);

  /**
   * \brief Updates parent node and transform
   *
   * Requires that the parent node is up to date with node references.
   * Note that this is not necessarily the same node as the parent BVH.
   * \param [in] index Node index to update
   * \param [in] parent Parent node reference
   * \param [in] transform Parent transform index, or -1 if
   *    the parent node's transform should be used directly.
   */
  void updateNodeParent(
          uint32_t                      index,
          int32_t                       parent,
          int32_t                       transform);

  /**
   * \brief Updates node transform
   *
   * The transform is relative to the parent node. Absolute
   * transforms will be updated during BVH traversal.
   * \param [in] index Node index to update
   * \param [in] transform New relative transform
   */
  void updateNodeTransform(
          uint32_t                      index,
    const QuatTransform&                transform);

  /**
   * \brief Creates a BVH node
   *
   * Creates a new BVH node for an existing node index.
   * \param [in] desc BVH node description
   * \returns Node reference for the new BVH node.
   */
  GfxSceneNodeRef createBvhNode(
    const GfxSceneBvhDesc&              desc);

  /**
   * \brief Destroys a BVH node
   *
   * All nodes that are attached as child nodes to
   * the given BVH node will be orphaned.
   * \param [in] reference Node reference
   * \param [in] frameId Current frame ID
   */
  void destroyBvhNode(
          GfxSceneNodeRef               reference,
          uint32_t                      frameId);

  /**
   * \brief Updates BVH bounding volume
   *
   * \param [in] reference BVH node reference
   * \param [in] aabb New bounding box
   * \param [in] maxDistance New maximum distance
   */
  void updateBvhVolume(
          GfxSceneNodeRef               reference,
    const GfxAabb<float16_t>&           aabb,
          float16_t                     maxDistance);

  /**
   * \brief Attaches a set of nodes to a BVH
   *
   * Implicitly detaches the nodes from their current parent BVH node
   * as well. Note that a node's parent BVH is not necessarily the same
   * node as its actual parent node, since the latter only applies to
   * transforms. The BVH structure is flatter in that sense.
   *
   * Node chains are also managed automatically in that additional BVH
   * node objects will automatically be allocated whenever the maximum
   * number of children for the given target BVH node is exceeded.
   *
   * Note that this method takes a global lock and is not intended to
   * be used from multiple threads concurrently due to its complexity.
   * \param [in] target Target BVH node. In order to orphan the given
   *    list of nodes, pass a default-initialized node reference with
   *    a node type of \c eNone.
   * \param [in] nodeCount Number of nodes
   * \param [in] nodes Node references
   */
  void attachNodesToBvh(
          GfxSceneNodeRef               target,
          uint32_t                      nodeCount,
    const GfxSceneNodeRef*              nodes);

  /**
   * \brief Commits pending updates
   *
   * This method must be called once at the start of a frame. The node
   * buffers must be ready to be used with transfer and shader storage
   * operations. Also ensures constant time access to node objects.
   * \param [in] context Context object
   * \param [in] pipelines Update pipelines
   * \param [in] currFrameId Current frame ID
   * \param [in] lastFrameId Last completed frame ID
   */
  void commitUpdates(
    const GfxContext&                   context,
    const GfxScenePipelines&            pipelines,
          uint32_t                      currFrameId,
          uint32_t                      lastFrameId);

  /**
   * \brief Traverses BVH for one or more pass groups
   *
   * Dispatches compute workgroups for BVH traversal for the given
   * set of set of pass groups. Traversing for multiple pass groups
   * at once may be useful in case disjoint parts of the world need
   * to be rendered at once, without interfering each other, and
   * passing multiple groups in one call will interleave dispatches
   * with no data dependency to avoid unnecessary memory barriers.
   * \param [in] context Context object
   * \param [in] pipelines Update pipelines
   * \param [in] groupBuffer Pass group buffer
   * \param [in] groupInfo Pass group parameters
   * \param [in] frameId Current frame ID
   * \param [in] referencePass Pass index used for distance culling
   *    and LOD selection. This should generally refer to the main
   *    render pass, except when rendering static shadow maps.
   */
  void traverseBvh(
    const GfxContext&                   context,
    const GfxScenePipelines&            pipelines,
    const GfxScenePassGroupBuffer&      groupBuffer,
    const GfxScenePassGroupInfo&        groupInfo,
          uint32_t                      frameId,
          uint32_t                      referencePass);

private:

  GfxSceneNodeBuffer                  m_gpuResources;

  std::unordered_map<
    uint32_t, GfxBuffer>              m_gpuBuffers;

  alignas(CacheLineSize)
  std::mutex                          m_nodeMutex;
  ObjectMap<GfxSceneNodeInfo>         m_nodeData;
  ObjectMap<GfxSceneNodeHostData>     m_hostData;
  ObjectMap<GfxSceneBvhInfo>          m_bvhData;

  std::array<ObjectMap<uint32_t>,
    uint8_t(GfxSceneNodeType::eCount)> m_nodeMap;

  ObjectAllocator                     m_nodeAllocator;
  ObjectAllocator                     m_bvhAllocator;

  std::vector<GfxSceneUploadChunk>    m_uploadChunks;

  alignas(CacheLineSize)
  LockFreeGrowList<uint32_t>          m_dirtyNodes;
  LockFreeGrowList<uint32_t>          m_dirtyBvhs;

  alignas(CacheLineSize)
  std::mutex                          m_freeMutex;
  std::unordered_multimap<
    uint32_t, GfxSceneNodeRef>        m_freeNodeQueue;

  void markDirty(
          uint32_t                      index,
          GfxSceneNodeDirtyFlags        flags);

  void addDirtyNode(
          uint32_t                      index);

  void addDirtyBvh(
          uint32_t                      index);

  void updateBufferData(
    const GfxContext&                   context,
    const GfxScenePipelines&            pipelines,
          uint32_t                      frameId);

  void cleanupNodes(
          uint32_t                      frameId);

  void cleanupGpuBuffers(
          uint32_t                      frameId);

  void compactBvhChain(
          GfxSceneNodeRef               bvh,
          uint32_t                      frameId);

  void resizeGpuBuffer(
    const GfxContext&                   context,
          uint32_t                      frameId);

  void removeFromNodeMap(
          GfxSceneNodeRef               reference);

  void insertIntoNodeMap(
          GfxSceneNodeRef               reference,
          uint32_t                      index);

};

}
