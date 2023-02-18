#pragma once

#include "gfx_vulkan_include.h"

#define GFX_VK_LPROC(proc) GfxVulkanProc<PFN_ ## proc> proc = getLoaderProc(#proc)
#define GFX_VK_IPROC(proc) GfxVulkanProc<PFN_ ## proc> proc = getInstanceProc(#proc)
#define GFX_VK_DPROC(proc) GfxVulkanProc<PFN_ ## proc> proc = getDeviceProc(#proc)

namespace as {

template<typename T>
class GfxVulkanProc;

/**
 * \brief Vulkan function wrapper
 *
 * Basic wrapper type that implicitly converts function
 * pointers returned from \c vkGetInstanceProcAddr and
 * \c vkGetDeviceProcAddr to the correct type.
 */
template<typename Ret, typename... Args>
class GfxVulkanProc<Ret (VKAPI_PTR *)(Args...)> {
  using ptr_t = Ret (VKAPI_PTR *)(Args...);
public:

  GfxVulkanProc() { }
  GfxVulkanProc(ptr_t pfn)
  : m_proc(pfn) { }
  GfxVulkanProc(PFN_vkVoidFunction pfn)
  : m_proc(reinterpret_cast<ptr_t>(pfn)) { }

  Ret operator () (Args... args) const {
    return (*m_proc)(args...);
  }

  operator bool () const {
    return m_proc != nullptr;
  }

  void* getAddress() const {
    return reinterpret_cast<void*>(m_proc);
  }

private:

  ptr_t m_proc = nullptr;

};

/**
 * \brief Vulkan function table
 *
 * Loads function pointers from the Vulkan library itself,
 * for the specific Vulkan instance, and for a given device,
 * which reduces function call overhead and allows the use
 * of Vulkan extensions.
 */
class GfxVulkanProcs {

public:

  GfxVulkanProcs();
  explicit GfxVulkanProcs(PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr);
  GfxVulkanProcs(const GfxVulkanProcs& loader, VkInstance instanceHandle);
  GfxVulkanProcs(const GfxVulkanProcs& loader, VkPhysicalDevice adapterHandle, VkDevice deviceHandle);
  ~GfxVulkanProcs();

  VkInstance        instance  = VK_NULL_HANDLE;
  VkPhysicalDevice  adapter   = VK_NULL_HANDLE;
  VkDevice          device    = VK_NULL_HANDLE;

  GfxVulkanProc<PFN_vkGetInstanceProcAddr> vkGetInstanceProcAddr;

  GFX_VK_LPROC(vkCreateInstance);
  GFX_VK_LPROC(vkEnumerateInstanceExtensionProperties);
  GFX_VK_LPROC(vkEnumerateInstanceLayerProperties);
  GFX_VK_LPROC(vkEnumerateInstanceVersion);

  GFX_VK_IPROC(vkCreateDebugUtilsMessengerEXT);
  GFX_VK_IPROC(vkCreateDevice);
  GFX_VK_IPROC(vkDestroyDebugUtilsMessengerEXT);
  GFX_VK_IPROC(vkDestroyInstance);
  GFX_VK_IPROC(vkDestroySurfaceKHR);
  GFX_VK_IPROC(vkEnumerateDeviceExtensionProperties);
  GFX_VK_IPROC(vkEnumerateDeviceLayerProperties);
  GFX_VK_IPROC(vkEnumeratePhysicalDeviceGroups);
  GFX_VK_IPROC(vkEnumeratePhysicalDevices);
  GFX_VK_IPROC(vkGetDeviceProcAddr);
  GFX_VK_IPROC(vkGetPhysicalDeviceExternalBufferProperties);
  GFX_VK_IPROC(vkGetPhysicalDeviceExternalFenceProperties);
  GFX_VK_IPROC(vkGetPhysicalDeviceExternalSemaphoreProperties);
  GFX_VK_IPROC(vkGetPhysicalDeviceFeatures);
  GFX_VK_IPROC(vkGetPhysicalDeviceFeatures2);
  GFX_VK_IPROC(vkGetPhysicalDeviceFormatProperties);
  GFX_VK_IPROC(vkGetPhysicalDeviceFormatProperties2);
  GFX_VK_IPROC(vkGetPhysicalDeviceImageFormatProperties);
  GFX_VK_IPROC(vkGetPhysicalDeviceImageFormatProperties2);
  GFX_VK_IPROC(vkGetPhysicalDeviceMemoryProperties);
  GFX_VK_IPROC(vkGetPhysicalDeviceMemoryProperties2);
  GFX_VK_IPROC(vkGetPhysicalDevicePresentRectanglesKHR);
  GFX_VK_IPROC(vkGetPhysicalDeviceProperties);
  GFX_VK_IPROC(vkGetPhysicalDeviceProperties2);
  GFX_VK_IPROC(vkGetPhysicalDeviceQueueFamilyProperties);
  GFX_VK_IPROC(vkGetPhysicalDeviceQueueFamilyProperties2);
  GFX_VK_IPROC(vkGetPhysicalDeviceSparseImageFormatProperties);
  GFX_VK_IPROC(vkGetPhysicalDeviceSparseImageFormatProperties2);
  GFX_VK_IPROC(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
  GFX_VK_IPROC(vkGetPhysicalDeviceSurfaceFormatsKHR);
  GFX_VK_IPROC(vkGetPhysicalDeviceSurfacePresentModesKHR);
  GFX_VK_IPROC(vkGetPhysicalDeviceSurfaceSupportKHR);
  GFX_VK_IPROC(vkGetPhysicalDeviceToolProperties);
  GFX_VK_IPROC(vkSubmitDebugUtilsMessageEXT);

  GFX_VK_DPROC(vkAcquireNextImage2KHR);
  GFX_VK_DPROC(vkAcquireNextImageKHR);
  GFX_VK_DPROC(vkAllocateCommandBuffers);
  GFX_VK_DPROC(vkAllocateDescriptorSets);
  GFX_VK_DPROC(vkAllocateMemory);
  GFX_VK_DPROC(vkBeginCommandBuffer);
  GFX_VK_DPROC(vkBindBufferMemory);
  GFX_VK_DPROC(vkBindBufferMemory2);
  GFX_VK_DPROC(vkBindImageMemory);
  GFX_VK_DPROC(vkBindImageMemory2);
  GFX_VK_DPROC(vkCmdBeginDebugUtilsLabelEXT);
  GFX_VK_DPROC(vkCmdBeginQuery);
  GFX_VK_DPROC(vkCmdBeginRendering);
  GFX_VK_DPROC(vkCmdBeginRenderPass);
  GFX_VK_DPROC(vkCmdBeginRenderPass2);
  GFX_VK_DPROC(vkCmdBindDescriptorSets);
  GFX_VK_DPROC(vkCmdBindIndexBuffer);
  GFX_VK_DPROC(vkCmdBindPipeline);
  GFX_VK_DPROC(vkCmdBindVertexBuffers);
  GFX_VK_DPROC(vkCmdBindVertexBuffers2);
  GFX_VK_DPROC(vkCmdBlitImage);
  GFX_VK_DPROC(vkCmdBlitImage2);
  GFX_VK_DPROC(vkCmdClearAttachments);
  GFX_VK_DPROC(vkCmdClearColorImage);
  GFX_VK_DPROC(vkCmdClearDepthStencilImage);
  GFX_VK_DPROC(vkCmdCopyBuffer);
  GFX_VK_DPROC(vkCmdCopyBuffer2);
  GFX_VK_DPROC(vkCmdCopyBufferToImage);
  GFX_VK_DPROC(vkCmdCopyBufferToImage2);
  GFX_VK_DPROC(vkCmdCopyImage);
  GFX_VK_DPROC(vkCmdCopyImage2);
  GFX_VK_DPROC(vkCmdCopyImageToBuffer);
  GFX_VK_DPROC(vkCmdCopyImageToBuffer2);
  GFX_VK_DPROC(vkCmdCopyQueryPoolResults);
  GFX_VK_DPROC(vkCmdDispatch);
  GFX_VK_DPROC(vkCmdDispatchBase);
  GFX_VK_DPROC(vkCmdDispatchIndirect);
  GFX_VK_DPROC(vkCmdDraw);
  GFX_VK_DPROC(vkCmdDrawIndexed);
  GFX_VK_DPROC(vkCmdDrawIndexedIndirect);
  GFX_VK_DPROC(vkCmdDrawIndexedIndirectCount);
  GFX_VK_DPROC(vkCmdDrawIndirect);
  GFX_VK_DPROC(vkCmdDrawIndirectCount);
  GFX_VK_DPROC(vkCmdDrawMeshTasksEXT);
  GFX_VK_DPROC(vkCmdDrawMeshTasksIndirectCountEXT);
  GFX_VK_DPROC(vkCmdDrawMeshTasksIndirectEXT);
  GFX_VK_DPROC(vkCmdEndDebugUtilsLabelEXT);
  GFX_VK_DPROC(vkCmdEndQuery);
  GFX_VK_DPROC(vkCmdEndRendering);
  GFX_VK_DPROC(vkCmdEndRenderPass);
  GFX_VK_DPROC(vkCmdEndRenderPass2);
  GFX_VK_DPROC(vkCmdExecuteCommands);
  GFX_VK_DPROC(vkCmdFillBuffer);
  GFX_VK_DPROC(vkCmdInsertDebugUtilsLabelEXT);
  GFX_VK_DPROC(vkCmdNextSubpass);
  GFX_VK_DPROC(vkCmdNextSubpass2);
  GFX_VK_DPROC(vkCmdPipelineBarrier);
  GFX_VK_DPROC(vkCmdPipelineBarrier2);
  GFX_VK_DPROC(vkCmdPushConstants);
  GFX_VK_DPROC(vkCmdResetEvent);
  GFX_VK_DPROC(vkCmdResetEvent2);
  GFX_VK_DPROC(vkCmdResetQueryPool);
  GFX_VK_DPROC(vkCmdResolveImage);
  GFX_VK_DPROC(vkCmdResolveImage2);
  GFX_VK_DPROC(vkCmdSetAlphaToCoverageEnableEXT);
  GFX_VK_DPROC(vkCmdSetAlphaToOneEnableEXT);
  GFX_VK_DPROC(vkCmdSetBlendConstants);
  GFX_VK_DPROC(vkCmdSetColorBlendAdvancedEXT);
  GFX_VK_DPROC(vkCmdSetColorBlendEnableEXT);
  GFX_VK_DPROC(vkCmdSetColorBlendEquationEXT);
  GFX_VK_DPROC(vkCmdSetColorWriteMaskEXT);
  GFX_VK_DPROC(vkCmdSetConservativeRasterizationModeEXT);
  GFX_VK_DPROC(vkCmdSetCoverageModulationModeNV);
  GFX_VK_DPROC(vkCmdSetCoverageModulationTableEnableNV);
  GFX_VK_DPROC(vkCmdSetCoverageModulationTableNV);
  GFX_VK_DPROC(vkCmdSetCoverageReductionModeNV);
  GFX_VK_DPROC(vkCmdSetCoverageToColorEnableNV);
  GFX_VK_DPROC(vkCmdSetCoverageToColorLocationNV);
  GFX_VK_DPROC(vkCmdSetCullMode);
  GFX_VK_DPROC(vkCmdSetDepthBias);
  GFX_VK_DPROC(vkCmdSetDepthBiasEnable);
  GFX_VK_DPROC(vkCmdSetDepthBounds);
  GFX_VK_DPROC(vkCmdSetDepthBoundsTestEnable);
  GFX_VK_DPROC(vkCmdSetDepthClampEnableEXT);
  GFX_VK_DPROC(vkCmdSetDepthClipEnableEXT);
  GFX_VK_DPROC(vkCmdSetDepthClipNegativeOneToOneEXT);
  GFX_VK_DPROC(vkCmdSetDepthCompareOp);
  GFX_VK_DPROC(vkCmdSetDepthTestEnable);
  GFX_VK_DPROC(vkCmdSetDepthWriteEnable);
  GFX_VK_DPROC(vkCmdSetDeviceMask);
  GFX_VK_DPROC(vkCmdSetEvent);
  GFX_VK_DPROC(vkCmdSetEvent2);
  GFX_VK_DPROC(vkCmdSetExtraPrimitiveOverestimationSizeEXT);
  GFX_VK_DPROC(vkCmdSetFrontFace);
  GFX_VK_DPROC(vkCmdSetLineRasterizationModeEXT);
  GFX_VK_DPROC(vkCmdSetLineStippleEnableEXT);
  GFX_VK_DPROC(vkCmdSetLineWidth);
  GFX_VK_DPROC(vkCmdSetLogicOpEXT);
  GFX_VK_DPROC(vkCmdSetLogicOpEnableEXT);
  GFX_VK_DPROC(vkCmdSetPatchControlPointsEXT);
  GFX_VK_DPROC(vkCmdSetPolygonModeEXT);
  GFX_VK_DPROC(vkCmdSetPrimitiveRestartEnable);
  GFX_VK_DPROC(vkCmdSetPrimitiveTopology);
  GFX_VK_DPROC(vkCmdSetRasterizationSamplesEXT);
  GFX_VK_DPROC(vkCmdSetRasterizationStreamEXT);
  GFX_VK_DPROC(vkCmdSetRasterizerDiscardEnable);
  GFX_VK_DPROC(vkCmdSetRepresentativeFragmentTestEnableNV);
  GFX_VK_DPROC(vkCmdSetSampleLocationsEnableEXT);
  GFX_VK_DPROC(vkCmdSetSampleMaskEXT);
  GFX_VK_DPROC(vkCmdSetShadingRateImageEnableNV);
  GFX_VK_DPROC(vkCmdSetScissor);
  GFX_VK_DPROC(vkCmdSetScissorWithCount);
  GFX_VK_DPROC(vkCmdSetStencilCompareMask);
  GFX_VK_DPROC(vkCmdSetStencilOp);
  GFX_VK_DPROC(vkCmdSetStencilReference);
  GFX_VK_DPROC(vkCmdSetStencilTestEnable);
  GFX_VK_DPROC(vkCmdSetStencilWriteMask);
  GFX_VK_DPROC(vkCmdSetTessellationDomainOriginEXT);
  GFX_VK_DPROC(vkCmdSetViewport);
  GFX_VK_DPROC(vkCmdSetViewportSwizzleNV);
  GFX_VK_DPROC(vkCmdSetViewportWithCount);
  GFX_VK_DPROC(vkCmdSetViewportWScalingEnableNV);
  GFX_VK_DPROC(vkCmdUpdateBuffer);
  GFX_VK_DPROC(vkCmdWaitEvents);
  GFX_VK_DPROC(vkCmdWaitEvents2);
  GFX_VK_DPROC(vkCmdWriteTimestamp);
  GFX_VK_DPROC(vkCmdWriteTimestamp2);
  GFX_VK_DPROC(vkCreateBuffer);
  GFX_VK_DPROC(vkCreateBufferView);
  GFX_VK_DPROC(vkCreateCommandPool);
  GFX_VK_DPROC(vkCreateComputePipelines);
  GFX_VK_DPROC(vkCreateDescriptorPool);
  GFX_VK_DPROC(vkCreateDescriptorSetLayout);
  GFX_VK_DPROC(vkCreateDescriptorUpdateTemplate);
  GFX_VK_DPROC(vkCreateEvent);
  GFX_VK_DPROC(vkCreateFence);
  GFX_VK_DPROC(vkCreateFramebuffer);
  GFX_VK_DPROC(vkCreateGraphicsPipelines);
  GFX_VK_DPROC(vkCreateImage);
  GFX_VK_DPROC(vkCreateImageView);
  GFX_VK_DPROC(vkCreatePipelineCache);
  GFX_VK_DPROC(vkCreatePipelineLayout);
  GFX_VK_DPROC(vkCreatePrivateDataSlot);
  GFX_VK_DPROC(vkCreateQueryPool);
  GFX_VK_DPROC(vkCreateRenderPass);
  GFX_VK_DPROC(vkCreateRenderPass2);
  GFX_VK_DPROC(vkCreateSampler);
  GFX_VK_DPROC(vkCreateSamplerYcbcrConversion);
  GFX_VK_DPROC(vkCreateSemaphore);
  GFX_VK_DPROC(vkCreateShaderModule);
  GFX_VK_DPROC(vkCreateSwapchainKHR);
  GFX_VK_DPROC(vkDestroyBuffer);
  GFX_VK_DPROC(vkDestroyBufferView);
  GFX_VK_DPROC(vkDestroyCommandPool);
  GFX_VK_DPROC(vkDestroyDescriptorPool);
  GFX_VK_DPROC(vkDestroyDescriptorSetLayout);
  GFX_VK_DPROC(vkDestroyDescriptorUpdateTemplate);
  GFX_VK_DPROC(vkDestroyDevice);
  GFX_VK_DPROC(vkDestroyEvent);
  GFX_VK_DPROC(vkDestroyFence);
  GFX_VK_DPROC(vkDestroyFramebuffer);
  GFX_VK_DPROC(vkDestroyImage);
  GFX_VK_DPROC(vkDestroyImageView);
  GFX_VK_DPROC(vkDestroyPipeline);
  GFX_VK_DPROC(vkDestroyPipelineCache);
  GFX_VK_DPROC(vkDestroyPipelineLayout);
  GFX_VK_DPROC(vkDestroyPrivateDataSlot);
  GFX_VK_DPROC(vkDestroyQueryPool);
  GFX_VK_DPROC(vkDestroyRenderPass);
  GFX_VK_DPROC(vkDestroySampler);
  GFX_VK_DPROC(vkDestroySamplerYcbcrConversion);
  GFX_VK_DPROC(vkDestroySemaphore);
  GFX_VK_DPROC(vkDestroyShaderModule);
  GFX_VK_DPROC(vkDestroySwapchainKHR);
  GFX_VK_DPROC(vkDeviceWaitIdle);
  GFX_VK_DPROC(vkEndCommandBuffer);
  GFX_VK_DPROC(vkFlushMappedMemoryRanges);
  GFX_VK_DPROC(vkFreeCommandBuffers);
  GFX_VK_DPROC(vkFreeDescriptorSets);
  GFX_VK_DPROC(vkFreeMemory);
  GFX_VK_DPROC(vkGetBufferDeviceAddress);
  GFX_VK_DPROC(vkGetBufferMemoryRequirements);
  GFX_VK_DPROC(vkGetBufferMemoryRequirements2);
  GFX_VK_DPROC(vkGetBufferOpaqueCaptureAddress);
  GFX_VK_DPROC(vkGetDescriptorSetLayoutSupport);
  GFX_VK_DPROC(vkGetDeviceGroupPeerMemoryFeatures);
  GFX_VK_DPROC(vkGetDeviceGroupPresentCapabilitiesKHR);
  GFX_VK_DPROC(vkGetDeviceGroupSurfacePresentModesKHR);
  GFX_VK_DPROC(vkGetDeviceBufferMemoryRequirements);
  GFX_VK_DPROC(vkGetDeviceImageMemoryRequirements);
  GFX_VK_DPROC(vkGetDeviceImageSparseMemoryRequirements);
  GFX_VK_DPROC(vkGetDeviceMemoryCommitment);
  GFX_VK_DPROC(vkGetDeviceMemoryOpaqueCaptureAddress);
  GFX_VK_DPROC(vkGetDeviceQueue);
  GFX_VK_DPROC(vkGetDeviceQueue2);
  GFX_VK_DPROC(vkGetEventStatus);
  GFX_VK_DPROC(vkGetFenceStatus);
  GFX_VK_DPROC(vkGetImageMemoryRequirements);
  GFX_VK_DPROC(vkGetImageMemoryRequirements2);
  GFX_VK_DPROC(vkGetImageSparseMemoryRequirements);
  GFX_VK_DPROC(vkGetImageSparseMemoryRequirements2);
  GFX_VK_DPROC(vkGetImageSubresourceLayout);
  GFX_VK_DPROC(vkGetPipelineCacheData);
  GFX_VK_DPROC(vkGetPrivateData);
  GFX_VK_DPROC(vkGetQueryPoolResults);
  GFX_VK_DPROC(vkGetRenderAreaGranularity);
  GFX_VK_DPROC(vkGetSemaphoreCounterValue);
  GFX_VK_DPROC(vkGetSwapchainImagesKHR);
  GFX_VK_DPROC(vkInvalidateMappedMemoryRanges);
  GFX_VK_DPROC(vkMapMemory);
  GFX_VK_DPROC(vkMergePipelineCaches);
  GFX_VK_DPROC(vkQueueBeginDebugUtilsLabelEXT);
  GFX_VK_DPROC(vkQueueBindSparse);
  GFX_VK_DPROC(vkQueueEndDebugUtilsLabelEXT);
  GFX_VK_DPROC(vkQueueInsertDebugUtilsLabelEXT);
  GFX_VK_DPROC(vkQueuePresentKHR);
  GFX_VK_DPROC(vkQueueSubmit);
  GFX_VK_DPROC(vkQueueSubmit2);
  GFX_VK_DPROC(vkQueueWaitIdle);
  GFX_VK_DPROC(vkResetCommandBuffer);
  GFX_VK_DPROC(vkResetCommandPool);
  GFX_VK_DPROC(vkResetDescriptorPool);
  GFX_VK_DPROC(vkResetEvent);
  GFX_VK_DPROC(vkResetFences);
  GFX_VK_DPROC(vkResetQueryPool);
  GFX_VK_DPROC(vkSetDebugUtilsObjectNameEXT);
  GFX_VK_DPROC(vkSetDebugUtilsObjectTagEXT);
  GFX_VK_DPROC(vkSetEvent);
  GFX_VK_DPROC(vkSetPrivateData);
  GFX_VK_DPROC(vkSignalSemaphore);
  GFX_VK_DPROC(vkTrimCommandPool);
  GFX_VK_DPROC(vkUnmapMemory);
  GFX_VK_DPROC(vkUpdateDescriptorSets);
  GFX_VK_DPROC(vkUpdateDescriptorSetWithTemplate);
  GFX_VK_DPROC(vkWaitForFences);
  GFX_VK_DPROC(vkWaitSemaphores);

private:

  PFN_vkVoidFunction getLoaderProc(const char* name) const;
  PFN_vkVoidFunction getInstanceProc(const char* name) const;
  PFN_vkVoidFunction getDeviceProc(const char* name) const;

};


/**
 * \brief Helper functions to get Vulkan object types
 *
 * \param [in] object Object to get the type of
 * \returns Object type
 */
inline VkObjectType getVulkanObjectType(VkDevice)         { return VK_OBJECT_TYPE_DEVICE;           }
inline VkObjectType getVulkanObjectType(VkQueue)          { return VK_OBJECT_TYPE_QUEUE;            }
inline VkObjectType getVulkanObjectType(VkSemaphore)      { return VK_OBJECT_TYPE_SEMAPHORE;        }
inline VkObjectType getVulkanObjectType(VkBuffer)         { return VK_OBJECT_TYPE_BUFFER;           }
inline VkObjectType getVulkanObjectType(VkBufferView)     { return VK_OBJECT_TYPE_BUFFER_VIEW;      }
inline VkObjectType getVulkanObjectType(VkImage)          { return VK_OBJECT_TYPE_IMAGE;            }
inline VkObjectType getVulkanObjectType(VkImageView)      { return VK_OBJECT_TYPE_IMAGE_VIEW;       }
inline VkObjectType getVulkanObjectType(VkCommandPool)    { return VK_OBJECT_TYPE_COMMAND_POOL;     }
inline VkObjectType getVulkanObjectType(VkCommandBuffer)  { return VK_OBJECT_TYPE_COMMAND_BUFFER;   }
inline VkObjectType getVulkanObjectType(VkPipeline)       { return VK_OBJECT_TYPE_PIPELINE;         }
inline VkObjectType getVulkanObjectType(VkDescriptorPool) { return VK_OBJECT_TYPE_DESCRIPTOR_POOL;  }
inline VkObjectType getVulkanObjectType(VkDescriptorSet)  { return VK_OBJECT_TYPE_DESCRIPTOR_SET;   }
inline VkObjectType getVulkanObjectType(VkSampler)        { return VK_OBJECT_TYPE_SAMPLER;          }

}
