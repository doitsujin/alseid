#pragma once

#include "../../util/util_buffer.h"
#include "../../util/util_object_map.h"
#include "../../util/util_quaternion.h"

#include "../asset/gfx_asset_manager.h"

#include "../gfx_buffer_pool.h"

#include "gfx_scene_node.h"
#include "gfx_scene_pipelines.h"

namespace as {

struct GfxSceneInstanceDesc;

/**
 * \brief Instance node flags
 */
enum class GfxSceneInstanceFlag : uint32_t {
  /** Indicates that all required resources, including the geometry buffer,
   *  are fully resident. This flag is managed entirely by GPU shaders. */
  eResident         = (1u << 0),
  /** Indicates that the geometry of this instance is largely static. This
   *  enables some optimizations for local shadow map rendering, and should
   *  only be used for geometry that has no animations so that it does not
   *  frequently trigger re-rendering of cached shadows. However, shaders
   *  remain responsible to detect changes to the instance regardless. */
  eStatic           = (1u << 1),
  /** The instance does has joints or morph targets that can deform the
   *  geometry. Useful to determine whether to update the instance without
   *  having to access the instance data buffer or geometry buffer. */
  eDeform           = (1u << 2),
  /** The instance has animations. This implies that whenever a geometry
   *  buffer is specified, an animation buffer must also be present, and
   *  the animation count for this instance must not be zero. */
  eAnimation        = (1u << 3),
  /** Indicates that motion vectors should not be calculated when rendering
   *  an instance during the next frame. This flag may be set internally if
   *  the instance has not been visible during the previous frame, which means
   *  there is no valid data to compute motion vectors with, but may also be
   *  set externally if instance parameters have changed significantly. */
  eNoMotionVectors  = (1u << 4),

  /** Indicates that joints and morph target weights are dirty and need
   *  to be updated. This flag is primarily managed by GPU shaders. */
  eDirtyDeform      = (1u << 24),
  /** Indicates that per-draw instance data needs to be updated, e.g.
   *  after an update to either of the asset lists. Primarily managed
   *  by GPU shaders, but may be updated manually if needed. */
  eDirtyAssets      = (1u << 25),

  /** Set of all dirty flags */
  eDirtyFlags       = eDirtyDeform | eDirtyAssets,
  /** First dirty flag bit */
  eDirtyShift       = 24u,

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
  /** GPU address of the instance property buffer. This stores all sorts
   *  of per-instance data, including joint transforms. */
  uint64_t propertyBuffer;
  /** GPU address of asset list to use for this instance. If set,
   *  all non-local asset data will be pulled from this buffer. */
  uint64_t assetListBuffer;
};

static_assert(sizeof(GfxSceneInstanceNodeInfo) == 32);


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
  /** Meshlet count of the most detailed LOD for this draw. Only
   *  counts for a single local mesh instance, so the maximum total
   *  task shader thread count is the mesh instance count multiplied
   *  with the meshlet count. */
  uint32_t maxMeshletCount;
  /** Offset of material parameter data for this particular draw
   *  within the instance data buffer. Shaders assigned to the
   *  given material must interpret this data consistently. */
  uint32_t materialParameterOffset;
  /** Total material data size, in bytes. Used during allocation.
   *  The intention is to use the same set of parameters for all
   *  mesh instances, unless the data is arrayed internally. */
  uint32_t materialParameterSize;
  /** Offset of resource parameter data, in bytes. This section
   *  will only be written by the GPU and contains descriptor
   *  indices and buffer addresses to be used by this instance. */
  uint32_t resourceParameterOffset;
  /** Size of resource parameter data, in bytes. */
  uint32_t resourceParameterSize;
  /** Reserved for future use. */
  uint32_t reserved;
};

static_assert(sizeof(GfxSceneInstanceDraw) == 32);


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
 * \brief Resource type for an instance
 */
enum class GfxSceneInstanceResourceType : uint8_t {
  /** Descriptor index. May index into an arbitrary descriptor array
   *  that the application must bind before performing draws. Indices
   *  are represented as signed 32-bit integers, with negative values
   *  indicating that the resource is not valid or not resident. */
  eDescriptorIndex  = 0,
  /** Buffer address. Points directly to a buffer of an arbitrary type
   *  that the shader can interpret. Addresses are 64-bit unsigned
   *  integers, and must be aligned to 16 bytes. */
  eBufferAddress    = 1,
};


/**
 * \brief Resource flags for an instance
 */
enum class GfxSceneInstanceResourceFlag : uint8_t {
  /** The resource is optional, and shaders will either ignore a null
   *  buffer address or descriptor index. */
  eOptional         = (1u << 0),

  eFlagEnum         = 0
};

using GfxSceneInstanceResourceFlags = Flags<GfxSceneInstanceResourceFlag>;


/**
 * \brief Resource indirection entry
 *
 * Stores information about where to read a descriptor index or buffer
 * address from, and an byte offset relative to the instance data buffer
 * that points into a draw's resource buffer to write to.
 */
struct GfxSceneInstanceResourceIndirectionEntry {
  /** Resource type. Determines the parameter data type. */
  GfxSceneInstanceResourceType type;
  /** Resource flags */
  GfxSceneInstanceResourceFlags flags;
  /** Resource entry within the instance. */
  uint16_t srcEntry;
  /** Byte offset of where to write the parameter. */
  uint32_t dstOffset;
};

static_assert(sizeof(GfxSceneInstanceResourceIndirectionEntry) == 8);


/**
 * \brief Resource info
 *
 * Encodes either an index into an asset list, or a resource
 * parameter directly such as a descriptor. If the least
 * significant bit is set, the index in the upper dword is
 * an asset reference.
 */
struct GfxSceneInstanceResource {
  GfxSceneInstanceResource() = default;

  explicit GfxSceneInstanceResource(uint64_t d)
  : data(d) { }

  static GfxSceneInstanceResource fromAssetIndex(uint32_t index) {
    return GfxSceneInstanceResource(uint64_t(index) << 32 | 1ull);
  }

  static GfxSceneInstanceResource fromDescriptorIndex(int32_t index) {
    return GfxSceneInstanceResource(uint64_t(index) << 32);
  }

  static GfxSceneInstanceResource fromBufferAddress(uint64_t va) {
    return GfxSceneInstanceResource(va);
  }

  uint64_t data;
};


/**
 * \brief Instance data buffer header
 *
 * Stores offsets to various categories of per-instance data. This
 * data is not stored together with the instance nodes since the
 * amount of storage required heavily depends on the geometry and
 * the instance itself.
 */
struct GfxSceneInstanceDataHeader {
  /** Absolute address of geometry buffer. */
  uint64_t geometryVa;
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
  /** Number of unique resources referenced by this instance. */
  uint32_t resourceCount;
  /** Resource buffer. Stores asset list indices or plain resource
   *  parameters in the form of descriptor indices or buffer addresses. */
  uint32_t resourceOffset;
  /** Resource indirection count. Resource indirection data is stored
   *  immediately following resource entries in the resource buffer. */
  uint32_t indirectionCount;
  /** Indirection data offset, in bytes. Stores information about where
   *  to copy resource parameters, so that draw time indirections can
   *  be avoided as much as possible. */
  uint32_t indirectionOffset;
  /** Axis-aligned bounding box, in model space. Empty if the number of
   *  joints is zero, otherwise this will contain the adjusted AABB. */
  GfxAabb<float16_t> aabb;
};

static_assert(sizeof(GfxSceneInstanceDataHeader) == 80);


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

    return draws[draw].materialParameterSize
      ? m_buffer.getAt(draws[draw].materialParameterOffset)
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

  /**
   * \brief Retrieves pointer to resource entries
   *
   * Encodes a raw descriptor index, a buffer address,
   * or an index into the asset list buffer.
   * \returns Pointer to resource entries
   */
  GfxSceneInstanceResource* getResourceEntries() const {
    auto header = getHeader();

    return header->resourceCount
      ? m_buffer.getAs<GfxSceneInstanceResource>(header->resourceOffset)
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
  /** Resource parameters were updated on the CPU and need to be
   *  uploaded to the GPU. Will trigger the needed GPU update. */
  eDirtyAssets              = (1u << 7),

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
  /** Allocated buffer slice. */
  GfxBufferSlice gpuBuffer;
};


/**
 * \brief Instance draw description
 *
 * Defines the mesh and mesh instances to draw, as well as
 * the absolute index of the material to use. Also assigns
 * resources to use for the draw and the shader parameter
 * data layout.
 */
struct GfxSceneInstanceDrawDesc {
  /** Absolute material index. Used to determine which draw
   *  list to add this particular draw to. */
  uint16_t materialIndex = 0;
  /** Local mesh index within the geometry. */
  uint16_t meshIndex = 0;
  /** Local mesh instance index. */
  uint16_t meshInstanceIndex = 0;
  /** Local mesh instance count. Ideally, all mesh instances should
   *  be processed in a single draw, unless individual instances
   *  use unique sets of parameters or are skipped entirely. */
  uint16_t meshInstanceCount = 0;
  /** Number of meshlets in the most detailed LOD for this draw. */
  uint32_t maxMeshletCount = 0;
  /** Material parameter size, in bytes. This data is uniform
   *  within the draw. */
  uint32_t materialParameterSize = 0;
  /** Number of resources used within this draw */
  uint32_t resourceCount = 0;
  /** Resource indices used within the draw. Each index accesses
   *  a resource from the instance description, and the order in
   *  which resources are defined determines the layout of the
   *  resource buffer. Buffer addresses are aligned to 8 bytes. */
  const uint16_t* resourceIndices = nullptr;
};


/**
 * \brief Resource description for an instance
 */
struct GfxSceneInstanceResourceDesc {
  /** Resource name. If not empty, resources can be looked up by
   *  name and manually get replaced, which can be useful in case
   *  per-instance resources need to be created dynamically. */
  GfxSemanticName name = "";
  /** Resource type. */
  GfxSceneInstanceResourceType type = GfxSceneInstanceResourceType::eDescriptorIndex;
  /** Resource flags. */
  GfxSceneInstanceResourceFlags flags = 0;
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
  const GfxSceneInstanceDrawDesc* draws = nullptr;
  /** Number of resources. Resources defined for this instance can
   *  be used by multiple draws via the respective index array. */
  uint32_t resourceCount = 0;
  /** Geometry resource index. Must point to a valid buffer
   *  resource for the instance to render. */
  uint32_t geometryResource = ~0u;
  /** Resource descriptions. */
  const GfxSceneInstanceResourceDesc* resources = nullptr;
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
  /** Dirty flags to set for the node. */
  uint8_t dirtyFlags;
  /** Instance node index to update */
  uint24_t dstIndex;
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
   */
  void resizeBuffer(
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
   * \brief Updates resources for the instance
   *
   * \param [in] instance Instance node reference
   * \param [in] resourceIndex Resource to update
   * \param [in] resourceInfo Encoded resource info
   */
  void updateResource(
          GfxSceneNodeRef               instance,
          uint32_t                      resourceIndex,
          GfxSceneInstanceResource      resourceInfo);

  /**
   * \brief Updates pointer to asset list
   *
   * \param [in] instance Instance node reference
   * \param [in] assetListBuffer Asset list buffer address
   */
  void updateAssetList(
          GfxSceneNodeRef               instance,
          uint64_t                      assetListBuffer);

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
   * \param [in] assetManager Asset manager
   * \param [in] frameId Current frame ID
   */
  void processPassGroupInstances(
    const GfxContext&                   context,
    const GfxScenePipelines&            pipelines,
    const GfxSceneNodeManager&          nodeManager,
    const GfxScenePassGroupBuffer&      groupBuffer,
    const GfxAssetManager&              assetManager,
          uint32_t                      frameId);

private:

  GfxSceneInstanceBuffer              m_gpuResources;

  ObjectMap<GfxSceneInstanceNodeInfo> m_instanceNodeData;
  ObjectMap<GfxSceneInstanceHostInfo> m_instanceHostData;

  ObjectAllocator                     m_instanceAllocator;

  alignas(CacheLineSize)
  LockFreeGrowList<uint32_t>          m_dirtyIndices;

  std::vector<GfxSceneInstanceNodeUpdateEntry> m_updateEntries;
  std::vector<GfxSceneUploadChunk>    m_uploadChunks;

  alignas(CacheLineSize)
  std::mutex                          m_freeMutex;
  std::unordered_multimap<
    uint32_t, uint32_t>               m_freeQueue;

  void updateBufferData(
    const GfxContext&                   context,
    const GfxScenePipelines&            pipelines,
          uint32_t                      frameId);

  void cleanupInstanceNodes(
          uint32_t                      frameId);

  void markDirty(
          uint32_t                      index,
          GfxSceneInstanceDirtyFlags    flags);

  void addToDirtyList(
          uint32_t                      index);

  void resizeGpuBuffer(
    const GfxContext&                   context,
          uint32_t                      frameId);

  void uploadInstanceData(
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
