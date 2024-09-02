#pragma once

#include "../util/util_flags.h"
#include "../util/util_types.h"

namespace as {

/**
 * \brief Shader I/O variable name
 */
using GfxSemanticName = ShortString<32>;


/**
 * \brief Resource usage
 *
 * Upon resource creation, specifies all the ways
 * in which a resource or view can be used.
 */
enum class GfxUsage : uint32_t {
  /** Resource can be used as the source in copy operations. For
   *  barriers, this will synchronize reads of copy operations. */
  eTransferSrc      = (1u << 0),
  /** Resource can be used as the destination in copy operations.
   *  For barriers, this will synchronize writes of copy operations. */
  eTransferDst      = (1u << 1),
  /** Resource can be used as an argument buffer for indirect draws and
   *  dispatches. For barriers, this will synchronize argument buffer reads. */
  eParameterBuffer  = (1u << 2),
  /** Resource can be used as an index buffer. For barriers,
   *  this will synchronize index buffer reads. */
  eIndexBuffer      = (1u << 3),
  /** Resource can be used as a vertex buffer. For barriers,
   *  this will synchronize vertex buffer reads. */
  eVertexBuffer     = (1u << 4),
  /** Resource can be used as a shader constant buffer. If this
   *  is used in a barrier, shader stages must be specified, and
   *  constant buffer reads from those stages will be synchronized. */
  eConstantBuffer   = (1u << 5),
  /** Resource can be used as a shader read-only resource. If this
   *  is used in a barrier, shader stages must be specified, and
   *  shader read operations from those stages will be synchronized. */
  eShaderResource   = (1u << 6),
  /** Resource can be used as a shader read-write resource. If this is
   *  used in a barrier, shader stages must be specified, and shader
   *  read and write operations from those stages will be synchronized. */
  eShaderStorage    = (1u << 7),
  /** Resource can be used as a render target. If this is used in a
   *  barrier, render target reads and writes will be synchronized. */
  eRenderTarget     = (1u << 8),
  /** Resource can be used as a shading rate image. If this is used
   *  in a barrier. shading rate reads will be synchronized. */
  eShadingRate      = (1u << 9),
  /** In a barrier, this will synchronize ray traversal access within
   *  the given shader stages. Cannot be used as a resource usage flag. */
  eBvhTraversal     = (1u << 10),
  /** Buffer resource can be used as an input buffer for BVH builds.
   *  In a barrier, this will synchronize build and copy operations
   *  on ray tracing BVHs. */
  eBvhBuild         = (1u << 11),
  /** Buffer can be written to by the CPU. This flag has no effect
   *  when used in barriers, since writes are either coherent or
   *  will be flushed on queue submissions. */
  eCpuWrite         = (1u << 12),
  /** Buffer can be read by the CPU. This should be used in a barrier
   *  after the last write to the resource before it gets read back. */
  eCpuRead          = (1u << 13),
  /** Image can be presented. This is only allowed on presentable images
   *  and only as the destination usage parameter of a barrier. */
  ePresent          = (1u << 14),
  /** Buffer can be used as an input for GPU decompression. */
  eDecompressionSrc = (1u << 15),
  /** Buffer can be used as an output for GPU decompression. */
  eDecompressionDst = (1u << 16),

  eFlagEnum         = 0
};

using GfxUsageFlags = Flags<GfxUsage>;


/**
 * \brief Shader stage
 */
enum class GfxShaderStage : uint32_t {
  eVertex           = (1u << 0),  ///< Vertex shader
  eTessControl      = (1u << 1),  ///< Tessellation control (hull) shader
  eTessEval         = (1u << 2),  ///< Tessellation evaluation (domain) shader
  eGeometry         = (1u << 3),  ///< Geometry shader
  eTask             = (1u << 4),  ///< Task shader
  eMesh             = (1u << 5),  ///< Mesh shader
  eFragment         = (1u << 6),  ///< Fragment shader
  eCompute          = (1u << 7),  ///< Compute shader

  /** All legacy pre-rasterization stages, i.e. vertex, tessellation
   *  and geometry shaders. Mesh and task shaders are not included. */
  ePreRaster        = eVertex | eTessControl | eTessEval | eGeometry,
  /** Both task and mesh shader stages. */
  eMeshTask         = eTask | eMesh,
  /** All supported shader stages. Backends must filter out any
   *  stages that are disabled or not supported. */
  eAll              = ~0u,

  eFlagEnum         = 0
};

using GfxShaderStages = Flags<GfxShaderStage>;


/**
 * \brief Barrier flags
 *
 * Specifies behaviour of image barriers.
 */
enum class GfxBarrierFlag : uint32_t {
  /** Discards previous image contents */
  eDiscard          = (1u << 0),
  eFlagEnum         = 0
};

using GfxBarrierFlags = Flags<GfxBarrierFlag>;


/**
 * \brief Device queue
 *
 * Identifies queues with given semantics and priorities on the system.
 * These queues may or may not natively map to hardware queues, and if
 * necessary, they will be mapped to other existing queues in order to
 * provide a common abstraction.
 */
enum class GfxQueue : uint32_t {
  /** Graphics queue used for rendering operations. */
  eGraphics         = 0,
  /** Compute queue used for per-frame operations that can run
   *  in parallel with rendering. Work submitted to this queue
   *  should not introduce bubbles even if this queue is mapped
   *  to the graphics queue. */
  eCompute          = 1,
  /** Compute queue used for asynchronous backgroud operations.
   *  If possible, this will be be mapped to a low-priority
   *  device queue, otherwise uses the regular compute queue. */
  eComputeBackground = 2,
  /** Compute queue used for asynchronous data uploads. This is
   *  a compute queue in order to allow decompression and further
   *  processing of the data while writing it to a GPU resource. */
  eComputeTransfer  = 3,
  /** Transfer queue. If available, this is a pure DMA queue
   *  on the device, otherwise it will be mapped to the compute
   *  upload qeueue. */
  eTransfer         = 4,
  /** Sparse binding queue. This may be mapped to the graphics
   *  queue on some devices, or be a dedicated queue. Commands
   *  must not be directly submitted to this queue. */
  eSparseBinding    = 5,
  /** Presentation queue. Commands must not be directly submitted
   *  to this queue, it is only used for present operations. */
  ePresent          = 6,
  /** Total number of unique queues */
  eQueueCount
};


/**
 * \brief Virtual address range
 */
struct GfxAddressRange {
  /** Base virtual address */
  uint64_t base = 0;
  /** Size of the range, in bytes */
  uint64_t size = 0;
};


/**
 * \brief Image aspect
 */
enum class GfxImageAspect : uint32_t {
  eColor              = (1u << 0),
  eDepth              = (1u << 1),
  eStencil            = (1u << 2),
  ePlane0             = (1u << 3),
  ePlane1             = (1u << 4),
  ePlane2             = (1u << 5),
  eFlagEnum           = 0
};

using GfxImageAspects = Flags<GfxImageAspect>;


/**
 * \brief Image subresources
 */
struct GfxImageSubresource {
  GfxImageSubresource() { }
  GfxImageSubresource(
    GfxImageAspects aspects_,
    uint32_t        mipIndex_,
    uint32_t        mipCount_,
    uint32_t        layerIndex_,
    uint32_t        layerCount_)
  : aspects     (aspects_)
  , mipIndex    (mipIndex_)
  , mipCount    (mipCount_)
  , layerIndex  (layerIndex_)
  , layerCount  (layerCount_) { }

  GfxImageAspects aspects = 0;
  uint32_t mipIndex   = 0;
  uint32_t mipCount   = 0;
  uint32_t layerIndex = 0; 
  uint32_t layerCount = 0; 

  bool operator == (const GfxImageSubresource&) const = default;
  bool operator != (const GfxImageSubresource&) const = default;

  /**
   * \brief Extracts a single mip level
   *
   * \param [in] mip Mip level, relative to \c mipIndex
   * \returns Subresources with only that mip level
   */
  GfxImageSubresource pickMip(uint32_t mip) const {
    return GfxImageSubresource(aspects,
      mipIndex + mip, 1, layerIndex, layerCount);
  }

  /**
   * \brief Extracts a mip range
   *
   * \param [in] mip Mip level, relative to \c mipIndex
   * \param [in] count Number of mip levels to pick
   * \returns Subresources with only that mip level
   */
  GfxImageSubresource pickMips(uint32_t mip, uint32_t count) const {
    return GfxImageSubresource(aspects,
      mipIndex + mip, count, layerIndex, layerCount);
  }

  /**
   * \brief Extracts a single array layer
   *
   * \param [in] layer Array layer, relative to \c layerIndex
   * \returns Subresources with only that array layer
   */
  GfxImageSubresource pickLayer(uint32_t layer) const {
    return GfxImageSubresource(aspects,
      mipIndex, mipCount, layerIndex + layer, 1);
  }

  /**
   * \brief Extracts a layer range
   *
   * \param [in] layer Array layer, relative to \c layerIndex
   * \param [in] count Number of arry layers to pick
   * \returns Subresources with only that array layer
   */
  GfxImageSubresource pickLayers(uint32_t layer, uint32_t count) const {
    return GfxImageSubresource(aspects,
      mipIndex, mipCount, layerIndex + layer, count);
  }

  /**
   * \brief Extracts aspects
   *
   * \param [in] mask Aspect mask, ANDed with \c aspects
   * \returns Subresources with the resulting aspect mask
   */
  GfxImageSubresource pickAspects(GfxImageAspects mask) const {
    return GfxImageSubresource(aspects & mask,
      mipIndex, mipCount, layerIndex, layerCount);
  }

  /**
   * \brief Extracts a single subresource
   *
   * \param [in] mask Aspect mask, ANDed with \c aspects
   * \param [in] mip Mip level, relative to \c mipIndex
   * \param [in] layer Array layer, relative to \c layerIndex
   * \returns Subresources with only that subresource
   */
  GfxImageSubresource pick(GfxImageAspects mask, uint32_t mip, uint32_t layer) const {
    return GfxImageSubresource(aspects & mask,
      mipIndex + mip, 1, layerIndex + layer, 1);
  }
};


/**
 * \brief Primitive type
 */
enum class GfxPrimitiveType : uint32_t {
  ePointList      = 0,  ///< Points
  eLineList       = 1,  ///< Line list
  eLineStrip      = 2,  ///< Line strip with primitive restart
  eTriangleList   = 3,  ///< Triangle list
  eTriangleStrip  = 4,  ///< Triangle strip with primitive restart
  ePatchList      = 5,  ///< Tessellation patch list
};


/**
 * \brief Viewport
 *
 * Includes both the viewport and scissor rect,
 * since both have to be set in one go anyway.
 */
struct GfxViewport {
  GfxViewport() { }

  GfxViewport(
          Offset2D                      offset_,
          Extent2D                      extent_)
  : offset(Vector2D(offset_))
  , extent(Vector2D(extent_))
  , scissor(offset_, extent_) { }

  Vector2D  offset    = Vector2D(0.0f, 0.0f);
  Vector2D  extent    = Vector2D(0.0f, 0.0f);
  float     minDepth  = 0.0f;
  float     maxDepth  = 1.0f;
  Rect2D    scissor   = Rect2D(Offset2D(0, 0), Extent2D(0, 0));
};


/**
 * \brief Indirect draw arguments
 */
struct GfxDrawArgs {
  uint32_t vertexCount;
  uint32_t instanceCount;
  uint32_t firstVertex;
  uint32_t firstInstance;
};


/**
 * \brief Indirect instanced draw arguments
 */
struct GfxDrawIndexedArgs {
  uint32_t indexCount;
  uint32_t instanceCount;
  uint32_t firstIndex;
  uint32_t firstVertex;
  uint32_t firstInstance;
};


/**
 * \brief Indirect dispatch count
 *
 * Applies to compute and mesh dispaches.
 */
struct GfxDispatchArgs {
  uint32_t x;
  uint32_t y;
  uint32_t z;
};


/**
 * \brief Buffer range
 *
 * Convenience structure.
 */
struct GfxRange {
  /** Range offset, in bytes */
  uint64_t offset;
  /** Range length, in bytes */
  uint64_t length;
};

}
