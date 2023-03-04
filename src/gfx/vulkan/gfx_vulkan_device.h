#pragma once

#include <array>
#include <memory>
#include <vector>

#include "../gfx.h"
#include "../gfx_device.h"
#include "../gfx_scratch.h"

#include "gfx_vulkan.h"
#include "gfx_vulkan_descriptor_pool.h"
#include "gfx_vulkan_format.h"
#include "gfx_vulkan_gdeflate.h"
#include "gfx_vulkan_loader.h"
#include "gfx_vulkan_memory.h"
#include "gfx_vulkan_pipeline.h"
#include "gfx_vulkan_queue.h"
#include "gfx_vulkan_ray_tracing.h"

namespace as {

/**
 * \brief Vulkan device
 */
class GfxVulkanDevice : public GfxDeviceIface
, public std::enable_shared_from_this<GfxVulkanDevice> {
public:

  GfxVulkanDevice(
          std::shared_ptr<GfxVulkan>    gfx,
          VkPhysicalDevice              adapter);

  ~GfxVulkanDevice();

  /**
   * \brief Queries Vulkan functions
   * \returns Vulkan functions
   */
  const GfxVulkanProcs& vk() const {
    return m_vk;
  }

  /**
   * \brief Queries enabled Vulkan extensions
   * \returns Enabled extensions
   */
  const GfxVulkanDeviceExtensions& getVkExtensions() const {
    return m_extensions;
  }

  /**
   * \brief Queries Vulkan device properties
   * \returns Device properties
   */
  const GfxVulkanDeviceProperties& getVkProperties() const {
    return m_properties;
  }

  /**
   * \brief Queries enabled Vulkan features
   * \returns Vulkan device features
   */
  const GfxVulkanDeviceFeatures& getVkFeatures() const {
    return m_features;
  }

  /**
   * \brief Checks whether debug markers are enabled
   *
   * Used internally to check whether methods
   * to assign debug names should be called.
   * \returns \c true if the device is in debug mode
   */
  bool isDebugDevice() const {
    return m_instanceFlags & GfxInstanceFlag::eDebugMarkers;
  }

  /**
   * \brief Queries memory type masks
   * \returns Memory type masks
   */
  GfxVulkanMemoryTypeMasks getMemoryTypeInfo() const {
    return m_memoryTypeMasks;
  }

  /**
   * \brief Queries queue family index of a given queue
   *
   * \param [in] queue Queue
   * \returns Queue family index
   */
  uint32_t getQueueFamilyIndex(
          GfxQueue                      queue) const {
    return m_queues[uint32_t(queue)].queueFamily;
  }

  /**
   * \brief Looks up Vulkan format for a common format
   *
   * \param [in] format Common format to look up
   * \returns Corresponding Vulkan format
   */
  VkFormat getVkFormat(
          GfxFormat                     format) const {
    return m_formatMap.getVkFormat(format);
  }

  /**
   * \brief Looks up common format for a Vulkan format
   *
   * This should only be used when absolutely needed.
   * \param [in] format Vulkan format to look up
   * \returns Corresponding common format
   */
  GfxFormat getGfxFormat(
          VkFormat                      format) const {
    return m_formatMap.getGfxFormat(format);
  }

  /**
   * \brief Retrieves descriptor pool manager
   * \returns Descriptor pool manager
   */
  GfxVulkanDescriptorPoolManager& getDescriptorPoolManager() const {
    return *m_descriptorPoolManager;
  }

  /**
   * \brief Retrieves pipeline manager
   * \returns Pipeline manager
   */
  GfxVulkanPipelineManager& getPipelineManager() const {
    return *m_pipelineManager;
  }

  /**
   * \brief Retrieves memory allocator
   * \returns Memory allocator
   */
  GfxVulkanMemoryAllocator& getMemoryAllocator() const {
    return *m_memoryAllocator;
  }

  /**
   * \brief Retrieves GDeflate pipeline
   * \returns GDeflate pipeline
   */
  GfxVulkanGDeflatePipeline& getGDeflatePipeline() const {
    return *m_gdeflatePipeline;
  }

  /**
   * \brief Allocates scratch buffer
   *
   * \param [in] pageCount Number of pages to allocate
   * \returns Scratch buffer page
   */
  GfxScratchBufferPage allocScratchMemory(
          GfxMemoryType                 memoryType,
          uint32_t                      pageCount) {
    return m_scratchBufferPool->allocPages(memoryType, pageCount);
  }

  /**
   * \brief Populates resource sharing mode info
   *
   * \param [out] sharingMode Sharing mode
   * \param [out] queueFamilyCount Number of queue families
   * \param [out] queueFamilies Pointer to queue families
   */
  void getQueueSharingInfo(
          VkSharingMode*                sharingMode,
          uint32_t*                     queueFamilyCount,
    const uint32_t**                    queueFamilies) const;

  /**
   * \brief Queries shader format info
   *
   * This will return info for the compressed SPIR-V
   * binaries as they are stored in archive files.
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
   * \returns Context object
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
   * \brief Creates a render target state object
   *
   * Render target states are created and bound automatically
   * when binding render targets. However, explicitly creating
   * objects is required when pre-compiling full pipelines.
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

  /**
   * \brief Presents a swap chain image
   *
   * \param [in] queue Present queue
   * \param [in] semaphore Binary wait semaphore
   * \param [in] swapchain Vulkan swap chain
   * \param [in] imageId Swap image ID
   * \returns Vulkan result of the operation
   */
  VkResult present(
          GfxQueue                      queue,
          VkSemaphore                   semaphore,
          VkSwapchainKHR                swapchain,
          uint32_t                      imageId);

  /**
   * \brief Waits for a queue to become idle
   * \param [in] queue Queue to wait for
   */
  void waitQueueIdle(
          GfxQueue                      queue);

  /**
   * \brief Sets debug name of a Vulkan object
   *
   * Does nothing if the device is not in debug mode.
   * \param [in] objectHandle Vulkan object handle
   * \param [in] debugName Debug name
   */
  template<typename T>
  void setDebugName(
          T                             objectHandle,
    const char*                         debugName) const {
    if ((m_instanceFlags & GfxInstanceFlag::eDebugMarkers) && debugName) {
      VkDebugUtilsObjectNameInfoEXT info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
      info.objectType = getVulkanObjectType(objectHandle);
      info.objectHandle = uint64_t(objectHandle);
      info.pObjectName = debugName;

      m_vk.vkSetDebugUtilsObjectNameEXT(m_vk.device, &info);
    }
  }

private:

  std::shared_ptr<GfxVulkan>          m_gfx;
  GfxInstanceFlags                    m_instanceFlags;

  GfxVulkanProcs                      m_vk;
  GfxVulkanDeviceExtensions           m_extensions;
  GfxVulkanDeviceProperties           m_properties;
  GfxVulkanDeviceFeatures             m_features;
  GfxVulkanFormatMap                  m_formatMap;

  GfxVulkanMemoryTypeMasks            m_memoryTypeMasks;

  std::unique_ptr<GfxVulkanPipelineManager>       m_pipelineManager;
  std::unique_ptr<GfxVulkanDescriptorPoolManager> m_descriptorPoolManager;
  std::unique_ptr<GfxVulkanMemoryAllocator>       m_memoryAllocator;
  std::unique_ptr<GfxScratchBufferPool>           m_scratchBufferPool;
  std::unique_ptr<GfxVulkanGDeflatePipeline>      m_gdeflatePipeline;

  std::mutex                          m_submissionMutex;

  std::array<GfxVulkanQueue,
    uint32_t(GfxQueue::eQueueCount)>  m_queues        = { };

  std::array<uint32_t,
    uint32_t(GfxQueue::eQueueCount)>  m_queueFamilies = { };
  uint32_t                            m_queueFamilyCount = 0;

  GfxVulkanMemoryTypeMasks getMemoryTypeMasks() const;

  GfxVulkanRayTracingBvhSize computeRayTracingBvhSize(
    const GfxVulkanRayTracingBvhInfo&   info) const;

  GfxRayTracingBvh createRayTracingBvh(
    const GfxRayTracingBvhDesc&         desc,
    const GfxVulkanRayTracingBvhSize&   size,
          GfxVulkanRayTracingBvhInfo&&  info);

};

}
