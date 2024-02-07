#pragma once

#include "../common/gfx_common_hiz.h"

#include "gfx_scene_node.h"
#include "gfx_scene_pipelines.h"

namespace as {

class GfxSceneInstanceManager;
class GfxScenePassManager;

/**
 * \brief Maximum number of passes per pass group
 *
 * Chosen to allow bit masks to be used for visibility status.
 */
constexpr uint32_t GfxMaxPassesPerGroup = 32u;


/**
 * \brief Render pass type flags
 *
 * Used to look up material pipelines during rendering. If a material
 * does not have a pipeline set for a given pass type, rendering will
 * be skipped.
 */
enum class GfxScenePassType : uint32_t {
  /** Depth pre-pass for the main render pass. Generally expected
   *  to have no fragment shader, unless alpha testing is required. */
  eMainDepth              = (1u << 0),
  /** Main opaque color pass. Should write all render targets. The
   *  fragment shader should not discard to enable early Z. */
  eMainOpaque             = (1u << 1),
  /** Main transparency pass. This will be run after the opaque pass, and
   *  the fragment shader must handle layered transparency manually. */
  eMainTransparency       = (1u << 2),
  /** Depth pre-pass for secondary render targets, such as reflection
   *  probes, planar reflections, or just secondary passes in general. */
  eOtherDepth             = (1u << 3),
  /** Opaque color pass for secondary render targets. This should use a
   *  simplified lighting model for performance reasons. */
  eOtherOpaque            = (1u << 4),
  /** Transparency pass for secondary render passes. Uses alpha blending
   *  to resolve transparency, and does not support refraction. */
  eOtherTransparency      = (1u << 5),
  /** Render pass for shadow map rendering of all kinds. Again expected
   *  to have no fragment shader unless alpha testing is required, but
   *  the alpha testing logic itself may be different. */
  eShadow                 = (1u << 6),

  eFlagEnum               = 0u
};

using GfxScenePassTypeFlags = Flags<GfxScenePassType>;


/**
 * \brief Render pass flags
 *
 * Flags that do not need to be known for anything other than
 * render pass buffer updates are in the lower 16 bits.
 */
enum class GfxScenePassFlag : uint32_t {
  /** Indicates that the render pass renders a cube map. */
  eIsCubeMap              = (1u << 0),
  /** Indicates that the render pass uses a mirror plane. */
  eUsesMirrorPlane        = (1u << 1),
  /** Indicates that the render pass uses a custom viewport region.
   *  If not set, the viewport region must be ignored. */
  eUsesViewportRegion     = (1u << 2),

  /** Indicates that the pass uses lighting, and that a light list
   *  should be generated for any pass group containing this pass. */
  eEnableLighting         = (1u << 6),

  /** Indicates that occlusion testing will be performed for this pass.
   *  If set, occlusion testing \e must be performed, or objects will
   *  remain invisible. */
  ePerformOcclusionTest   = (1u << 8),
  /** Indicates that occlusion test results from the previous frame
   *  should be ignored. This should be set if the view has changed
   *  significantly, e.g. after a camera cut, to avoid glitches. */
  eIgnoreOcclusionTest    = (1u << 9),

  /** Indicates that pass metadata has already been set and should
   *  be ignored during host updates. This includes most pass flags. */
  eKeepMetadata           = (1u << 16),
  /** Indicates that the projecction is provided by the GPU and should
   *  be ignored during host updates. */
  eKeepProjection         = (1u << 17),
  /** Indicates that the view transform is provided by the GPU
   *  and should be ignored during host updates. */
  eKeepViewTransform      = (1u << 18),
  /** Indicates that the mirror plane is provided by the GPU
   *  and should be ignored during host updates. */
  eKeepMirrorPlane        = (1u << 19),
  /** Indicates that the viewport index and layer index are provided
   *  by the GPU and should be ignored during host updates. */
  eKeepViewportLayerIndex = (1u << 20),
  /** Indicates that the viewport region is provided by the GPU and
   *  should be ignored during host updates. */
  eKeepViewportRegion     = (1u << 21),
  /** Indicates that view distance properties are provided by the GPU
   *  and should be ignored during host updates. */
  eKeepViewDistance       = (1u << 22),

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
  /** Render pass flags */
  GfxScenePassFlags flags;
  /** Application-defined pass types that this pass can be used for.
   *  Can be used to filter out certain instances during rendering. */
  uint32_t passTypeMask;
  /** Frame ID of when the pass has last been updated. */
  uint32_t dirtyFrameId;
  /** Frame ID of when pass updates have been committed. */
  uint32_t updateFrameId;
  /** Projection for the render pass. */
  Projection projection;
  /** View transform, either from world space coordinates, or relative
   *  to a given node. Only used when updating the pass. */
  QuatTransform viewTransform;
  /** View space transform for the current frame. */
  QuatTransform currTransform;
  /** View space transform for the previous frame. Should only be
   *  used to compute motion vectors. */
  QuatTransform prevTransform;
  /** Mirror plane, in world space or relative to a given node. */
  Vector4D mirrorPlane;
  /** Transformed mirror plane, in view space. May also be used as a clipping
   *  plane when rendering planar reflections, and must be processed by the
   *  mesh shader right before applying the projection. */
  Vector4D currMirrorPlane;
  /** Mirror plane of the previous frame. Used to compute motion vectors. */
  Vector4D prevMirrorPlane;
  /** Node index for the view transform. This can be used to attach the
   *  camera to a given node without further host intervention. If negative,
   *  the view transform will be used as-is. */
  int32_t cameraNode;
  /** Joint to attach the camera to. */
  int32_t cameraJoint;
  /** Node index for the mirror plane. Useful for rendering actual mirrors. */ 
  int32_t mirrorNode;
  /** Joint to attach the mirror plane to. */
  int32_t mirrorJoint;
  /** Maximum view distance of this render pass. Useful for shadow
   *  maps of point lights since the number of instances contributing
   *  to visible shadows is limited by the light's maximum range.
   *  A value of 0 or less indicates an infinite view distance. */
  float viewDistanceLimit;
  /** Distance scaling factor for LOD selection. Can be used to render
   *  instances at a lower or higher level of detail without affecting
   *  the maximum view distance. */
  float lodDistanceScale;
  /** First render target array layer to use for this render pass.
   *  It is up to the mesh shader to apply this correctly. This is
   *  primarily intended for use cases such as stereo 3D rendering. */
  uint32_t layerIndex;
  /** First hardware viewport to use for this render pass. It is up
   *  to the mesh shader to apply this correctly. */
  uint32_t viewportIndex;
  /** Virtual viewport region, in pixels. This is primarily intended for
   *  rendering large atlas textures when the exact regions to render are not
   *  known up front. Mesh shaders must apply the transform after applying
   *  the initial projection, and clip against the view frustum as necessary. */
  Offset2D viewportOffset;
  Extent2D viewportExtent;
  /** View-space frustum planes. Generally computed directly from the
   *  projection. For cube map passes, the global per-face rotation must
   *  be applied for culling purposes. */
  ViewFrustum frustum;
};

static_assert(sizeof(GfxScenePassInfo) == 320);


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
  /** Offset to BVH occlusion test data. */
  uint32_t bvhOcclusionTestOffset;
  /** Reserved for future use. */
  uint32_t reserved0;
  uint32_t reserved1;
  uint32_t reserved2;
  /** Render pass indices. */
  std::array<uint16_t, GfxMaxPassesPerGroup> passes;
  /** Offset of each set of typed node lists, except for BVHs and abstract nodes. */
  std::array<GfxScenePassTypedNodeListOffsets,
    size_t(GfxSceneNodeType::eCount) - size_t(GfxSceneNodeType::eBuiltInCount)> listOffsets;
};

static_assert(sizeof(GfxScenePassGroupHeader) == 336);


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
 * \brief List entry for BVH nodes within a pass group
 *
 * Stores the node reference, as well as a mask of passes for which the
 * bounding box of the BVH lies within the view frustum and view distance
 * limits.
 */
struct GfxSceneBvhListEntry {
  /** Node reference. */
  GfxSceneNodeRef nodeRef;
  /** Pass mask where the node is potentially visible. */
  uint32_t visibilityMask;
};

static_assert(sizeof(GfxSceneBvhListEntry) == 8);


/**
 * \brief Occlusion test header
 *
 * Stores info about a compute dispatch which uses the processed BVH
 * node list as an input, and is immediately followed by a node list.
 */
struct GfxSceneBvhOcclusionTestHeader {
  /** Compute shader dispatch arguments. Uses the BVH list. */
  GfxDispatchArgs csDispatch;
  /** Mesh shader dispatch arguments. Must be initialized to 0
   *  before dispatching the compute shader. */
  GfxDispatchArgs msDispatch;
  /** Number of nodes for the mesh shader to process */
  uint32_t        msNodeCount;
};

static_assert(sizeof(GfxSceneBvhOcclusionTestHeader) == 28);


/**
 * \brief Indirect dispatch descriptors for node processing
 */
struct GfxScenePassGroupDispatchDescriptors {
  /** Dispatch to process only newly added nodes since the last pass.
   *  For the first pass, this is equivalent to processing all nodes. */
  GfxDescriptor processNew;
  /** Dispatch to process all nodes in the list. Requires a barrier
   *  to use since this dispatch is set up after traversal. */
  GfxDescriptor processAll;
  /** Dispatch to process the update list generated by a prior
   *  processing pass. */
  GfxDescriptor update;
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
   * \returns Set of dispatch descriptors.
   */
  GfxScenePassGroupDispatchDescriptors getDispatchDescriptors(
          GfxSceneNodeType              nodeType) const;

  /**
   * \brief Queries occlusion test dispatch descriptor
   *
   * \param [in] stage Selects whether to return the compute
   *    shader or mesh shader descriptor.
   * \returns Descriptor for occlusion testing
   */
  GfxDescriptor getOcclusionTestDispatchDescriptor(
          GfxShaderStage                stage) const;

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
   * Writes any changes to the render pass list to the GPU buffer. Must
   * be called at the start of each frame. If the current buffer is too
   * small or if the data layout changes, any previous contents will be
   * discarded.
   * \param [in] context Context object to record the copy operation on.
   *    Expects the buffer to be ready for \c GfxUsage::eTransferDst usage.
   */
  void commitUpdates(
    const GfxContext&                   context,
    const GfxSceneNodeManager&          nodeManager);

  /**
   * \brief Resets update lists
   *
   * Dispatches a compute shader workgroup to reset update lists and
   * occlusion test node lists, and emits the necessary barrier. Must
   * be called between compute passes that access these lists.
   * \param [in] context Context object
   * \param [in] pipelines Update pipelines
   */
  void resetUpdateLists(
    const GfxContext&                   context,
    const GfxScenePipelines&            pipelines);

  /**
   * \brief Inserts a full compute barrier
   *
   * Waits for prior compute shader passes to complete and makes any
   * memory written by them visible to subsequent passes. Uses of this
   * should be kept to a minimum in order to allow parallel execution
   * of passes that do not have any data dependencies.
   * \param [in] context Context object
   */
  void passBarrier(
    const GfxContext&                   context);

  /**
   * \brief Performs instance-level visibility test for all passes
   *
   * Must be run after instance updates have completed.
   * \param [in] context Context object
   * \param [in] pipelines Update pipelines
   * \param [in] nodeManager Scene node manager
   * \param [in] instanceManager Scene instance manager
   * \param [in] passManager Render pass manager
   * \param [in] frameId Current frame ID
   */
  void cullInstances(
    const GfxContext&                   context,
    const GfxScenePipelines&            pipelines,
    const GfxSceneNodeManager&          nodeManager,
    const GfxSceneInstanceManager&      instanceManager,
    const GfxScenePassManager&          passManager,
          uint32_t                      frameId);

  /**
   * \brief Performs BVH occlusion testing for a given render pass
   *
   * Computes BVH visibility for the next frame, using the hierarchical
   * depth buffer as an input. Must be executed on the graphics queue.
   * \param [in] context Context object
   * \param [in] pipelines Update pipelines
   * \param [in] hizImage Hierarchical depth image
   * \param [in] nodeManager Scene node manager
   * \param [in] passManager Render pass manager
   * \param [in] passIndex Pass index within the pass group
   * \param [in] frameId Current frame ID
   */
  void performOcclusionTest(
    const GfxContext&                   context,
    const GfxScenePipelines&            pipelines,
    const GfxCommonHizImage&            hizImage,
    const GfxSceneNodeManager&          nodeManager,
    const GfxScenePassManager&          passManager,
          uint32_t                      passIndex,
          uint32_t                      frameId);

private:

  GfxDevice                   m_device;
  GfxBuffer                   m_buffer;

  GfxScenePassGroupHeader     m_header = { };
  uint32_t                    m_version = 0u;

  std::array<uint32_t,
    uint32_t(GfxSceneNodeType::eCount)> m_nodeCounts = { };

  bool resizeBuffer(
    const GfxContext&                   context,
    const GfxSceneNodeManager&          nodeManager);

  static uint32_t allocStorage(
          uint32_t&                     allocator,
          size_t                        size);

};


/**
 * \brief Render pass metadata
 *
 * Stores some properties of the render pass which are not expected
 * to change frequently.
 */
struct GfxScenePassDesc {
  /** Render pass flags. Defines the type and behaviour of the pass,
   *  as well as info on which parts of the pass are provided by the
   *  GPU on the fly. */
  GfxScenePassFlags flags = 0u;
  /** Application-defined render pass type mask. This can be used to
   *  mark passes as shadow map or mirror passes, which can be useful
   *  to lock out specific instances out of certain passes. */
  uint32_t typeMask = 0u;
  /** Node to attach the camera to. If negative, the camera will be
   *  in world space without any further transforms. */
  int32_t cameraNode = -1;
  /** Joint to attach the camera to. Ignored if negative or if no node
   *  is specified, otherwise this refers to an instance joint. */
  int32_t cameraJoint = -1;
  /** Node to attach the mirror plane to. If negative, the plane will
   *  in world space. Ignored if the \c GfxScenePassFlag::eUsesMirrorPlane
   *  flag is not set. */
  int32_t mirrorNode = -1;
  /** Joint to attach the mirror plane to. */
  int32_t mirrorJoint = -1;
};


/**
 * \brief Pass buffer header
 */
struct GfxScenePassBufferHeader {
  /** Rotation quaternions to apply to the final view transform
   *  when rendering cube maps. */
  std::array<Quat, 6> cubeFaceRotations;
};

static_assert(sizeof(GfxScenePassBufferHeader) == 96);


/**
 * \brief Render pass manager
 *
 * Stores a GPU buffer with render pass properties, and provides methods
 * to create and update render passes from the application.
 */
class GfxScenePassManager {

public:

  GfxScenePassManager(
          GfxDevice                     device);

  ~GfxScenePassManager();

  /**
   * \brief Queries GPU address of pass buffer
   *
   * Used for culling as well as rendering.
   * \returns GPU address of render pass buffer.
   */
  uint64_t getGpuAddress() const {
    return m_buffer ? m_buffer->getGpuAddress() : uint64_t(0u);
  }

  /**
   * \brief Creates a render pass
   *
   * \param [in] desc Render pass description
   * \returns Index of newly created render pass
   */
  uint16_t createRenderPass(
    const GfxScenePassDesc&             desc);

  /**
   * \brief Frees render pass
   *
   * The render pass will be overwritten with a set of default
   * properties on the GPU buffer in order to ensure safe
   * operation, and the index will be freed for reuse.
   * \param [in] pass Render pass index
   */
  void freeRenderPass(
          uint16_t                      pass);

  /**
   * \brief Updates render pass metadata
   *
   * Can be used to change render pass flags or to attach the camera
   * to a different node on the fly. If any property changes, this
   * will count as a camera cut, see \c updateRenderPassTransform.
   * \param [in] pass Render pass index
   * \param [in] desc Render pass metadata
   */
  void updateRenderPassMetadata(
          uint16_t                      pass,
    const GfxScenePassDesc&             desc);

  /**
   * \brief Updates projection
   *
   * If the projection differs from the existing one, this will
   * count as a camera cut, see \c updateRenderPassTransform.
   * \param [in] pass Render pass index
   * \param [in] projection New projection
   */
  void updateRenderPassProjection(
          uint16_t                      pass,
    const Projection&                   projection);

  /**
   * \brief Updates view space transform
   *
   * The transform is given relative to the camera node or joint,
   * or in world space if no camera node is specified.
   * \param [in] pass Render pass index
   * \param [in] transform World to view space transform
   * \param [in] cut If true, this update will be treated as a camera
   *    cut, and occlusion culling as well as motion vectors will be
   *    disabled for this pass for one single frame.
   */
  void updateRenderPassTransform(
          uint16_t                      pass,
    const QuatTransform&                transform,
          bool                          cut);

  /**
   * \brief Updates mirror plane
   *
   * \param [in] pass Render pass index
   * \param [in] plane Plane equation
   * \param [in] cut Whether to consider this a camera cut,
   *    see \c updateRenderPassTransform for details.
   */
  void updateRenderPassMirrorPlane(
          uint16_t                      pass,
    const Vector4D&                     plane,
          bool                          cut);

  /**
   * \brief Updates maximum view distance
   *
   * \param [in] pass Render pass index
   * \param [in] viewDistance Maximum view distance. If 0,
   *    the view distance will be considered infinite.
   */
  void updateRenderPassViewDistance(
          uint16_t                      pass,
          float                         viewDistance);

  /**
   * \brief Updates maximum view distance
   *
   * \param [in] pass Render pass index
   * \param [in] factor Distance factor for LOD selection. If less
   *    than 1.0, this will generally lead to higher geometry LODs
   *    being used at long view distances.
   */
  void updateRenderPassLodSelection(
          uint16_t                      pass,
          float                         factor);

  /**
   * \brief Updates viewport and array layer index
   *
   * Can be used to render to a different part of an image in
   * a multi-pass scenario.
   * \param [in] pass Render pass index
   * \param [in] viewport New viewport index
   * \param [in] layer New layer index
   */
  void updateRenderPassViewportLayer(
          uint16_t                      pass,
          uint32_t                      viewport,
          uint32_t                      layer);

  /**
   * \brief Updates virtual viewport region
   *
   * Ignored if the \c GfxScenePassFlag::eUsesViewportRegion flag is
   * not set for the pass. Can be used when using real viewports is
   * not an option for whatever reason.
   * \param [in] pass Render pass index
   * \param [in] offset Region offset
   * \param [in] extent Region extent
   */
  void updateRenderPassViewportRegion(
          uint16_t                      pass,
    const Offset2D&                     offset,
    const Extent2D&                     extent);

  /**
   * \brief Commits render pass updates
   *
   * Uploads modified pass properties to the GPU.
   * \param [in] context Context object
   * \param [in] pipelines Update pipelines
   * \param [in] frameId Current frame ID
   */
  void commitUpdates(
    const GfxContext&                   context,
    const GfxScenePipelines&            pipelines,
          uint32_t                      frameId);

  /**
   * \brief Processes render passes
   *
   * Finalizes render pass updates and computes absolute transforms,
   * mirror planes and view frustums as necessary.
   * \param [in] context Context object
   * \param [in] pipelines Update pipelines
   * \param [in] nodeManager Node manager
   * \param [in] frameId Current frame ID
   */
  void processPasses(
    const GfxContext&                   context,
    const GfxScenePipelines&            pipelines,
    const GfxSceneNodeManager&          nodeManager,
          uint32_t                      frameId);

private:

  struct DirtyPass {
    uint16_t          pass;
    uint16_t          reserved;
    GfxScenePassFlags flags;
  };

  GfxDevice                           m_device;
  GfxBuffer                           m_buffer;
  uint64_t                            m_bufferUpdateOffset = 0u;

  ObjectAllocator                     m_passAllocator;
  ObjectMap<GfxScenePassInfo, 8u, 8u> m_passData;

  LockFreeGrowList<DirtyPass>         m_dirtyList;

  void resizeBuffer(
          GfxContext                    context,
          uint32_t                      passCount,
          uint32_t                      currFrameId);

  void dispatchUpdateListInit(
    const GfxContext&                   context,
    const GfxScenePipelines&            pipelines);

  void dispatchHostCopy(
    const GfxContext&                   context,
    const GfxScenePipelines&            pipelines,
          uint32_t                      currFrameId);

  void addDirtyPass(
          uint16_t                      pass,
          GfxScenePassFlags             flags);

};

}
