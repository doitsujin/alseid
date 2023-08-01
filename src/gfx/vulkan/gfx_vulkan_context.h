#pragma once

#include <memory>
#include <vector>

#include "../gfx_context.h"
#include "../gfx_scratch.h"

#include "gfx_vulkan_barrier.h"
#include "gfx_vulkan_descriptor_pool.h"
#include "gfx_vulkan_device.h"
#include "gfx_vulkan_image.h"
#include "gfx_vulkan_loader.h"

namespace as {

/**
 * \brief Vulkan context flags
 */
enum class GfxVulkanContextFlag : uint32_t {
  eDirtyPipeline      = (1u << 0),
  eDirtyConstants     = (1u << 1),
  eDirtyIndexBuffer   = (1u << 2),
  eFlagEnum           = 0
};


using GfxVulkanContextFlags = Flags<GfxVulkanContextFlag>;

/**
 * \brief Vulkan device
 */
class GfxVulkanContext : public GfxContextIface {

public:

  GfxVulkanContext(
          std::shared_ptr<GfxVulkanDevice> device,
          GfxQueue                      queue);

  ~GfxVulkanContext();

  /**
   * \brief Ends current command list
   * \returns The resulting command list
   */
  GfxCommandList endCommandList() override;

  /**
   * \brief Resets context
   */
  void reset() override;

  /**
   * \brief Inserts a debug label
   *
   * \param [in] text Label text
   * \param [in] color Label color
   */
  void insertDebugLabel(
    const char*                         text,
          GfxColorValue                 color) override;

  /**
   * \brief Begins a scoped debug label
   *
   * \param [in] text Label text
   * \param [in] color Label color
   */
  void beginDebugLabel(
    const char*                         text,
          GfxColorValue                 color) override;

  /**
   * \brief Ends a scoped debug label
   */
  void endDebugLabel() override;

  /**
   * \brief Allocates scratch memory
   *
   * \param [in] usage Desired usage of the memory slice. The returned
   *    slice will be aligned to match the requirements of this usage bit.
   * \param [in] cpuAccess Desired CPU access
   * \param [in] size Number of bytes to allocate
   * \returns Allocated buffer slice
   */
  GfxScratchBuffer allocScratch(
          GfxUsageFlags                 usage,
          uint64_t                      size) override;

  /**
   * \brief Begins render pass
   *
   * \param [in] renderingInfo Render target info
   * \param [in] flags Rendering flags
   */
  void beginRendering(
    const GfxRenderingInfo&             renderingInfo,
          GfxRenderingFlags             flags) override;

  /**
   * \brief Ends render pass
   */
  void endRendering() override;

  /**
   * \brief Emits a global memory barrier
   *
   * Barriers \e must not be used inside a render pass.
   * \param [in] srcUsage Source commands to wait for
   * \param [in] srcStages Shader stages used in source commands
   * \param [in] dstUsage Destination commands to stall
   * \param [in] dstStages Shader stages used in destination commands
   */
  void memoryBarrier(
          GfxUsageFlags                 srcUsage,
          GfxShaderStages               srcStages,
          GfxUsageFlags                 dstUsage,
          GfxShaderStages               dstStages) override;

  /**
   * \brief Emits an image memory barrier
   *
   * \param [in] image Image to transition
   * \param [in] subresource Subresources to transition
   * \param [in] srcUsage Previous image usage
   * \param [in] srcStages Shader stages used in source commands
   * \param [in] dstUsage New image usage
   * \param [in] dstStages Shader stages used in destination commands
   * \param [in] flags Image transition flags
   */
  void imageBarrier(
    const GfxImage&                     image,
    const GfxImageSubresource&          subresource,
          GfxUsageFlags                 srcUsage,
          GfxShaderStages               srcStages,
          GfxUsageFlags                 dstUsage,
          GfxShaderStages               dstStages,
          GfxBarrierFlags               flags) override;

  /**
   * \brief Acquires image from another queue
   *
   * \param [in] image Image to acquire
   * \param [in] subresource Subresources to acquire
   * \param [in] srcQueue Queue that the image was released from
   * \param [in] srcUsage Last image usage on the source queue
   * \param [in] dstUsage Desired image usage and destination commands
   * \param [in] dstStages Shader stages used in destination commands
   */
  void acquireImage(
    const GfxImage&                     image,
    const GfxImageSubresource&          subresource,
          GfxQueue                      srcQueue,
          GfxUsageFlags                 srcUsage,
          GfxUsageFlags                 dstUsage,
          GfxShaderStages               dstStages) override;

  /**
   * \brief Releases image so it can be used on another queue
   *
   * \param [in] image Image to acquire
   * \param [in] subresource Subresources to acquire
   * \param [in] srcUsage Current image usage and source commands
   * \param [in] srcStages Shader stages used in source commands
   * \param [in] dstQueue Queue that the image will be used on
   * \param [in] dstUsage Desired image usage on destination queue
   */
  void releaseImage(
    const GfxImage&                     image,
    const GfxImageSubresource&          subresource,
          GfxUsageFlags                 srcUsage,
          GfxShaderStages               srcStages,
          GfxQueue                      dstQueue,
          GfxUsageFlags                 dstUsage) override;

  /**
   * \brief Binds compute pipeline to the context
   * \param [in] pipeline Pipeline to bind
   */
  void bindPipeline(
          GfxComputePipeline            pipeline) override;

  /**
   * \brief Binds graphics pipeline to the context
   * \param [in] pipeline Pipeline to bind
   */
  void bindPipeline(
          GfxGraphicsPipeline           pipeline) override;

  /**
   * \brief Binds descriptor array to a given set
   *
   * \param [in] set Descriptor set index
   * \param [in] array Descriptor array
   */
  void bindDescriptorArray(
          uint32_t                      set,
    const GfxDescriptorArray&           array) override;

  /**
   * \brief Sets descriptors for a given set
   *
   * \param [in] set Descriptor set index
   * \param [in] index First descriptor to set
   * \param [in] count Number of descriptors to set
   * \param [in] descriptors Descriptors
   */
  void bindDescriptors(
          uint32_t                      set,
          uint32_t                      index,
          uint32_t                      count,
    const GfxDescriptor*                descriptors) override;

  /**
   * \brief Sets index buffer
   *
   * \param [in] descriptor Index buffer descriptor
   * \param [in] format Index format
   */
  void bindIndexBuffer(
    const GfxDescriptor&                descriptor,
          GfxFormat                     format) override;

  /**
   * \brief Sets vertex buffers
   *
   * \param [in] index First vertex buffer to set
   * \param [in] count Number of vertex buffers to set
   * \param [in] descriptors Vertex buffer descriptors
   * \param [in] strides Vertex buffer strides
   */
  void bindVertexBuffers(
          uint32_t                      index,
          uint32_t                      count,
    const GfxDescriptor*                descriptors) override;

  /**
   * \brief Builds a BVH
   *
   * \param [in] bvh Ray tracing BVH to build or update
   * \param [in] mode Whether to re-build or update the BVH
   * \param [in] data Data sources, with one entry per geometry
   *    or per set of instances as defined during BVH creation.
   */
  void buildRayTracingBvh(
    const GfxRayTracingBvh&             bvh,
          GfxRayTracingBvhBuildMode     mode,
    const GfxRayTracingBvhData*         data) override;

  /**
   * \brief Copies buffer data
   *
   * \param [in] dstBuffer Buffer to write to
   * \param [in] dstOffset Destination offset, in bytes
   * \param [in] srcBuffer Buffer to read from
   * \param [in] srcOffset Source offset, in bytes
   * \param [in] size Number of bytes to copy
   */
  void copyBuffer(
    const GfxBuffer&                    dstBuffer,
          uint64_t                      dstOffset,
    const GfxBuffer&                    srcBuffer,
          uint64_t                      srcOffset,
          uint64_t                      size) override;

  /**
   * \brief Copies buffer data to an image
   *
   * \param [in] image Image to copy to
   * \param [in] imageSubresource Image subresource.
   *    Must only contain one aspect and mip level.
   * \param [in] imageOffset Offset of the image area
   * \param [in] imageExtent Size of the area to copy
   * \param [in] buffer Buffer to read from
   * \param [in] bufferOffset Buffer offset, in bytes
   * \param [in] bufferLayout Extent of the image data
   *    within the buffer, in pixels.
   */
  void copyBufferToImage(
    const GfxImage&                     image,
    const GfxImageSubresource&          imageSubresource,
          Offset3D                      imageOffset,
          Extent3D                      imageExtent,
    const GfxBuffer&                    buffer,
          uint64_t                      bufferOffset,
          Extent2D                      bufferLayout) override;

  /**
   * \brief Copies image data
   *
   * \param [in] dstImage Image to write to
   * \param [in] dstSubresource Destination subresources
   * \param [in] dstOffset Destination image offset
   * \param [in] srcImage Image to read from
   * \param [in] srcSubresource Source subresources
   * \param [in] srcOffset Source image offset
   * \param [in] extent Extent of the area to copy
   */
  void copyImage(
    const GfxImage&                     dstImage,
    const GfxImageSubresource&          dstSubresource,
          Offset3D                      dstOffset,
    const GfxImage&                     srcImage,
    const GfxImageSubresource&          srcSubresource,
          Offset3D                      srcOffset,
          Extent3D                      extent) override;

  /**
   * \brief Copies image data to a buffer
   *
   * \param [in] buffer Buffer to write to
   * \param [in] bufferOffset Buffer offset, in bytes
   * \param [in] bufferLayout Extent of the image data
   *    within the buffer, in pixels.
   * \param [in] image Image to copy from
   * \param [in] imageSubresource Image subresource.
   *    Must only contain one aspect and mip level.
   * \param [in] imageOffset Offset of the image area
   * \param [in] imageExtent Size of the area to copy
   */
  void copyImageToBuffer(
    const GfxBuffer&                    buffer,
          uint64_t                      bufferOffset,
          Extent2D                      bufferLayout,
    const GfxImage&                     image,
    const GfxImageSubresource&          imageSubresource,
          Offset3D                      imageOffset,
          Extent3D                      imageExtent) override;

  /**
   * \brief Decompresses a buffer
   *
   * \param [in] dstBuffer Output buffer
   * \param [in] dstOffset Output buffer offset
   * \param [in] dstSize Decompressed data size
   * \param [in] srcBuffer Compressed buffer
   * \param [in] srcOffset Compressed buffer offset
   * \param [in] srcSize Compressed data size
   */
  void decompressBuffer(
    const GfxBuffer&                    dstBuffer,
          uint64_t                      dstOffset,
          uint64_t                      dstSize,
    const GfxBuffer&                    srcBuffer,
          uint64_t                      srcOffset,
          uint64_t                      srcSize) override;

  /**
   * \brief Executes a compute dispatch
   * \param workgroupCount Workgroup count vector
   */
  void dispatch(
          Extent3D                      workgroupCount) override;

  /**
   * \brief Executes an indirect compute dispatch
   * \param [in] args Argument buffer descriptor
   */
  void dispatchIndirect(
    const GfxDescriptor&                args) override;

  /**
   * \brief Executes a non-indexed draw
   *
   * \param [in] vertexCount Number of vertices
   * \param [in] instanceCount Number of instances
   * \param [in] firstVertex First vertex index
   * \param [in] firstInstance First instance index
   */
  void draw(
          uint32_t                      vertexCount,
          uint32_t                      instanceCount,
          uint32_t                      firstVertex,
          uint32_t                      firstInstance) override;

  /**
   * \brief Executes an indirect draw
   *
   * \param [in] args Argument buffer descriptor
   * \param [in] count Count buffer descriptor
   * \param [in] maxCount Maximum number of draws
   */
  void drawIndirect(
    const GfxDescriptor&                args,
    const GfxDescriptor&                count,
          uint32_t                      maxCount) override;

  /**
   * \brief Executes an indexed draw
   *
   * \param [in] indexCount Number of indices
   * \param [in] instanceCount Number of instances
   * \param [in] firstIndex Offset into index buffer, in index elements
   * \param [in] firstVertex Vertex index to add to index buffer values
   * \param [in] firstInstance First instance index
   */
  void drawIndexed(
          uint32_t                      indexCount,
          uint32_t                      instanceCount,
          uint32_t                      firstIndex,
          int32_t                       firstVertex,
          uint32_t                      firstInstance) override;

  /**
   * \brief Executes an indirect indexed draw
   *
   * \param [in] args Argument buffer descriptor
   * \param [in] count Count buffer descriptor
   * \param [in] maxCount Maximum number of draws
   */
  void drawIndexedIndirect(
    const GfxDescriptor&                args,
    const GfxDescriptor&                count,
          uint32_t                      maxCount) override;

  /**
   * \brief Executes a mesh shader draw
   * \param workgroupCount Workgroup count vector
   */
  void drawMesh(
          Extent3D                      workgroupCount) override;

  /**
   * \brief Executes an indirect mesh shader draw
   *
   * \param [in] args Argument buffer descriptor
   * \param [in] count Count buffer descriptor
   * \param [in] maxCount Maximum number of draws
   */
  void drawMeshIndirect(
    const GfxDescriptor&                args,
    const GfxDescriptor&                count,
          uint32_t                      maxCount) override;

  /**
   * \brief Sets blend constants
   * \param [in] constants Blend constants
   */
  void setBlendConstants(
          GfxColorValue                 constants) override;

  /**
   * \brief Sets blend state
   * \param [in] state Blend state object
   */
  void setColorBlendState(
          GfxColorBlendState            state) override;

  /**
   * \brief Sets depth bounds
   *
   * \param [in] minDepth Minimum depth value
   * \param [in] maxDepth Maximum depth value
   */
  void setDepthBounds(
          float                         minDepth,
          float                         maxDepth) override;

  /**
   * \brief Sets depth-stencil state
   * \param [in] state Depth-stencil state object
   */
  void setDepthStencilState(
          GfxDepthStencilState          state) override;

  /**
   * \brief Sets multisample state
   * \param [in] state Multisample state object
   */
  void setMultisampleState(
          GfxMultisampleState           state) override;

  /**
   * \brief Sets rasterizer state
   * \param [in] state Rasterizer state object
   */
  void setRasterizerState(
          GfxRasterizerState            state) override;

  /**
   * \brief Sets render state
   * \param [in] state Render state object
   */
  void setRenderState(
          GfxRenderState                state) override;

  /**
   * \brief Sets shader constants
   *
   * \param [in] offset Offset into constant block
   * \param [in] size Number of bytes to set
   * \param [in] data Constant data
   */
  void setShaderConstants(
          uint32_t                      offset,
          uint32_t                      size,
    const void*                         data) override;

  /**
   * \brief Sets stencil reference
   *
   * \param [in] front Front face stencil reference
   * \param [in] back Back face stencil reference
   */
  void setStencilReference(
          uint32_t                      front,
          uint32_t                      back) override;

  /**
   * \brief Sets vertex input state
   * \param [in] state Vertex input state object
   */
  void setVertexInputState(
          GfxVertexInputState           state) override;

  /**
   * \brief Sets viewports
   *
   * \param [in] count Number of viewports to set
   * \param [in] viewports Viewport array
   */
  void setViewports(
          uint32_t                      count,
    const GfxViewport*                  viewports) override;

private:

  std::shared_ptr<GfxVulkanDevice>  m_device;
  GfxQueue                          m_queue;

  VkCommandPool   m_commandPool   = VK_NULL_HANDLE;
  VkCommandBuffer m_cmd           = VK_NULL_HANDLE;

  std::vector<VkCommandBuffer>      m_commandBuffers;
  size_t                            m_commandBufferIndex = 0;

  GfxVulkanBarrierBatch             m_barrierBatch;

  GfxVulkanContextFlags             m_flags     = 0;
  uint32_t                          m_dirtySets = 0;

  GfxGraphicsStateDesc              m_defaultState;
  GfxGraphicsStateDesc              m_graphicsState;
  GfxRenderStateData                m_renderState;
  const GfxVulkanRenderState*       m_renderStateObject = nullptr;
  GfxVulkanGraphicsPipeline*        m_graphicsPipeline;
  GfxVulkanComputePipeline*         m_computePipeline;

  GfxVulkanDynamicStates            m_dynamicStatesActive = 0;
  GfxVulkanDynamicStates            m_dynamicStatesDirty = 0;

  uint32_t                          m_viewportCount = 1;
  std::array<VkViewport,  GfxMaxViewportCount> m_viewports = { };
  std::array<VkRect2D,    GfxMaxViewportCount> m_scissors = { };

  VkBuffer                          m_indexBufferHandle = VK_NULL_HANDLE;
  VkDeviceSize                      m_indexBufferOffset = 0;
  VkIndexType                       m_indexBufferFormat = VK_INDEX_TYPE_UINT16;

  uint32_t                          m_vbosDirty = 0;
  uint32_t                          m_vbosActive = 0;

  std::array<VkBuffer,      GfxMaxVertexBindings> m_vertexBufferHandles = { };
  std::array<VkDeviceSize,  GfxMaxVertexBindings> m_vertexBufferOffsets = { };
  std::array<VkDeviceSize,  GfxMaxVertexBindings> m_vertexBufferSizes = { };

  float                             m_depthBoundsMin  = 1.0f;
  float                             m_depthBoundsMax  = 1.0f;

  uint32_t                          m_stencilRefBack  = 0;
  uint32_t                          m_stencilRefFront = 0;

  VkClearColorValue                 m_blendConstants = { };

  std::vector<std::shared_ptr<GfxVulkanDescriptorPool>> m_descriptorPools;

  std::array<VkDescriptorSet,     GfxMaxDescriptorSets>                           m_descriptorArrays  = { };
  std::array<GfxVulkanDescriptor, GfxMaxDescriptorSets * GfxMaxDescriptorsPerSet> m_descriptors       = { };

  std::vector<GfxScratchBufferPage> m_scratchPages;

  alignas(16) std::array<char, 256> m_shaderConstants = { };

  void updateGraphicsState(
    const GfxVulkanProcs&               vk,
          bool                          indexed);

  void updateComputeState(
    const GfxVulkanProcs&               vk);

  void updateDescriptorSets(
          VkPipelineBindPoint           bindPoint,
    const GfxVulkanPipelineLayout*      pipelineLayout);

  void updatePushConstants(
    const GfxVulkanPipelineLayout*      pipelineLayout);

  void computeDirtySets(
    const GfxVulkanPipelineLayout*      oldLayout,
    const GfxVulkanPipelineLayout*      newLayout);

  void invalidateState();

  void resetState();

  VkCommandBuffer allocateCommandBuffer();

  void allocateDescriptorSets(
          uint32_t                      setCount,
    const VkDescriptorSetLayout*        setLayouts,
          VkDescriptorSet*              sets);

  static small_vector<VkBufferImageCopy2, 16> getVkBufferImageCopyRegions(
    const GfxImage&                     image,
    const GfxImageSubresource&          imageSubresource,
          Offset3D                      imageOffset,
          Extent3D                      imageExtent,
    const GfxBuffer&                    buffer,
          uint64_t                      bufferOffset,
          Extent2D                      bufferLayout);

  static std::pair<VkPipelineStageFlags2, VkAccessFlags2> getVkStageAccessFromUsage(
          GfxUsageFlags                 gfxUsage,
          GfxShaderStages               gfxStages);

};

}
