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
#include "gfx_sampler.h"
#include "gfx_semaphore.h"
#include "gfx_shader.h"
#include "gfx_submission.h"
#include "gfx_types.h"

namespace as {

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
   * \brief Creates buffer
   *
   * If memory for the resource cannot be allocated from
   * the given allocator on the desired memory types, this
   * method will return a \c nullptr buffer.
   * \param [in] desc Image properties
   * \param [in] memoryTypes Allowed memory types
   * \returns Buffer object
   */
  virtual GfxBuffer createBuffer(
    const GfxBufferDesc&                desc,
          GfxMemoryTypes                memoryTypes) = 0;

  /**
   * \brief Creates a color blend state object
   *
   * \param [in] desc Color blend properties
   * \returns Color blend state object
   */
  virtual GfxColorBlendState createColorBlendState(
    const GfxColorBlendStateDesc&       desc) = 0;

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
   * \brief Creates a depth-stencil state object
   *
   * \param [in] desc Depth-stencil properties
   * \returns Depth-stencil state object
   */
  virtual GfxDepthStencilState createDepthStencilState(
    const GfxDepthStencilStateDesc&     desc) = 0;

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
   * \brief Creates a multisample state object
   *
   * \param [in] desc Multisample properties
   * \returns Multisample state object
   */
  virtual GfxMultisampleState createMultisampleState(
    const GfxMultisampleStateDesc&      desc) = 0;

  /**
   * \brief Creates presenter for a given window
   *
   * \param [in] desc Presenter properties
   * \returns Presenter object
   */
  virtual GfxPresenter createPresenter(
    const GfxPresenterDesc&             desc) = 0;

  /**
   * \brief Creates a rasterizer state object
   *
   * \param [in] desc Rasterizer properties
   * \returns Rasterizer state object
   */
  virtual GfxRasterizerState createRasterizerState(
    const GfxRasterizerStateDesc&       desc) = 0;

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
   * \brief Creates a vertex input state object
   *
   * \param [in] desc Vertex input properties
   * \returns Vertex input state object
   */
  virtual GfxVertexInputState createVertexInputState(
    const GfxVertexInputStateDesc&      desc) = 0;

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

using GfxDevice = IfaceRef<GfxDeviceIface>;

}
