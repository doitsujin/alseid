#include <iomanip>

#include "../../util/util_log.h"

#include "gfx_vulkan_buffer.h"
#include "gfx_vulkan_command_list.h"
#include "gfx_vulkan_context.h"
#include "gfx_vulkan_descriptor_array.h"
#include "gfx_vulkan_device.h"
#include "gfx_vulkan_image.h"
#include "gfx_vulkan_presenter.h"
#include "gfx_vulkan_sampler.h"
#include "gfx_vulkan_semaphore.h"
#include "gfx_vulkan_utils.h"

namespace as {

GfxVulkanDevice::GfxVulkanDevice(
        std::shared_ptr<GfxVulkan>    gfx,
        VkPhysicalDevice              adapter)
: m_gfx                   (std::move(gfx))
, m_instanceFlags         (m_gfx->getInstanceFlags())
, m_vk                    (m_gfx->vk(), adapter, VK_NULL_HANDLE)
, m_extensions            (m_gfx->vk(), adapter)
, m_properties            (m_gfx->vk(), adapter, m_extensions)
, m_features              (m_gfx->vk(), adapter, m_extensions)
, m_memoryTypeMasks       (getMemoryTypeMasks())
, m_pipelineManager       (std::make_unique<GfxVulkanPipelineManager>(*this))
, m_descriptorPoolManager (std::make_unique<GfxVulkanDescriptorPoolManager>(*this))
, m_memoryAllocator       (std::make_unique<GfxVulkanMemoryAllocator>(*this))
, m_scratchBufferPool     (std::make_unique<GfxScratchBufferPool>(*this)) {
  Log::info("Vulkan: Initializing device: ", m_properties.core.properties.deviceName);
  Log::info("Vulkan: Using driver: ", m_properties.vk12.driverName, " (", m_properties.vk12.driverInfo, ")");

  // Create Vulkan device and update the loader with device-level functions
  GfxVulkanQueueMapping queueMapping(m_vk, m_gfx->getWsiBridge());

  VkDeviceCreateInfo deviceInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, m_features.core.pNext };
  queueMapping.getQueueCreateInfos(&deviceInfo.queueCreateInfoCount, &deviceInfo.pQueueCreateInfos);
  m_extensions.getExtensionNames(&deviceInfo.enabledExtensionCount, &deviceInfo.ppEnabledExtensionNames);
  deviceInfo.pEnabledFeatures = &m_features.core.features;

  VkDevice device = VK_NULL_HANDLE;
  VkResult vr = m_vk.vkCreateDevice(adapter, &deviceInfo, nullptr, &device);

  if (vr)
    throw VulkanError("Failed to create Vulkan device.", vr);

  m_vk = GfxVulkanProcs(m_vk, adapter, device);

  // Set up the actual Vulkan queue objects
  for (uint32_t i = 0; i < uint32_t(GfxQueue::eQueueCount); i++) {
    auto queue = GfxQueue(i);
    auto metadata = queueMapping.getQueueMetadata(queue);

    if (metadata) {
      m_vk.vkGetDeviceQueue(m_vk.device,
        metadata->queueFamily,
        metadata->queueIndexInFamily,
        &m_queues[i].queue);

      m_queues[i].queueFamily = metadata->queueFamily;

      if (queue != GfxQueue::eSparseBinding && queue != GfxQueue::ePresent) {
        bool foundQueueFamily = false;

        for (uint32_t j = 0; j < m_queueFamilyCount; j++)
          foundQueueFamily |= m_queueFamilies[j] == metadata->queueFamily;

        if (!foundQueueFamily)
          m_queueFamilies[m_queueFamilyCount++] = metadata->queueFamily;
      }
    } else {
      m_queues[i].queue       = VK_NULL_HANDLE;
      m_queues[i].queueFamily = VK_QUEUE_FAMILY_IGNORED;
    }
  }
}


GfxVulkanDevice::~GfxVulkanDevice() {
  m_memoryAllocator.reset();
  m_descriptorPoolManager.reset();
  m_pipelineManager.reset();
  m_scratchBufferPool.reset();

  m_vk.vkDestroyDevice(m_vk.device, nullptr);
}


void GfxVulkanDevice::getQueueSharingInfo(
        VkSharingMode*                sharingMode,
        uint32_t*                     queueFamilyCount,
  const uint32_t**                    queueFamilies) const {
  if (m_queueFamilyCount > 1) {
    *sharingMode = VK_SHARING_MODE_CONCURRENT;
    *queueFamilyCount = m_queueFamilyCount;
    *queueFamilies = m_queueFamilies.data();
  } else {
    *sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    *queueFamilyCount = 0;
    *queueFamilies = nullptr;
  }
}


GfxBuffer GfxVulkanDevice::createBuffer(
  const GfxBufferDesc&                desc,
        GfxMemoryTypes                memoryTypes) {
  VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
  bufferInfo.size = desc.size;
  bufferInfo.usage = getVkBufferUsage(desc.usage);

  getQueueSharingInfo(
    &bufferInfo.sharingMode,
    &bufferInfo.queueFamilyIndexCount,
    &bufferInfo.pQueueFamilyIndices);

  if (desc.flags & GfxBufferFlag::eSparseResidency) {
    bufferInfo.flags = VK_BUFFER_CREATE_SPARSE_BINDING_BIT
                     | VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT
                     | VK_BUFFER_CREATE_SPARSE_ALIASED_BIT;
  }

  // Try to allocate memory
  GfxVulkanMemoryAllocationInfo allocationInfo = { };
  GfxVulkanMemoryRequirements requirements = { };
  GfxVulkanMemorySlice memorySlice;

  if (!(desc.flags & GfxBufferFlag::eSparseResidency)) {
    requirements.dedicated = { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS };
    requirements.core = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, &requirements.dedicated };

    VkDeviceBufferMemoryRequirements info = { VK_STRUCTURE_TYPE_DEVICE_BUFFER_MEMORY_REQUIREMENTS };
    info.pCreateInfo = &bufferInfo;

    m_vk.vkGetDeviceBufferMemoryRequirements(m_vk.device, &info, &requirements.core);

    if (desc.flags & GfxBufferFlag::eDedicatedAllocation)
      requirements.dedicated.prefersDedicatedAllocation = VK_TRUE;

    // If possible, allocate memory first so that we can exit
    // early on failure, without creating a resource object.
    allocationInfo.tiling = VK_IMAGE_TILING_LINEAR;
    allocationInfo.memoryTypes = memoryTypes;
    allocationInfo.cpuAccess = desc.usage & (GfxUsage::eCpuWrite | GfxUsage::eCpuRead);

    if (!requirements.dedicated.prefersDedicatedAllocation) {
      if (!(memorySlice = m_memoryAllocator->allocateMemory(requirements, allocationInfo)))
        return GfxBuffer();
    }
  }

  VkBuffer buffer = VK_NULL_HANDLE;
  VkResult vr = m_vk.vkCreateBuffer(m_vk.device, &bufferInfo, nullptr, &buffer);

  if (vr)
    throw VulkanError("Vulkan: Failed to create buffer", vr);

  if (requirements.dedicated.prefersDedicatedAllocation) {
    allocationInfo.dedicated = { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO };
    allocationInfo.dedicated.buffer = buffer;

    if (!(memorySlice = m_memoryAllocator->allocateMemory(requirements, allocationInfo))) {
      m_vk.vkDestroyBuffer(m_vk.device, buffer, nullptr);
      return GfxBuffer();
    }
  }

  if (!(desc.flags & GfxBufferFlag::eSparseResidency)) {
    VkBindBufferMemoryInfo bind = { VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO };
    bind.buffer = buffer;
    bind.memory = memorySlice.getHandle();
    bind.memoryOffset = memorySlice.getOffset();

    vr = m_vk.vkBindBufferMemory2(m_vk.device, 1, &bind);

    if (vr) {
      m_vk.vkDestroyBuffer(m_vk.device, buffer, nullptr);
      throw VulkanError("Vulkan: Failed bind buffer memory", vr);
    }
  }

  return GfxBuffer(std::make_shared<GfxVulkanBuffer>(
    shared_from_this(), desc, buffer, std::move(memorySlice)));
}


GfxColorBlendState GfxVulkanDevice::createColorBlendState(
  const GfxColorBlendStateDesc&       desc) {
  return GfxColorBlendState(m_pipelineManager->createColorBlendState(desc));
}


GfxComputePipeline GfxVulkanDevice::createComputePipeline(
  const GfxComputePipelineDesc&       desc) {
  return GfxComputePipeline(m_pipelineManager->createComputePipeline(desc));
}


GfxContext GfxVulkanDevice::createContext(
        GfxQueue                      queue) {
  return GfxContext(std::make_shared<GfxVulkanContext>(
    shared_from_this(), queue));
}


GfxDepthStencilState GfxVulkanDevice::createDepthStencilState(
  const GfxDepthStencilStateDesc&     desc) {
  return GfxDepthStencilState(m_pipelineManager->createDepthStencilState(desc));
}


GfxDescriptorArray GfxVulkanDevice::createDescriptorArray(
  const GfxDescriptorArrayDesc&       desc) {
  return GfxDescriptorArray(std::make_shared<GfxVulkanDescriptorArray>(
    shared_from_this(), desc));
}


GfxGraphicsPipeline GfxVulkanDevice::createGraphicsPipeline(
  const GfxGraphicsPipelineDesc&      desc) {
  return GfxGraphicsPipeline(m_pipelineManager->createGraphicsPipeline(desc));
}


GfxGraphicsPipeline GfxVulkanDevice::createGraphicsPipeline(
  const GfxMeshPipelineDesc&          desc) {
  return GfxGraphicsPipeline(m_pipelineManager->createGraphicsPipeline(desc));
}


GfxImage GfxVulkanDevice::createImage(
  const GfxImageDesc&                 desc,
        GfxMemoryTypes                memoryTypes) {
  // Gather unique image view formats
  std::array<VkFormat, GfxMaxViewFormats + 1> viewFormats = { };

  VkImageFormatListCreateInfo imageFormatList = { VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO };
  imageFormatList.viewFormatCount = 0;
  imageFormatList.pViewFormats = viewFormats.data();

  viewFormats[imageFormatList.viewFormatCount++] = getVkFormat(desc.format);

  for (uint32_t i = 0; i < desc.viewFormatCount; i++) {
    VkFormat format = getVkFormat(desc.viewFormats[i]);
    bool found = false;

    for (uint32_t j = 0; j < imageFormatList.viewFormatCount && !found; j++)
      found = viewFormats[j] == format;

    if (!found)
      viewFormats[imageFormatList.viewFormatCount++] = format;
  }

  // Create actual image object
  VkImageCreateInfo imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
  imageInfo.imageType = getVkImageType(desc.type);
  imageInfo.format = getVkFormat(desc.format);
  imageInfo.extent = getVkExtent3D(desc.extent);
  imageInfo.mipLevels = desc.mips;
  imageInfo.arrayLayers = desc.layers;
  imageInfo.samples = VkSampleCountFlagBits(desc.samples);
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.usage = getVkImageUsage(desc.format, desc.usage);
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  if (desc.flags & GfxImageFlag::eSparseResidency) {
    imageInfo.flags |= VK_IMAGE_CREATE_SPARSE_BINDING_BIT
                    |  VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT
                    |  VK_IMAGE_CREATE_SPARSE_ALIASED_BIT;
  }

  if (desc.flags & GfxImageFlag::eCubeViews)
    imageInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

  if (desc.type == GfxImageType::e3D && (desc.usage & GfxUsage::eRenderTarget))
    imageInfo.flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;

  if (imageFormatList.viewFormatCount > 1)
    imageInfo.pNext = &imageFormatList;

  if (desc.flags & GfxImageFlag::eSimultaneousAccess) {
    getQueueSharingInfo(
      &imageInfo.sharingMode,
      &imageInfo.queueFamilyIndexCount,
      &imageInfo.pQueueFamilyIndices);
  }

  // Try to allocate memory
  GfxVulkanMemoryAllocationInfo allocationInfo = { };
  GfxVulkanMemoryRequirements requirements = { };
  GfxVulkanMemorySlice memorySlice;

  if (!(desc.flags & GfxImageFlag::eSparseResidency)) {
    requirements.dedicated = { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS };
    requirements.core = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, &requirements.dedicated };

    VkDeviceImageMemoryRequirements info = { VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS };
    info.pCreateInfo = &imageInfo;

    m_vk.vkGetDeviceImageMemoryRequirements(m_vk.device, &info, &requirements.core);

    if (desc.flags & GfxImageFlag::eDedicatedAllocation)
      requirements.dedicated.prefersDedicatedAllocation = VK_TRUE;

    // If possible, allocate memory first so that we can exit
    // early on failure, without creating a resource object.
    allocationInfo.tiling = imageInfo.tiling;
    allocationInfo.memoryTypes = memoryTypes;

    if (!requirements.dedicated.prefersDedicatedAllocation) {
      if (!(memorySlice = m_memoryAllocator->allocateMemory(requirements, allocationInfo)))
        return GfxImage();
    }
  }

  VkImage image = VK_NULL_HANDLE;
  VkResult vr = m_vk.vkCreateImage(m_vk.device, &imageInfo, nullptr, &image);

  if (vr)
    throw VulkanError("Vulkan: Failed to create image", vr);

  if (requirements.dedicated.prefersDedicatedAllocation) {
    allocationInfo.dedicated = { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO };
    allocationInfo.dedicated.image = image;

    if (!(memorySlice = m_memoryAllocator->allocateMemory(requirements, allocationInfo))) {
      m_vk.vkDestroyImage(m_vk.device, image, nullptr);
      return GfxImage();
    }
  }

  if (!(desc.flags & GfxImageFlag::eSparseResidency)) {
    VkBindImageMemoryInfo bind = { VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO };
    bind.image = image;
    bind.memory = memorySlice.getHandle();
    bind.memoryOffset = memorySlice.getOffset();

    vr = m_vk.vkBindImageMemory2(m_vk.device, 1, &bind);

    if (vr) {
      m_vk.vkDestroyImage(m_vk.device, image, nullptr);
      throw VulkanError("Vulkan: Failed bind image memory", vr);
    }
  }

  return GfxImage(std::make_shared<GfxVulkanImage>(
    shared_from_this(), desc, image, std::move(memorySlice)));
}


GfxMultisampleState GfxVulkanDevice::createMultisampleState(
  const GfxMultisampleStateDesc&      desc) {
  return GfxMultisampleState(m_pipelineManager->createMultisampleState(desc));
}


GfxPresenter GfxVulkanDevice::createPresenter(
  const GfxPresenterDesc&             desc) {
  return GfxPresenter(std::make_shared<GfxVulkanPresenter>(
    shared_from_this(), m_gfx->getWsiBridge(), desc));
}


GfxRasterizerState GfxVulkanDevice::createRasterizerState(
  const GfxRasterizerStateDesc&       desc) {
  return GfxRasterizerState(m_pipelineManager->createRasterizerState(desc));
}


GfxRenderTargetState GfxVulkanDevice::createRenderTargetState(
  const GfxRenderTargetStateDesc&     desc) {
  return GfxRenderTargetState(m_pipelineManager->createRenderTargetState(desc));
}


GfxSampler GfxVulkanDevice::createSampler(
  const GfxSamplerDesc&               desc) {
  return GfxSampler(std::make_shared<GfxVulkanSampler>(
    shared_from_this(), desc));
}


GfxSemaphore GfxVulkanDevice::createSemaphore(
  const GfxSemaphoreDesc&             desc) {
  return GfxSemaphore(std::make_shared<GfxVulkanSemaphore>(
    shared_from_this(), desc, VK_SEMAPHORE_TYPE_TIMELINE));
}


GfxVertexInputState GfxVulkanDevice::createVertexInputState(
  const GfxVertexInputStateDesc&      desc) {
  return GfxVertexInputState(m_pipelineManager->createVertexInputState(desc));
}


void GfxVulkanDevice::submit(
        GfxQueue                      queue,
        GfxCommandSubmission&&        submission) {
  if (submission.isEmpty())
    return;

  // Consume submission so caller doesn't reuse it
  GfxCommandSubmission submissionCopy = std::move(submission);
  GfxCommandSubmissionInternal submissionInfo = submissionCopy.getInternalInfo();

  // Convert submission info arrays to something we can pass to Vulkan
  small_vector<VkCommandBufferSubmitInfo, 64> commandBuffers(submissionInfo.commandListCount);
  small_vector<VkSemaphoreSubmitInfo, 64> waitSemaphores(submissionInfo.waitSemaphoreCount);
  small_vector<VkSemaphoreSubmitInfo, 64> signalSemaphores(submissionInfo.signalSemaphoreCount);

  for (uint32_t i = 0; i < submissionInfo.commandListCount; i++) {
    auto& commandList = static_cast<const GfxVulkanCommandList&>(*submissionInfo.commandLists[i]);

    VkCommandBufferSubmitInfo& info = commandBuffers[i];
    info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
    info.commandBuffer = commandList.getHandle();
  }

  for (uint32_t i = 0; i < submissionInfo.waitSemaphoreCount; i++) {
    auto& semaphore = static_cast<const GfxVulkanSemaphore&>(*submissionInfo.waitSemaphores[i].semaphore);

    VkSemaphoreSubmitInfo& info = waitSemaphores[i];
    info = { VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
    info.semaphore = semaphore.getHandle();
    info.value = submissionInfo.waitSemaphores[i].value;
    info.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  }

  for (uint32_t i = 0; i < submissionInfo.signalSemaphoreCount; i++) {
    auto& semaphore = static_cast<const GfxVulkanSemaphore&>(*submissionInfo.signalSemaphores[i].semaphore);

    VkSemaphoreSubmitInfo& info = signalSemaphores[i];
    info = { VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
    info.semaphore = semaphore.getHandle();
    info.value = submissionInfo.signalSemaphores[i].value;
    info.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  }

  // Perform the submission. We do not have individual queue locks, but this
  // should not be an issue as concurrent submissions should be very rare.
  VkSubmitInfo2 submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };

  if (!waitSemaphores.empty()) {
    submitInfo.waitSemaphoreInfoCount = waitSemaphores.size();
    submitInfo.pWaitSemaphoreInfos = waitSemaphores.data();
  }

  if (!commandBuffers.empty()) {
    submitInfo.commandBufferInfoCount = commandBuffers.size();
    submitInfo.pCommandBufferInfos = commandBuffers.data();
  }

  if (!signalSemaphores.empty()) {
    submitInfo.signalSemaphoreInfoCount = signalSemaphores.size();
    submitInfo.pSignalSemaphoreInfos = signalSemaphores.data();
  }

  std::lock_guard lock(m_submissionMutex);

  VkResult vr = m_vk.vkQueueSubmit2(
    m_queues[uint32_t(queue)].queue,
    1, &submitInfo, VK_NULL_HANDLE);

  if (vr)
    throw VulkanError("Vulkan: Queue submission failed", vr);
}


void GfxVulkanDevice::waitIdle() {
  std::lock_guard lock(m_submissionMutex);

  VkResult vr = m_vk.vkDeviceWaitIdle(m_vk.device);

  if (vr)
    throw VulkanError("Vulkan: Waiting for device failed", vr);
}


VkResult GfxVulkanDevice::present(
        GfxQueue                      queue,
        VkSemaphore                   semaphore,
        VkSwapchainKHR                swapchain,
        uint32_t                      imageId) {
  VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = &semaphore;
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = &swapchain;
  presentInfo.pImageIndices = &imageId;

  std::lock_guard lock(m_submissionMutex);

  return m_vk.vkQueuePresentKHR(m_queues[uint32_t(queue)].queue, &presentInfo);
}


void GfxVulkanDevice::waitQueueIdle(
        GfxQueue                      queue) {
  std::lock_guard lock(m_submissionMutex);

  VkResult vr = m_vk.vkQueueWaitIdle(m_queues[uint32_t(queue)].queue);

  if (vr)
    throw VulkanError("Vulkan: Waiting for queue failed", vr);
}


GfxVulkanMemoryTypeMasks GfxVulkanDevice::getMemoryTypeMasks() const {
  const auto& memoryProperties = m_properties.memory.memoryProperties;

  GfxVulkanMemoryTypeMasks result = { };
  VkDeviceSize largestHeapSize = 0;

  for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
    if (memoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
      VkDeviceSize heapSize = memoryProperties.memoryHeaps[memoryProperties.memoryTypes[i].heapIndex].size;

      if (heapSize > largestHeapSize) {
        largestHeapSize = heapSize;
        result.vidMem = 0;
      }

      if (memoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        result.barMem |= 1u << i;
      else if (heapSize == largestHeapSize)
        result.vidMem |= 1u << i;
    } else {
      result.sysMem |= 1u << i;
    }
  }

  // UMA systems may report all memory types as device local
  if (!result.sysMem)
    result.sysMem = result.vidMem | result.barMem;

  if (!result.vidMem)
    result.vidMem = result.barMem;

  return result;
}

}
