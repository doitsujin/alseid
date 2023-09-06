#pragma once

#include "../../util/util_quaternion.h"
#include "../../util/util_small_vector.h"
#include "../../util/util_types.h"

#include "../gfx.h"
#include "../gfx_geometry.h"
#include "../gfx_types.h"

#include "gfx_scene_draw.h"

namespace as {

/**
 * \brief Node type
 */
enum class GfxSceneNodeType : uint8_t {
  /** Abstract node. The value of this must not change,
   *  since node references may be zero-initialized. */
  eNone             = 0,
  /** BVH node. The value of this must not be changed,
   *  since  */
  eBvh              = 1,

  /** Instance node. */
  eInstance         = 2,
  /** Light node. */
  eLight            = 3,
  /** Reflection probe. */
  eReflectionProbe  = 4,
  /** First custom node type. */
  eFirstCustom      = 16,

  /** Number of special node types. */
  eBuiltInCount     = 2,

  /** Maximum number of different node types. Used
   *  to determine the size of some lookup tables. */
  eCount            = 32 + uint8_t(eBuiltInCount)
};


/**
 * \brief Node reference
 *
 * Defines the type of a node, as well as the type-specific index
 * of that node which defines where type-specific data for that
 * node is stored, including the original scene node index.
 */
struct GfxSceneNodeRef {
  /** Node type. */
  GfxSceneNodeType type;
  /** Index into the typed node array. Not the scene node index. */
  uint24_t index;
};

static_assert(sizeof(GfxSceneNodeRef) == 4);


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
 * \brief Bounding volume node
 *
 * Stores a bounding box for coarse culling, as well as a list of
 * child nodes, which can be either futher BVH nodes, or renderable
 * nodes such as geometry instances or lights.
 */
struct GfxSceneBvhInfo {
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
  /** Chained node. Points to an optional \c GfxSceneBvhChain structure
   *  stored within the BVH node array, which only contains a list of
   *  additional child nodes. No node is chained if this is negative. */
  int32_t chainedNode;
  /** Array of child nodes for this BVH node. This is a fixed-size
   *  array in order to keep the data structure reasonably simple.
   *  If a BVH node has more than the maximum number of children,
   *  chained nodes must be used instead. */
  std::array<uint32_t, 26> childNodes;
};

static_assert(sizeof(GfxSceneBvhInfo) == 128);


/**
 * \brief Bounding volume chain
 *
 * Designed to be the same size as the \c GfxSceneBvhInfo struct
 * so that they can be aliased within the same memory. The number
 * of child nodes is higher here, which can increase parallelism.
 */
struct GfxSceneBvhChain {
  /** Number of child nodes in this chain node. */
  uint32_t childCount;
  /** Index of next chained node. */
  int32_t chainedNode;
  /** Child node index array. */
  std::array<uint32_t, 30> childNodes;
};

static_assert(sizeof(GfxSceneBvhChain) == sizeof(GfxSceneBvhInfo));


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
 * \brief Node residency flags
 */
enum class GfxSceneNodeResidencyFlag : uint8_t {
  /** Node is partially resident. This means that all required resources for the
   *  node are available, but not necessarily at the highest level of detail. */
  eStatusPartial  = (1u << 0),
  /** Node is fully resident. This means that all resources used by the
   *  node are available with the highest possible level of detail. */
  eStatusFull     = (1u << 1),
  /** A stream request has been submitted for this node. */
  eRequestStream  = (1u << 2),
  /** An eviction request has been submitted for this node. */
  eRequestEvict   = (1u << 3),

  eFlagEnum       = 0u
};

using GfxSceneNodeResidencyFlags = Flags<GfxSceneNodeResidencyFlag>;


/**
 * \brief Scene buffer header
 *
 * Stores the data layout of the scene buffer.
 */
struct GfxSceneHeader {
  /** Offset of node infos in bytes, relative to the start of the buffer.
   *  Points to an array of \c GfxSceneNodeInfo structures. */
  uint32_t nodeParameterOffset;
  /** Offset of absolute node transforms in bytes, relative to the start
   *  of the scene buffer. Points to an array of \c GfxSceneNodeTransform. */
  uint32_t nodeTransformOffset;
  /** Offset of the node residency status array. Points to an array of bytes
   *  that store each node's current residency and stream request status. */
  uint32_t nodeResidencyOffset;
  /** Offset of BVH infos in bytes, relative to the start of the buffer.
   *  Points to an array of \c GfxSceneBvhInfo structures. */
  uint32_t bvhOffset;
};

static_assert(sizeof(GfxSceneHeader) == 16);


/**
 * \brief Scene buffer description
 *
 * Stores capacities for all supported node types, which are
 * used to compute the buffer size and layout.
 */
struct GfxSceneBufferDesc {
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
class GfxSceneBuffer {

public:

  explicit GfxSceneBuffer(
          GfxDevice                     device);

  ~GfxSceneBuffer();

  GfxSceneBuffer             (const GfxSceneBuffer&) = delete;
  GfxSceneBuffer& operator = (const GfxSceneBuffer&) = delete;

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
  GfxSceneHeader getHeader() const {
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
    const GfxSceneBufferDesc&           desc);

private:

  GfxDevice                   m_device;
  GfxBuffer                   m_buffer;

  GfxSceneHeader              m_header = { };
  GfxSceneBufferDesc          m_desc;

  uint32_t                    m_version = 0u;

  static uint32_t allocStorage(
          uint32_t&                     allocator,
          size_t                        size);

};

}
