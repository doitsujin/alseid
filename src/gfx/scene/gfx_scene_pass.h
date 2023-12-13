#pragma once

#include "gfx_scene_node.h"

namespace as {

/**
 * \brief Maximum number of passes per pass group
 *
 * Chosen to allow bit masks to be used for visibility status.
 */
constexpr uint32_t GfxMaxPassesPerGroup = 32u;


/**
 * \brief Render pass type
 */
enum class GfxScenePassType : uint16_t {
  /** Flat render pass, with a defined view frustum. Used for
   *  regular rendering and directional shadows. */
  eFlat   = 0u,
  /** Flat render pass that mirrors around a plane in world space. */
  eMirror = 1u,
  /** Cube map render pass. Used for shadow maps for point lights,
   *  reflection probes, and similar passes. */
  eCube   = 1u,
};


/**
 * \brief Render pass flags
 */
enum class GfxScenePassFlag : uint16_t {
  /** Indicates that the pass uses lighting, and that a light list
   *  should be generated for any pass group containing this pass. */
  eEnableLighting       = (1u << 0),
  /** Indicates that frustum culling should be performed on nodes
   *  during this pass. Should be enabled for flat and mirror passes. */
  ePerformFrustumTest   = (1u << 1),
  /** Indicates that occlusion testing will be performed for this pass.
   *  If set, occlusion testing \e must be performed, or objects will
   *  remain invisible. */
  ePerformOcclusionTest = (1u << 2),
  /** Indicates that occlusion test results from the previous frame
   *  should be ignored. This should be set if the view has changed
   *  significantly, e.g. after a camera cut, to avoid glitches. */
  eIgnoreOcclusionTest  = (1u << 3),

  eFlagEnum = 0
};

using GfxScenePassFlags = Flags<GfxScenePassFlag>;


/**
 * \brief Render pass info
 *
 * Stores properties of a single render pass. Render passes must be
 * preprocessed every frame in order to resolve relative transforms.
 */
struct GfxScenePassInfo {
  /** Projection for the render pass. */
  Projection projection;
  /** Rotation component of the world to view space transform. For
   *  cube map passes, this must be an identity quaternion. */
  Vector4D viewSpaceRotation;
  /** Translation component of the world to view space transform. */
  Vector3D viewSpaceTranslation;
  /** Render pass type */
  GfxScenePassType type;
  /** Render pass flags */
  GfxScenePassFlags flags;
  /** Render pass index used for distance culling and LOD selection.
   *  This is relevant for shadow mapping, since rendering must be
   *  consistent between the main pass and shadow passes. */
  uint32_t distanceCullingPass;
  /** Maximum view distance of this render pass. Useful for shadow
   *  maps of point lights since the number of instances contributing
   *  to visible shadows is limited by the light's maximum range.
   *  A value of 0 or less indicates an infinite view distance. */
  float viewDistanceLimit;
  /** Distance scaling factor. Can be set to change the effective view
   *  distance of instances and BVH nodes. Values lower than 1 will
   *  increase the view distance, values greater than 1 will decrease it. */
  float viewDistanceScale;
  /** Distance scaling factor for LOD selection. Can be used to render
   *  instances at a lower or higher level of detail without affecting
   *  the maximum view distance. */
  float lodDistanceScale;
  /** Mirror plane, in view space. */
  Vector4D mirrorPlane;

  union {
    /** View-space frustum planes for flat render passes. Generally
     *  computed directly from the projection. */
    ViewFrustum frustum;
    /** Rotation transforms for each face of a cube render pass. */
    std::array<Vector4D, 6> faceRotations;
  };
};

static_assert(sizeof(GfxScenePassInfo) == 176);


/**
 * \brief Set of node list offsets
 */
struct GfxScenePassTypedNodeListOffsets {
  /** Offset to typed visible node list. */
  uint32_t nodeList;
  /** Offset to typed update node list, in case preprocessing
   *  is required for the given node type. */
  uint32_t updateList;
};


/**
 * \brief Render pass group buffer header
 *
 * Stores a set of render passes to process in a pass group,
 * as well as offsets to various parts of the buffer.
 */
struct GfxScenePassGroupHeader {
  /** Number of render passes in this pass group. */
  uint32_t passCount;
  /** Bit mask of passes that should ignore the previous frame's occlusion
   *  test results. Useful if the render pass has changed entirely, or if
   *  a camera cut has occured. */
  uint32_t ignoreOcclusionTestMask;
  /** Offset of the BVH node list buffer, relative to the start of the header.
   *  This buffer contains two sets of indirect dispatch parameters that the
   *  traversal shader will ping-pong between during BVH traversal. Must be
   *  initialized with the BVH root nodes prior to traversal. */
  uint32_t bvhListOffset;
  /** Offset of persistent BVH visibility buffer. Stores masks of which
   *  passes performed and passed occlusion testing for any given BVH node. */
  uint32_t bvhVisibilityOffset;
  /** Render pass indices. */
  std::array<uint16_t, GfxMaxPassesPerGroup> passes;
  /** Offset of each set of typed node lists, except for BVHs and abstract nodes. */
  std::array<GfxScenePassTypedNodeListOffsets,
    size_t(GfxSceneNodeType::eCount) - size_t(GfxSceneNodeType::eBuiltInCount)> listOffsets;
};

static_assert(sizeof(GfxScenePassGroupHeader) == 320);


/**
 * \brief Dispatch arguments for BVH nodes within a pass group
 */
struct GfxSceneBvhListArgs {
  /** Indirect dispatch parameters for BVH traversal */
  GfxDispatchArgs dispatchTraverse;
  /** Indirect dispatch parameters for resetting the
   *  next set of dispatch arguments to 0 */
  GfxDispatchArgs dispatchReset;
  /** Number of list entries */
  uint32_t entryCount;
  /** Index of the first entry within the node list */
  uint32_t entryIndex;
};

static_assert(sizeof(GfxSceneBvhListArgs) == 32);


/**
 * \brief List header for BVH nodes within a pass group
 */
struct GfxSceneBvhListHeader {
  /** Total number of BVH nodes in the list. Must be initialized to the number
   *  of root BVH nodes when the first set of node parameters is written. */
  uint32_t totalNodeCount;
  /** Double-buffered dispatch arguments for BVH traversal. The traversal shader
   *  will consume one list and generate another, as well as increment the total
   *  node count so it can be used to generate occlusion test draws. */
  std::array<GfxSceneBvhListArgs, 2> args;
};

static_assert(sizeof(GfxSceneBvhListHeader) == 68);


/**
 * \brief Pass group buffer description
 *
 * Provides capacities for the various different node types
 * that need to be stored in the pass group buffer.
 */
struct GfxScenePassGroupBufferDesc {
  /** Maximum number of nodes for each type. Nodes used in rendering
   *  will be distributed to one list per node type. */
  std::array<uint32_t, size_t(GfxSceneNodeType::eCount)> maxNodeCounts = { };
};


/**
 * \brief Pass group buffer
 *
 * Manages a buffer object for a single group of render passes. Since this has
 * to account for every single instance potentially being visible, applications
 * should try to reduce the number of pass groups as much as possible.
 */
class GfxScenePassGroupBuffer {

public:

  /**
   * \brief Creates pass group buffer
   *
   * Initializes the instance with no actual buffer object attached to it.
   * In order to use the buffer for rendering, it must first be resized.
   * \param [in] device Device object
   */
  explicit GfxScenePassGroupBuffer(
          GfxDevice                     device);

  ~GfxScenePassGroupBuffer();

  GfxScenePassGroupBuffer             (const GfxScenePassGroupBuffer&) = delete;
  GfxScenePassGroupBuffer& operator = (const GfxScenePassGroupBuffer&) = delete;

  /**
   * \brief Queries GPU address
   * \returns Buffer GPU address
   */
  uint64_t getGpuAddress() const {
    return m_buffer ? m_buffer->getGpuAddress() : 0ull;
  }

  /**
   * \brief Queries indirect dispatch descriptor for BVH traversal
   *
   * \param [in] bvhLayer BVH layer index
   * \param [in] traverse Whether to return the traversal
   *    argument descriptor or the reset argument descriptor.
   * \returns BVH traversal dispatch descriptor
   */
  GfxDescriptor getBvhDispatchDescriptor(
          uint32_t                      bvhLayer,
          bool                          traverse) const;

  /**
   * \brief Queries indirect dispatch descriptors for list processing
   *
   * \param [in] nodeType Node type. Must not be a built-in type.
   * \returns Pair of dispatch descriptors, the first one for node
   *    list traversal, the second one for node update list traversal.
   */
  std::pair<GfxDescriptor, GfxDescriptor> getDispatchDescriptors(
          GfxSceneNodeType              nodeType) const;

  /**
   * \brief Sets passes for the given pass group
   *
   * Passes \e must be valid and unique. Invalidates the header,
   * so it \e must be updated after changing the render pass list.
   * \param [in] passCount Number of passes
   * \param [in] passIndices Pass indices
   */
  void setPasses(
          uint32_t                      passCount,
    const uint16_t*                     passIndices);

  /**
   * \brief Updates the buffer
   *
   * Writes any changes to the render pass list to the GPU buffer. This
   * is a relatively cheap operation, and should be called once per frame.
   * \param [in] context Context object to record the copy operation on.
   *    Expects the buffer to be ready for \c GfxUsage::eTransferDst usage.
   */
  void updateBuffer(
    const GfxContext&                   context);

  /**
   * \brief Resizes and replaces buffer
   *
   * Must only be called at the start of a frame. If the buffer has to be
   * replaced due to being too small, the previous buffer object will be
   * returned, and must be tracked until all previous frames have completed.
   *
   * In general, this will attempt to choose a buffer size in such a way to
   * avoid having to frequently recreate the buffer. Thus, unless the buffer
   * reaches a very large size, it will likely not be shrunk if the number
   * of nodes ever decreases.
   *
   * After calling this, the buffer header must unconditionally be updated.
   * If the internal buffer layout changes, occlusion test results from
   * previous frames will be discarded to avoid rendering glitches.
   * \param [in] desc Description with the maximum node count
   * \returns The old buffer if the buffer has been replaced, or \c nullptr.
   */
  GfxBuffer resizeBuffer(
    const GfxScenePassGroupBufferDesc&  desc);

private:

  GfxDevice                   m_device;
  GfxBuffer                   m_buffer;

  GfxScenePassGroupBufferDesc m_desc = { };
  GfxScenePassGroupHeader     m_header = { };

  uint32_t                    m_version = 0u;
  bool                        m_doClear = false;

  static uint32_t allocStorage(
          uint32_t&                     allocator,
          size_t                        size);

};

}
