#pragma once

#include "../util/util_iface.h"
#include "../util/util_small_vector.h"

#include "gfx_buffer.h"
#include "gfx_context.h"
#include "gfx_descriptor_array.h"
#include "gfx_image.h"
#include "gfx_memory.h"
#include "gfx_pipeline.h"
#include "gfx_presenter.h"
#include "gfx_ray_tracing.h"
#include "gfx_sampler.h"
#include "gfx_semaphore.h"
#include "gfx_shader.h"
#include "gfx_submission.h"
#include "gfx_types.h"

namespace as {

/**
 * \brief Adapter features and capabilities
 */
struct GfxDeviceFeatures {
  /** Indicates support for conservative rasterization. */
  uint32_t conservativeRasterization : 1;
  /** Indicates support for the depth bounds test. */
  uint32_t depthBounds : 1;
  /** Indicates support for dual-source blending. */
  uint32_t dualSourceBlending : 1;
  /** Indicates that graphics pipelines can be fast-linked at
   *  runtime, so that explicit calls to \c compileVariant are
   *  not necessary in order to avoid stutter. */
  uint32_t fastLinkGraphicsPipelines : 1;
  /** Indicates whether the fragment shader can export
   *  a per-pixel stencil reference. */
  uint32_t fragmentShaderStencilExport : 1;
  /** Indicates support for variable rate shading. */
  uint32_t fragmentShadingRate : 1;
  /** Indicates support for decoding gdeflate-encoded buffers
   *  directly on the GPU. */
  uint32_t gdeflateDecompression : 1;
  /** Indicates support for ray tracing using ray queries. */
  uint32_t rayTracing : 1;
  /** Indicates support for 16-bit float
   *  and integer arithmetic in shaders */
  uint32_t shader16Bit : 1;
  /** Indicates support for 64-bit float
   *  and integer arithmetic in shaders */
  uint32_t shader64Bit : 1;
  /** Indicates support for reading and writing 16-bit
   *  values to or from storage buffers in shaders. */
  uint32_t shaderStorage16Bit : 1;
  /** Indicates whether vertex, geometry and tessellation
   *  shaders can access shader storage resources. */
  uint32_t vertexShaderStorage : 1;
  /** Indicates whether shader stages other than the geometry
   *  shader can export the viewport index or layer index */
  uint32_t vertexShaderViewportLayerExport : 1;
  /** Bit mask of supported shader stages. Support is guaranteed
   *  for vertex, fragment and compute shaders, and desktop systems
   *  will typically include support for tessellation and geometry. */
  GfxShaderStages shaderStages;
  /** Maximum number of sampler descriptors in descriptor arrays.
   *  This amount applies to \e all descriptor arrays combined,
   *  and is also the maximum number of sampler descriptors in
   *  a single descriptor array.
   *  This will be at least 1000. */
  uint32_t maxSamplerDescriptors;
  /** Maximum number of resource descriptors in descriptor arrays.
   *  This applies to all non-sampler descriptor types in the same
   *  fashion as \c maxSamplerDescriptors does to samplers.
   *  This will be at least 250000. */
  uint32_t maxResourceDescriptors;
  /** Tile size of shading rate images, in pixels. Will be between
   *  8 and 32, and will generally be the smallest that the device
   *  supports within these boundaries. */
  Extent2D shadingRateTileSize;
  /** Logarithmic representation of the shading rate tile size. Can
   *  be used to more easily compute the shading rate image size. */
  Extent2D shadingRateTileSizeLog2;
};


/**
 * \brief Format features
 */
enum class GfxFormatFeature : uint32_t {
  /** Format can be used for index buffers */
  eIndexBuffer          = (1u << 0),
  /** Format can be used for vertex buffers */
  eVertexBuffer         = (1u << 1),
  /** Format can be used to create buffer views
   *  with \c GfxUsage::eShaderResource usage. */
  eResourceBuffer       = (1u << 2),
  /** Format can be used to create buffer views
   *  with \c GfxUsage::eShaderStorage usage. */
  eStorageBuffer        = (1u << 3),
  /** Format can be used for ray tracing geometry */
  eBvhGeometry          = (1u << 4),
  /** Format can be used to create images or image
   *  views with \c GfxUsage::eShaderResource usage. */
  eResourceImage        = (1u << 5),
  /** Format can be used to create images or image
   *  views with \c GfxUsage::eShaderStorage usage. */
  eStorageImage         = (1u << 6),
  /** Format can be used to create images or image
   *  views with \c GfxUsage::eRenderTarget usage. */
  eRenderTarget         = (1u << 7),
  /** Format can be used to create images or image
   *  views with \c GfxUsage::eShadingRate usage. */
  eShadingRate          = (1u << 8),
  /** Format supports storage image reads without the
   *  format being specified in the shader. This will
   *  be set for some common formats. */
  eShaderStorageRead    = (1u << 9),
  /** Format supports atomic shader operations */
  eShaderStorageAtomic  = (1u << 10),
  /** Format can be sampled with a linear filter */
  eSampleLinear         = (1u << 11),

  eFlagEnum             = 0
};

using GfxFormatFeatures = Flags<GfxFormatFeature>;


/**
 * \brief Graphics device interface
 *
 * The device primarily facilitates object creation
 * and command submission.
 */
class GfxDeviceIface {

public:

  virtual ~GfxDeviceIface() { }

  /**
   * \brief Queries shader format info
   * \returns Shader format info
   */
  virtual GfxShaderFormatInfo getShaderInfo() const = 0;

  /**
   * \brief Queries device features
   * \returns Supported device capabilities
   */
  virtual GfxDeviceFeatures getFeatures() const = 0;

  /**
   * \brief Queries format features
   *
   * If this returns 0, the format is unsupported. If only
   * buffer bits are supported, images must not be created
   * with this format.
   * \param [in] format Format to query
   * \returns Format features
   */
  virtual GfxFormatFeatures getFormatFeatures(
          GfxFormat                     format) const = 0;

  /**
   * \brief Checks whether the given shading rate is supported
   *
   * If the fragment shading rate feature is supported, only 1x1,
   * 2x1 and 2x2 are guaranteed to be supported, and support may
   * futher vary with different sample counts.
   * Otherwise, only 1x1 is reported as supported.
   * \param [in] extent Desired fragment size
   * \param [in] samples Render target sample count
   * \returns \c true if the shading rate is supported
   *    for the given sample count.
   */
  virtual bool supportsShadingRate(
          Extent2D                      extent,
          uint32_t                      samples) const = 0;

  /**
   * \brief Computes allocation size of geometry BVH
   *
   * \param [in] desc Geometry description
   * \returns Required size for the BVH
   */
  virtual uint64_t computeRayTracingBvhSize(
    const GfxRayTracingGeometryDesc&    desc) const = 0;

  /**
   * \brief Computes allocation size of instance BVH
   *
   * \param [in] desc Instance description
   * \returns Required size for the BVH
   */
  virtual uint64_t computeRayTracingBvhSize(
    const GfxRayTracingInstanceDesc&    desc) const = 0;

  /**
   * \brief Creates buffer
   *
   * If memory for the resource cannot be allocated from
   * the given allocator on the desired memory types, this
   * method will return a \c nullptr buffer.
   * \param [in] desc Buffer properties
   * \param [in] memoryTypes Allowed memory types
   * \returns Buffer object
   */
  virtual GfxBuffer createBuffer(
    const GfxBufferDesc&                desc,
          GfxMemoryTypes                memoryTypes) = 0;

  /**
   * \brief Creates a compute pipeline
   *
   * The pipeline will be compiled in the background.
   * \param [in] desc Pipeline description
   * \returns Compute pipeline object
   */
  virtual GfxComputePipeline createComputePipeline(
    const GfxComputePipelineDesc&       desc) = 0;

  /**
   * \brief Creates a context for the given queue
   *
   * Command lists from the give context must
   * only be submitted to the given queue.
   * \param [in] queue Target queue
   * \returns Context object
   */
  virtual GfxContext createContext(
          GfxQueue                      queue) = 0;

  /**
   * \brief Creates a descriptor array
   *
   * All descriptors in the returned descriptor array
   * will be initialized with null descriptors.
   * \param [in] desc Descriptor array properties
   * \returns Descriptor array object
   */
  virtual GfxDescriptorArray createDescriptorArray(
    const GfxDescriptorArrayDesc&       desc) = 0;

  /**
   * \brief Creates a legacy graphics pipeline
   *
   * Creates a pipeline with a legacy vertex shader.
   * The pipeline will be compiled in the background.
   * \param [in] desc Pipeline description
   * \returns Graphics pipeline object
   */
  virtual GfxGraphicsPipeline createGraphicsPipeline(
    const GfxGraphicsPipelineDesc&      desc) = 0;

  /**
   * \brief Creates a mesh shading graphics pipeline
   *
   * Creates a graphics pipeline with a mesh shader.
   * The pipeline will be compiled in the background.
   * \param [in] desc Pipeline description
   * \returns Graphics pipeline object
   */
  virtual GfxGraphicsPipeline createGraphicsPipeline(
    const GfxMeshPipelineDesc&          desc) = 0;

  /**
   * \brief Creates an image resource
   *
   * If memory for the resource cannot be allocated from
   * the given allocator on the desired memory types, this
   * method will return a \c nullptr image.
   * \param [in] desc Image properties
   * \param [in] memoryTypes Allowed memory types
   * \returns Image object
   */
  virtual GfxImage createImage(
    const GfxImageDesc&                 desc,
          GfxMemoryTypes                memoryTypes) = 0;

  /**
   * \brief Creates presenter for a given window
   *
   * \param [in] desc Presenter properties
   * \returns Presenter object
   */
  virtual GfxPresenter createPresenter(
    const GfxPresenterDesc&             desc) = 0;

  /**
   * \brief Creates geometry BVH
   *
   * BVHs will always be allocated in device memory. The size is
   * not known up-front and memory will be allocated dynamically.
   * If the exact size needs to be known beforehand, use the
   * \c computeRayTracingBvhSize method.
   * \param [in] desc Geometry description
   * \returns Newly created ray tracing BVH
   */
  virtual GfxRayTracingBvh createRayTracingBvh(
    const GfxRayTracingGeometryDesc&    desc) = 0;

  /**
   * \brief Creates instance BVH
   *
   * \param [in] desc Instance description
   * \returns Newly created ray tracing BVH
   */
  virtual GfxRayTracingBvh createRayTracingBvh(
    const GfxRayTracingInstanceDesc&    desc) = 0;

  /**
   * \brief Creates a render state object
   *
   * \param [in] desc Render state description
   * \returns Render state object
   */
  virtual GfxRenderState createRenderState(
    const GfxRenderStateDesc&     desc) = 0;

  /**
   * \brief Creates a render target state object
   *
   * \param [in] desc Render target properties
   * \returns Render target state object
   */
  virtual GfxRenderTargetState createRenderTargetState(
    const GfxRenderTargetStateDesc&     desc) = 0;

  /**
   * \brief Creates a sampler object
   *
   * Note that samplers are treated as resources rather than state
   * objects. This means that the number of sampler objects that
   * can be created from the device is finite, and applications
   * should make an effort to deduplicate sampler objects.
   * \param [in] desc Sampler properties
   * \returns Sampler object
   */
  virtual GfxSampler createSampler(
    const GfxSamplerDesc&               desc) = 0;

  /**
   * \brief Creates a semaphore
   *
   * \param [in] desc Semaphore properties
   * \returns Semaphore object
   */
  virtual GfxSemaphore createSemaphore(
    const GfxSemaphoreDesc&             desc) = 0;

  /**
   * \brief Submits commands to a device queue
   *
   * This function will consume the submission object and
   * reset it to an empty state, in order to ensure that
   * no command lists are submitted multiple times.
   *
   * All command lits in the submission must have been
   * explicitly created for the given queue.
   * \param [in] queue Submission queue
   * \param [in] submission Submission object
   */
  virtual void submit(
          GfxQueue                      queue,
          GfxCommandSubmission&&        submission) = 0;

  /**
   * \brief Waits for all pending submissions to complete
   *
   * Stalls the calling thread until all pending submissions have
   * completed on the GPU, and blocks new submissions. This may be
   * useful during teardown in order to avoid having to track
   * resource lifetimes on presentation command lists.
   */
  virtual void waitIdle() = 0;

};

/** See GfxDeviceIface. */
using GfxDevice = IfaceRef<GfxDeviceIface>;

}
