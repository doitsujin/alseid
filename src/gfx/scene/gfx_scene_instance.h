#pragma once

#include "../../util/util_buffer.h"
#include "../../util/util_object_map.h"
#include "../../util/util_quaternion.h"

#include "../gfx_buffer_pool.h"

#include "gfx_scene_node.h"
#include "gfx_scene_pipelines.h"

namespace as {

struct GfxSceneInstanceDesc;

/**
 * \brief Instance node flags
 */
enum class GfxSceneInstanceFlag : uint32_t {
  /** Indicates that the geometry of this instance is largely static. This
   *  enables some optimizations for local shadow map rendering, and should
   *  only be used for geometry that has no animations so that it does not
   *  frequently trigger re-rendering of cached shadows. However, shaders
   *  remain responsible to detect changes to the instance regardless. */
  eStatic           = (1u << 0),
  /** The instance does has joints or morph targets that can deform the
   *  geometry. Useful to determine whether to update the instance without
   *  having to access the instance data buffer or geometry buffer. */
  eDeform           = (1u << 1),
  /** The instance has animations. This implies that whenever a geometry
   *  buffer is specified, an animation buffer must also be present, and
   *  the animation count for this instance must not be zero. */
  eAnimation        = (1u << 2),
  /** Indicates that motion vectors should not be calculated when rendering
   *  an instance during the next frame. This flag may be set internally if
   *  the instance has not been visible during the previous frame, which means
   *  there is no valid data to compute motion vectors with, but may also be
   *  set externally if instance parameters have changed significantly. */
  eNoMotionVectors  = (1u << 3),

  eFlagEnum         = 0
};

using GfxSceneInstanceFlags = Flags<GfxSceneInstanceFlag>;


/**
 * \brief Instance node properties
 */
struct GfxSceneInstanceNodeInfo {
  /** Node index of where the transform is stored. */
  int32_t nodeIndex;
  /** Instance node flags. */
  GfxSceneInstanceFlags flags;
  /** Frame ID of when any data that affects the instance geometry has
   *  last been updated by either the host or device. Implicitly serves
   *  as a dirty flag to recompute absolute joint transforms for the
   *  current frame. */
  uint32_t dirtyFrameId;
  /** Frame ID of when updates to the instance were last committed.
   *  This includes recomputing joint transforms as necessary, as well
   *  as updating the absolute transform of the node if it has changed.
   *  This can be used to determine when to update static shadow maps. */
  uint32_t updateFrameId;
  /** GPU address of the geometry buffer. May be 0 if the asset is not
   *  resident. This buffer stores the actual geometry to render. */
  uint64_t geometryBuffer;
  /** GPU address of the animation buffer. May be 0 if the asset is not
   *  resident. This buffer stores animation keyframes and metadata. */
  uint64_t animationBuffer;
  /** GPU address of the instance property buffer. This stores all sorts
   *  of per-instance data, including joint transforms. */
  uint64_t propertyBuffer;
  /** Reserved for future use */
  uint64_t reserved;
};

static_assert(sizeof(GfxSceneInstanceNodeInfo) == 48);


/**
 * \brief Instance draw parameters
 */
struct GfxSceneInstanceDraw {
  /** Absolute material index. Used to determine which draw
   *  list to add this particular draw to. */
  uint16_t materialIndex;
  /** Local mesh index within the geometry. */
  uint16_t meshIndex;
  /** Local mesh instance index. */
  uint16_t meshInstanceIndex;
  /** Local mesh instance count. Ideally, all mesh instances should
   *  be processed in a single draw, unless individual instances
   *  use unique sets of parameters or are skipped entirely. */
  uint16_t meshInstanceCount;
  /** Offset of shading parameter data for this particular draw
   *  within the instance data buffer. Shaders assigned to the
   *  given material must interpret this data consistently. */
  uint32_t shadingParameterOffset;
  /** Total shading data size, in bytes. Used during allocation.
   *  The intention is to use the same set of parameters for all
   *  mesh instances, unless the data is arrayed internally. */
  uint32_t shadingParameterSize;
};

static_assert(sizeof(GfxSceneInstanceDraw) == 16);


/**
 * \brief Instance animation property header
 *
 * The header is immediately followed by a list of animation
 * parameters, with no special padding or alignment.
 */
struct GfxSceneAnimationHeader {
  /** Number of active animations for this instance */
  uint32_t activeAnimationCount;
};

static_assert(sizeof(GfxSceneAnimationHeader) == 4);


/**
 * \brief Animation blend operation
 *
 * Defines how multiple animations are composited. 
 */
enum class GfxSceneAnimationBlendOp : uint8_t {
  /** Blending is disabled, and animated joints and morph target weights
   *  are written back to the instance properties directly. This mode
   *  \e must be used for the first animation in each channel. */
  eNone           = 0,
  /** Chains transforms and adds morph target weights together. Useful
   *  when active animations affect different parts of the geometry. */
  eChain          = 1,
  /** Interpolates between animations. Useful when transitioning between
   *  two animations that affect the same parts of the geometry. */
  eSlerp          = 2,
};


/**
 * \brief Animation parameters
 */
struct GfxSceneAnimationParameters {
  static constexpr uint16_t cGroupIndexUseAppDefined = 0xffffu;
  static constexpr uint16_t cGroupCountUseBlendChannel = 0x0u;

  /** Blend operation for this animation. */
  GfxSceneAnimationBlendOp blendOp;
  /** Destination blend channel. Different channels can be used to
   *  interpolate between composite animations. */
  uint8_t blendChannel;
  /** Blend weight, as a normalized unsigned 16-bit integer. Only used
   *  if the blend op is an interpolation mode. */
  uint16_t blendWeight;
  /** Index of the animation group to process. A value of -1 indicates
   *  that the instance's relative joint transforms should be used. */
  uint16_t groupIndex;
  /** Number of animation groups to process. If 0, this will blend two
   *  animation channels together, and \c animationGroupIndex stores
   *  the source blend channel index. */
  uint16_t groupCount;
  /** Animation timestamp for keyframe lookup. */
  float timestamp;
};

static_assert(sizeof(GfxSceneAnimationParameters) == 12);


/**
 * \brief Instance data buffer header
 *
 * Stores offsets to various categories of per-instance data. This
 * data is not stored together with the instance nodes since the
 * amount of storage required heavily depends on the geometry and
 * the instance itself.
 */
struct GfxSceneInstanceDataHeader {
  /** Offset to global shading parameter data. All materials that use
   *  this data must interpret this data in a consistent manner. */
  uint32_t instanceParameterOffset;
  /** Global shading parameter data size, in bytes. */
  uint32_t instanceParameterSize;
  /** Number of unique draws required to render the geometry. Ideally,
   *  this should be one draw per unique mesh and material pair. */
  uint32_t drawCount;
  /** Offset to draw parameters within the buffer. Points to a tightly
   *  packed array of \c GfxSceneInstanceDraw structrues. */
  uint32_t drawOffset;
  /** Number of joint transforms. */
  uint32_t jointCount;
  /** Offset to relative joint transforms for this instance. This is
   *  double-buffered only if the instance has animations, where the
   *  second set of joints would store the host-defined values. */
  uint32_t jointRelativeOffset;
  /** Offset to absoltue joint transforms for this instance. This is
   *  double-buffered in order to support motion vectors, with the
   *  current frame's transforms being stored first and the previous
   *  set stored in the second half of the array. */
  uint32_t jointAbsoluteOffset;
  /** Number of morph target weights. */
  uint32_t weightCount;
  /** Offset to morph target weights. This is triple- or quad-buffered
   *  depending on whether the instance has animations, the first two
   *  set store the current and previous frame's weights, and the last
   *  set contains new weights updated by the host. */
  uint32_t weightOffset;
  /** Maximum number of concurrently active animations. Note that the
   *  number of currently active animations is stored in the animation
   *  property buffer itself. */
  uint32_t animationCount;
  /** Animation info offset. Stores parameters of active animations, as
   *  well as information on how to blend animations together. */
  uint32_t animationOffset;
  /** Reserved for future use */
  uint32_t reserved0;
  uint32_t reserved1;
  /** Axis-aligned bounding box, in model space. Empty if the number of
   *  joints is zero, otherwise this will contain the adjusted AABB. */
  GfxAabb<float16_t> aabb;
};

static_assert(sizeof(GfxSceneInstanceDataHeader) == 64);


/**
 * \brief Instance data buffer class
 *
 * Convenience class to create and manage the CPU-side
 * copy of the per-instance data buffer.
 */
class GfxSceneInstanceDataBuffer {

public:

  GfxSceneInstanceDataBuffer() = default;

  explicit GfxSceneInstanceDataBuffer(
    const GfxSceneInstanceDesc&         desc);

  GfxSceneInstanceDataBuffer             (GfxSceneInstanceDataBuffer&&) = default;
  GfxSceneInstanceDataBuffer& operator = (GfxSceneInstanceDataBuffer&&) = default;

  ~GfxSceneInstanceDataBuffer();

  /**
   * \brief Queries buffer size
   * \returns Buffer size
   */
  size_t getSize() const {
    return m_buffer.getSize();
  }

  /**
   * \brief Queries raw data pointer
   *
   * \param [in] offset Offset
   * \returns Raw data pointer
   */
  void* getAt(size_t offset) const {
    return m_buffer.getAt(offset);
  }

  /**
   * \brief Queries header
   * \returns Buffer header
   */
  const GfxSceneInstanceDataHeader* getHeader() const {
    return m_buffer.template getAs<GfxSceneInstanceDataHeader>(0);
  }

  /**
   * \brief Retrieves pointer to draw data
   * \returns Pointer to draw data
   */
  const GfxSceneInstanceDraw* getDraws() const {
    return m_buffer.template getAs<GfxSceneInstanceDraw>(getHeader()->drawOffset);
  }

  /**
   * \brief Retrieves pointer to relative joint transforms
   * \returns Pointer to joint transforms
   */
  QuatTransform* getJoints() const {
    auto header = getHeader();

    uint32_t jointOffset = header->animationCount ? header->jointCount : 0u;

    return header->jointCount
      ? m_buffer.template getAs<QuatTransform>(header->jointRelativeOffset) + jointOffset
      : nullptr;
  }

  /**
   * \brief Retrieves pointer to morph target weights
   * \returns Pointer to morph target weights
   */
  int16_t* getWeights() const {
    auto header = getHeader();

    uint32_t weightOffset = (header->animationCount ? 3u : 2u) * header->weightCount;

    return header->weightCount
      ? m_buffer.template getAs<int16_t>(header->weightOffset) + weightOffset
      : nullptr;
  }

  /**
   * \brief Retrieves pointer to shading parameter data
   * \returns Pointer to global shading parameter data
   */
  void* getShadingParameters() const {
    auto header = getHeader();

    return header->instanceParameterSize
      ? m_buffer.getAt(header->instanceParameterOffset)
      : nullptr;
  }

  /**
   * \brief Retrieves pointer to material parameter data
   *
   * \param [in] draw Draw index
   * \returns Pointer to material properties
   */
  void* getMaterialParameters(uint32_t draw) const {
    auto draws = getDraws();

    return draws[draw].shadingParameterSize
      ? m_buffer.getAt(draws[draw].shadingParameterOffset)
      : nullptr;
  }

  /**
   * \brief Retrieves pointer to animation metadata
   * \returns Pointer to animation metadata
   */
  GfxSceneAnimationHeader* getAnimationMetadata() const {
    auto header = getHeader();

    return header->animationCount
      ? m_buffer.getAs<GfxSceneAnimationHeader>(header->animationOffset)
      : nullptr;
  }

  /**
   * \brief Retrieves pointer to animation parameters
   *
   * The returned pointer points to an array of
   * \c animationCount entries.
   * \returns Pointer to animation parameters
   */
  GfxSceneAnimationParameters* getAnimationParameters() const {
    auto header = getHeader();

    return header->animationCount
      ? m_buffer.getAs<GfxSceneAnimationParameters>(
          header->animationOffset + sizeof(GfxSceneAnimationHeader))
      : nullptr;
  }

private:

  AlignedBuffer m_buffer;

  uint32_t allocateStorage(
          uint32_t&                     allocator,
          uint32_t                      size);

};


/**
 * \brief Instance data dirty flags
 */
enum class GfxSceneInstanceDirtyFlag : uint32_t {
  /** Node info was updated. Usually only happens when the residency
   *  status changes and buffer pointers are set approproately. */
  eDirtyNode                = (1u << 0),
  /** Data buffer header has been updated. Only set for instances
   *  that were only just made resident. */
  eDirtyHeader              = (1u << 1),
  /** Relative transforms were updated on the CPU. Indicates that
   *  the given data buffer portion must be manually updated. */
  eDirtyRelativeTransforms  = (1u << 2),
  /** Morph target weights were updated on the CPU. */
  eDirtyMorphTargetWeights  = (1u << 3),
  /** Global shading parameters were updated on the CPU and need
   *  to be uploaded to the GPU. */
  eDirtyShadingParameters   = (1u << 4),
  /** Per-draw material data was updated on the CPU and needs to be
   *  uploaded to the GPU. Dirty states are not tracked per draw. */
  eDirtyMaterialParameters  = (1u << 5),
  /** Animation parameters were updated on the CPU and need to be
   *  uploaded to the GPU. GPU-side updates happen automatically. */
  eDirtyAnimations          = (1u << 6),

  eFlagEnum                 = 0
};

using GfxSceneInstanceDirtyFlags = Flags<GfxSceneInstanceDirtyFlag>;


/**
 * \brief Instance info
 *
 * Stores additional info about an instance that is not stored
 * inside the node structure itself.
 */
struct GfxSceneInstanceHostInfo {
  /** Dirty flags. When any of these are set, instance data must be
   *  updated within the current frame. */
  AtomicFlags<GfxSceneInstanceDirtyFlag> dirtyFlags = { 0u };
  /** Host copy of instance parameters. */
  GfxSceneInstanceDataBuffer dataBuffer;
  /** Allocated buffer slice. If this is a null slice, the
   *  instance is by definition not resident. */
  GfxBufferSlice dataSlice;
};


/**
 * \brief Instance description
 *
 * Stores properties needed to allocate instance storage, as
 * well as draw parameters needed to render the instance.
 */
struct GfxSceneInstanceDesc {
  /** Index of the transform node for this instance. The node
   *  index is immutable and must be allocated beforehand. */
  uint32_t nodeIndex = 0;
  /** Instance flags. Note that this can be changed later. */
  GfxSceneInstanceFlags flags = 0;
  /** Size of global shader parameter data, in bytes. */
  uint32_t parameterDataSize = 0;
  /** Number of joint transforms. Should match the number of
   *  joints of the geometry to render. */
  uint32_t jointCount = 0;
  /** Number of morph target weights. Should match the number of
   *  morph targets of the geometry to render. */
  uint32_t weightCount = 0;
  /** Maximum number of concurrently active animations. */
  uint32_t animationCount = 0;
  /** Number of draws. Must be greater than 0 for any renderable
   *  instance, and cannot be changed after the fact. */
  uint32_t drawCount = 0;
  /** Pointer to draw parameters, including the size of per-draw
   *  material parameters. */
  const GfxSceneInstanceDraw* draws = nullptr;
  /** Axis-aligned bounding box in model space. Should be identical
   *  to the geometry's AABB, and will be recomputed on the fly if
   *  the instance is animated. */
  GfxAabb<float16_t> aabb = { };
};


/**
 * \brief Instance node update info buffer
 *
 * Stores an index into the destination node buffer, as well as
 * an index into the scratch buffer to read node data from.
 */
struct GfxSceneInstanceNodeUpdateEntry {
  static constexpr uint32_t cSrcIndexNone = ~0u;
  /** Instance node index to update */
  uint32_t dstIndex;
  /** Index into the host data buffer. When set to \c cSrcIndexNone,
   *  only the dirty frame ID for the given node will be updated. */
  uint32_t srcIndex;
};

static_assert(sizeof(GfxSceneInstanceNodeUpdateEntry) == 8);


/**
 * \brief Instance data buffer description
 *
 * Used to compute the data layout of the data buffer.
 */
struct GfxSceneInstanceBufferDesc {
  /** Number of instances */
  uint32_t instanceCount = 0u;
};


/**
 * \brief Instance data buffer
 *
 * Provides GPU storage for instance data, including a buffer
 * pool for shading parameters and node transforms.
 */
class GfxSceneInstanceBuffer {

public:

  explicit GfxSceneInstanceBuffer(
          GfxDevice                     device);

  ~GfxSceneInstanceBuffer();

  GfxSceneInstanceBuffer             (const GfxSceneInstanceBuffer&) = delete;
  GfxSceneInstanceBuffer& operator = (const GfxSceneInstanceBuffer&) = delete;

  /**
   * \brief Queries node buffer address
   *
   * Needed when accessing instance node data. The entire buffer
   * is just a flat array of \c GfxSceneInstanceNodeInfo structs.
   * \returns Buffer address
   */
  uint64_t getGpuAddress() const {
    return m_nodeBuffer ? m_nodeBuffer->getGpuAddress() : 0ull;
  }

  /**
   * \brief Resizes instance node buffer to the given size
   *
   * Must be called any time the instance node count changes.
   * The buffer must be ready to be used for transfers.
   * \param [in] context Context object. Will be used to copy
   *    old buffer contents as necessary.
   * \param [in] desc Instance buffer description
   * \returns Old buffer, or \c nullptr if no the buffer was
   *    not actually replaced. This must be kept alive until
   *    the current frame finishes processing on the GPU.
   */
  GfxBuffer resizeBuffer(
    const GfxContext&                   context,
    const GfxSceneInstanceBufferDesc&   desc);

  /**
   * \brief Allocates data buffer slice for an instance
   *
   * \param [in] dataSize Number of bytes to allocate
   * \returns Allocated data slice
   */
  GfxBufferSlice allocData(
          uint64_t                      dataSize);

  /**
   * \brief Frees data buffer slice
   *
   * Must only be called when the given data
   * slice is no longer in use by the GPU.
   * \param [in] dataSlice Slice to free
   */
  void freeData(
    const GfxBufferSlice&               dataSlice);

  /**
   * \brief Cleans up GPU resources
   */
  void trim();

private:

  GfxDevice m_device;
  GfxBuffer m_nodeBuffer;

  std::unique_ptr<GfxBufferPool> m_dataBuffer;

};



/**
 * \brief Instance manager
 *
 * Provides both host and GPU storage for instance data, and manages
 * updates of the data structures involved. Works in tandem with the
 * normal node manager, which manages transforms and node residency.
 */
class GfxSceneInstanceManager {
  friend class GfxSceneMaterialManager;
public:

  explicit GfxSceneInstanceManager(
          GfxDevice                     device);

  ~GfxSceneInstanceManager();

  /**
   * \brief Queries instance buffer GPU address
   * \returns GPU address of instance buffer
   */
  uint64_t getGpuAddress() const {
    return m_gpuResources.getGpuAddress();
  }

  /**
   * \brief Creates an instance node
   *
   * \param [in] desc Instance description
   * \returns Node reference for the new instance
   */
  GfxSceneNodeRef createInstance(
    const GfxSceneInstanceDesc&         desc);

  /**
   * \brief Frees an instance node
   *
   * Releases resources attached to the instance once
   * the current frame ID has completed on the GPU.
   * \param [in] instance Node reference
   * \param [in] frameId Current frame ID
   */
  void destroyInstance(
          GfxSceneNodeRef               instance,
          uint32_t                      frameId);

  /**
   * \brief Updates basic node properties
   *
   * \param [in] instance Instance node reference
   * \param [in] flags Instance flags
   */
  void updateInstance(
          GfxSceneNodeRef               instance,
          GfxSceneInstanceFlags         flags);

  /**
   * \brief Updates relative joint transforms of an instance
   *
   * Will trigger an update of absolute transforms when the
   * instance is next used for rendering.
   * \param [in] instance Instance node reference
   * \param [in] first First joint to update
   * \param [in] count Number of joints to update
   * \param [in] joints Joint transform data
   */
  void updateJoints(
          GfxSceneNodeRef               instance,
          uint32_t                      first,
          uint32_t                      count,
    const QuatTransform*                joints);

  /**
   * \brief Updates morph target weights
   *
   * \param [in] instance Instance node reference
   * \param [in] first First weight to update
   * \param [in] count Number of weights to update
   * \param [in] weights Normalized weights
   */
  void updateWeights(
          GfxSceneNodeRef               instance,
          uint32_t                      first,
          uint32_t                      count,
    const int16_t*                      weights);

  /**
   * \brief Updates global shading parameters
   *
   * The entire data struct must be updated at once.
   * \param [in] instance Instance node reference
   * \param [in] size Data size
   * \param [in] data Shading parameter data
   */
  void updateShadingParameters(
          GfxSceneNodeRef               instance,
          size_t                        size,
    const void*                         data);

  /**
   * \brief Updates material parameters
   *
   * \param [in] instance Instance node reference
   * \param [in] draw Draw index
   * \param [in] size Data size
   * \param [in] data Shading parameter data
   */
  void updateMaterialParameters(
          GfxSceneNodeRef               instance,
          uint32_t                      draw,
          size_t                        size,
    const void*                         data);

  /**
   * \brief Updates animation metadata
   *
   * \param [in] instance Instance node reference
   * \param [in] metadata Animation metadata
   */
  void updateAnimationMetadata(
          GfxSceneNodeRef               instance,
    const GfxSceneAnimationHeader&      metadata);

  /**
   * \brief Updates animation properties
   *
   * \param [in] instance Instance node reference
   * \param [in] animation Active animation index
   * \param [in] parameters Animation parameters
   */
  void updateAnimationParameters(
          GfxSceneNodeRef               instance,
          uint32_t                      animation,
    const GfxSceneAnimationParameters&  parameters);

  /**
   * \brief Updates pointer to geometry buffer
   *
   * When setting the buffer address to 0, the node must
   * be marked as non-resident.
   * \param [in] instance Instance node reference
   * \param [in] geometryBuffer Geometry buffer address
   */
  void updateGeometryBuffer(
          GfxSceneNodeRef               instance,
          uint64_t                      geometryBuffer);

  /**
   * \brief Updates pointer to animation buffer
   *
   * If the instance is animated, the node must be marked
   * as non-resident when setting the buffer address to 0.
   * \param [in] instance Instance node reference
   * \param [in] animationBuffer Animation buffer address
   */
  void updateAnimationBuffer(
          GfxSceneNodeRef               instance,
          uint64_t                      animationBuffer);

  /**
   * \brief Allocates GPU memory for instance data
   *
   * Data is uploaded to the GPU on the next call to \c commitUpdates.
   * \param [in] instance Instance node reference
   */
  void allocateGpuBuffer(
          GfxSceneNodeRef               instance);

  /**
   * \brief Frees GPU memory for instance data
   *
   * Requires that the node be marked as non-resident.
   * \param [in] instance Instance node reference
   */
  void freeGpuBuffer(
          GfxSceneNodeRef               instance);

  /**
   * \brief Commits pending updates
   *
   * Ensures that access to already created instance objects
   * happens in constant time, and uploads updated instance
   * data to the GPU. This must not be executec concurrently
   * with any other method of this class.
   *
   * This method must be called once at the start of a frame.
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
   * \brief Processes animations for visible instances
   *
   * Iterates over potentially visible instances in the given pass
   * group and processes active animations for the current frame,
   * recomputing relative joint transforms and morph target weights.
   * \param [in] context Context object
   * \param [in] pipelines Update pipelines
   * \param [in] groupBuffer Pass group buffer
   * \param [in] frameId Current frame ID
   */
  void processPassGroupAnimations(
    const GfxContext&                   context,
    const GfxScenePipelines&            pipelines,
    const GfxScenePassGroupBuffer&      groupBuffer,
          uint32_t                      frameId);

  /**
   * \brief Processes visible instances
   *
   * Iterates over potentially visible instances in the given
   * pass group and updates absolute joint transforms, morph
   * target weights, and related properties as necessary.
   * This should be run immediately after BVH traversal.
   * \param [in] context Context object
   * \param [in] pipelines Update pipelines
   * \param [in] nodeManager Node manager
   * \param [in] groupBuffer Pass group buffer
   * \param [in] frameId Current frame ID
   */
  void processPassGroupInstances(
    const GfxContext&                   context,
    const GfxScenePipelines&            pipelines,
    const GfxSceneNodeManager&          nodeManager,
    const GfxScenePassGroupBuffer&      groupBuffer,
          uint32_t                      frameId);

private:

  GfxSceneInstanceBuffer              m_gpuResources;

  std::unordered_map<
    uint32_t, GfxBuffer>              m_gpuBuffers;

  ObjectMap<GfxSceneInstanceNodeInfo> m_instanceNodeData;
  ObjectMap<GfxSceneInstanceHostInfo> m_instanceHostData;

  ObjectAllocator                     m_instanceAllocator;

  alignas(CacheLineSize)
  std::mutex                          m_dirtyMutex;
  std::vector<uint32_t>               m_dirtyIndices;

  std::vector<GfxSceneInstanceNodeUpdateEntry> m_updateEntries;

  alignas(CacheLineSize)
  std::mutex                          m_freeMutex;
  std::unordered_multimap<
    uint32_t, uint32_t>               m_freeQueue;
  std::unordered_multimap<
    uint32_t, GfxBufferSlice>         m_freeSlices;

  void updateBufferData(
    const GfxContext&                   context,
    const GfxScenePipelines&            pipelines,
          uint32_t                      frameId);

  void cleanupBufferSlices(
          uint32_t                      frameId);

  void cleanupInstanceNodes(
          uint32_t                      frameId);

  void cleanupGpuBuffers(
          uint32_t                      frameId);

  void markDirty(
          uint32_t                      index,
          GfxSceneInstanceDirtyFlags    flags);

  void addToDirtyList(
          uint32_t                      index);

  void resizeGpuBuffer(
    const GfxContext&                   context,
          uint32_t                      frameId);

  static void uploadInstanceData(
    const GfxContext&                   context,
    const GfxSceneInstanceHostInfo&     hostData,
          uint32_t                      offset,
          uint32_t                      size);

  const GfxSceneInstanceDataBuffer& getInstanceData(
          GfxSceneNodeRef               instance) const {
    return m_instanceHostData[uint32_t(instance.index)].dataBuffer;
  }

};

}
