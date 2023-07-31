#pragma once

#include <array>
#include <memory>
#include <vector>

#include "../gfx_device.h"

namespace as {

/**
 * \brief Debug device
 *
 * Wrapper around a native device that performs validation
 * for debugging purposes. All objects created from a debug
 * device will also be wrapped.
 */
class GfxDebugDevice : public GfxDeviceIface {

public:

  GfxDebugDevice(
          GfxDevice&&                   device);

  ~GfxDebugDevice();

  /**
   * \brief Queries shader format info
   * \returns Shader format info
   */
  GfxShaderFormatInfo getShaderInfo() const override;

  /**
   * \brief Queries device features
   * \returns Supported device capabilities
   */
  GfxDeviceFeatures getFeatures() const override;

  /**
   * \brief Queries format features
   *
   * \param [in] format Format to query
   * \returns Format features
   */
  GfxFormatFeatures getFormatFeatures(
          GfxFormat                     format) const override;

  /**
   * \brief Checks whether the given shading rate is supported
   *
   * \param [in] shadingRate Desired fragment size
   * \param [in] samples Render target sample count
   * \returns \c true if the shading rate is supported
   *    for the given sample count.
   */
  bool supportsShadingRate(
          Extent2D                      shadingRate,
          uint32_t                      samples) const override;

  /**
   * \brief Computes allocation size of geometry BVH
   *
   * \param [in] desc Geometry description
   * \returns Required size for the BVH
   */
  uint64_t computeRayTracingBvhSize(
    const GfxRayTracingGeometryDesc&    desc) const override;

  /**
   * \brief Computes allocation size of instance BVH
   *
   * \param [in] desc Instance description
   * \returns Required size for the BVH
   */
  uint64_t computeRayTracingBvhSize(
    const GfxRayTracingInstanceDesc&    desc) const override;

  /**
   * \brief Creates buffer
   *
   * \param [in] desc Buffer properties
   * \param [in] memoryTypes Allowed memory types
   * \returns Buffer object
   */
  GfxBuffer createBuffer(
    const GfxBufferDesc&                desc,
          GfxMemoryTypes                memoryTypes) override;

  /**
   * \brief Creates a color blend state object
   *
   * \param [in] desc Color blend properties
   * \returns Color blend state object
   */
  GfxColorBlendState createColorBlendState(
    const GfxColorBlendStateDesc&       desc) override;

  /**
   * \brief Creates a compute pipeline
   *
   * \param [in] desc Pipeline description
   * \returns Compute pipeline object
   */
  GfxComputePipeline createComputePipeline(
    const GfxComputePipelineDesc&       desc) override;

  /**
   * \brief Creates a context for the given queue
   *
   * \param [in] queue Target queue
   * \returns Wrapped debug context
   */
  GfxContext createContext(
          GfxQueue                      queue) override;

  /**
   * \brief Creates a depth-stencil state object
   *
   * \param [in] desc Depth-stencil properties
   * \returns Depth-stencil state object
   */
  GfxDepthStencilState createDepthStencilState(
    const GfxDepthStencilStateDesc&     desc) override;

  /**
   * \brief Creates a descriptor array
   *
   * \param [in] desc Descriptor array properties
   * \returns Descriptor array object
   */
  GfxDescriptorArray createDescriptorArray(
    const GfxDescriptorArrayDesc&       desc) override;

  /**
   * \brief Creates a legacy graphics pipeline
   *
   * \param [in] desc Pipeline description
   * \returns Graphics pipeline object
   */
  GfxGraphicsPipeline createGraphicsPipeline(
    const GfxGraphicsPipelineDesc&      desc) override;

  /**
   * \brief Creates a mesh shading graphics pipeline
   *
   * Creates a graphics pipeline with a mesh shader.
   * The pipeline will be compiled in the background.
   * \param [in] desc Pipeline description
   * \returns Graphics pipeline object
   */
  GfxGraphicsPipeline createGraphicsPipeline(
    const GfxMeshPipelineDesc&          desc) override;

  /**
   * \brief Creates an image resource
   *
   * \param [in] desc Image properties
   * \param [in] memoryTypes Allowed memory types
   * \returns Image object
   */
  GfxImage createImage(
    const GfxImageDesc&                 desc,
          GfxMemoryTypes                memoryTypes) override;

  /**
   * \brief Creates a multisample state object
   *
   * \param [in] desc Multisample properties
   * \returns Multisample state object
   */
  GfxMultisampleState createMultisampleState(
    const GfxMultisampleStateDesc&      desc) override;

  /**
   * \brief Creates presenter for a given window
   *
   * \param [in] desc Presenter properties
   * \returns Presenter object
   */
  GfxPresenter createPresenter(
    const GfxPresenterDesc&             desc) override;

  /**
   * \brief Creates a rasterizer state object
   *
   * \param [in] desc Rasterizer properties
   * \returns Rasterizer state object
   */
  GfxRasterizerState createRasterizerState(
    const GfxRasterizerStateDesc&       desc) override;

  /**
   * \brief Creates geometry BVH
   *
   * \param [in] desc Geometry description
   * \returns Newly created ray tracing BVH
   */
  GfxRayTracingBvh createRayTracingBvh(
    const GfxRayTracingGeometryDesc&    desc) override;

  /**
   * \brief Creates instance BVH
   *
   * \param [in] desc Instance description
   * \returns Newly created ray tracing BVH
   */
  GfxRayTracingBvh createRayTracingBvh(
    const GfxRayTracingInstanceDesc&    desc) override;

  /**
   * \brief Creates render state object
   *
   * \param [in] desc Render state description
   * \returns Render state object
   */
  GfxRenderState createRenderState(
    const GfxRenderStateDesc&           desc) override;

  /**
   * \brief Creates a render target state object
   *
   * \param [in] desc Render target properties
   * \returns Render target state object
   */
  GfxRenderTargetState createRenderTargetState(
    const GfxRenderTargetStateDesc&     desc) override;

  /**
   * \brief Creates a sampler object
   *
   * \param [in] desc Sampler properties
   * \returns Sampler object
   */
  GfxSampler createSampler(
    const GfxSamplerDesc&               desc) override;

  /**
   * \brief Creates a semaphore
   *
   * \param [in] desc Semaphore properties
   * \returns Semaphore object
   */
  GfxSemaphore createSemaphore(
    const GfxSemaphoreDesc&             desc) override;

  /**
   * \brief Creates a vertex input state object
   *
   * \param [in] desc Vertex input properties
   * \returns Vertex input state object
   */
  GfxVertexInputState createVertexInputState(
    const GfxVertexInputStateDesc&      desc) override;

  /**
   * \brief Submits commands to a device queue
   *
   * \param [in] queue Submission queue
   * \param [in] submission Submission object
   */
  void submit(
          GfxQueue                      queue,
          GfxCommandSubmission&&        submission) override;

  /**
   * \brief Waits for all pending submissions to complete
   */
  void waitIdle() override;

private:

  GfxDevice m_device;

};

}
