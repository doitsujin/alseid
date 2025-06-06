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
, m_memoryTypeMasks       (queryMemoryTypeMasks())
, m_enabledShaderStages   (queryEnabledShaderStages())
, m_shadingRateTileSize   (determineShadingRateTileSize())
, m_shadingRates          (determineShadingRates())
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

  // Initialize objects that depend on the device being initialized
  m_gdeflatePipeline = std::make_unique<GfxVulkanGDeflatePipeline>(*this);
}


GfxVulkanDevice::~GfxVulkanDevice() {
  m_gdeflatePipeline.reset();
  m_scratchBufferPool.reset();
  m_memoryAllocator.reset();
  m_descriptorPoolManager.reset();
  m_pipelineManager.reset();

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


GfxShaderFormatInfo GfxVulkanDevice::getShaderInfo() const {
  GfxShaderFormatInfo result;
  result.format = GfxShaderFormat::eVulkanSpirvCompressed;
  result.identifier = FourCC('S', 'P', 'I', 'R');
  return result;
}


GfxDeviceFeatures GfxVulkanDevice::getFeatures() const {
  GfxDeviceFeatures result = { };
  result.conservativeRasterization = m_extensions.extConservativeRasterization;
  result.depthBounds = m_features.core.features.depthBounds;
  result.dualSourceBlending = m_features.core.features.dualSrcBlend;
  result.fastLinkGraphicsPipelines = m_features.extGraphicsPipelineLibrary.graphicsPipelineLibrary;
  result.fragmentShaderStencilExport = m_extensions.extShaderStencilExport;
  result.fragmentShadingRate = !m_shadingRates.empty();
  result.gdeflateDecompression = m_gdeflatePipeline->getPipeline() != VK_NULL_HANDLE;

  result.rayTracing =
    m_features.khrRayQuery.rayQuery &&
    m_features.khrAccelerationStructure.accelerationStructure;

  result.shader16Bit =
    m_features.core.features.shaderInt16 &&
    m_features.vk12.shaderFloat16;

  result.shader64Bit =
    m_features.core.features.shaderInt64 &&
    m_features.core.features.shaderFloat64;

  result.shaderStorage16Bit = m_features.vk11.storageBuffer16BitAccess;

  result.vertexShaderStorage = m_features.core.features.vertexPipelineStoresAndAtomics;
  result.vertexShaderViewportLayerExport =
    m_features.vk12.shaderOutputViewportIndex &&
    m_features.vk12.shaderOutputLayer;

  result.shaderStages = m_enabledShaderStages;

  // We could expose more here depending on device properties,
  // but just be conservative. These are guaranteed to work on
  // all supported Vulkan devices.
  result.maxSamplerDescriptors = 1000;
  result.maxResourceDescriptors = 250000;

  // Fill in shading rate properties if the feature is supported
  if (result.fragmentShadingRate) {
    result.shadingRateTileSize = m_shadingRateTileSize;
    result.shadingRateTileSizeLog2 = Extent2D(
      findmsb(m_shadingRateTileSize.at<0>()),
      findmsb(m_shadingRateTileSize.at<1>()));
  }

  return result;
}


GfxFormatFeatures GfxVulkanDevice::getFormatFeatures(
        GfxFormat                     format) const {
  VkFormatProperties3 features3 = { VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3 };
  VkFormatProperties2 features2 = { VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2, &features3 };

  m_vk.vkGetPhysicalDeviceFormatProperties2(m_vk.adapter, getVkFormat(format), &features2);

  bool storageRead = true;
  bool storageAtomic = true;

  GfxFormatFeatures result = 0;

  if (format == GfxFormat::eR16ui || format == GfxFormat::eR32ui)
    result |= GfxFormatFeature::eIndexBuffer;

  if (features3.bufferFeatures & VK_FORMAT_FEATURE_2_VERTEX_BUFFER_BIT)
    result |= GfxFormatFeature::eVertexBuffer;

  if (features3.bufferFeatures & VK_FORMAT_FEATURE_2_UNIFORM_TEXEL_BUFFER_BIT)
    result |= GfxFormatFeature::eResourceBuffer;

  if ((features3.bufferFeatures & VK_FORMAT_FEATURE_2_STORAGE_TEXEL_BUFFER_BIT)
   && (features3.bufferFeatures & VK_FORMAT_FEATURE_2_STORAGE_WRITE_WITHOUT_FORMAT_BIT)) {
    result |= GfxFormatFeature::eStorageBuffer;

    storageRead &= (features3.bufferFeatures & VK_FORMAT_FEATURE_2_STORAGE_READ_WITHOUT_FORMAT_BIT);
    storageAtomic &= (features3.bufferFeatures & VK_FORMAT_FEATURE_2_STORAGE_TEXEL_BUFFER_ATOMIC_BIT);
  }

  if (features3.bufferFeatures & VK_FORMAT_FEATURE_2_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR)
    result |= GfxFormatFeature::eBvhGeometry;

  if (features3.optimalTilingFeatures & VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT)
    result |= GfxFormatFeature::eResourceImage;

  if ((features3.optimalTilingFeatures & VK_FORMAT_FEATURE_2_STORAGE_IMAGE_BIT)
   && (features3.optimalTilingFeatures & VK_FORMAT_FEATURE_2_STORAGE_WRITE_WITHOUT_FORMAT_BIT)) {
    result |= GfxFormatFeature::eStorageImage;

    storageRead &= (features3.optimalTilingFeatures & VK_FORMAT_FEATURE_2_STORAGE_READ_WITHOUT_FORMAT_BIT);
    storageAtomic &= (features3.optimalTilingFeatures & VK_FORMAT_FEATURE_2_STORAGE_IMAGE_ATOMIC_BIT);
  }

  if (features3.optimalTilingFeatures & (VK_FORMAT_FEATURE_2_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_2_DEPTH_STENCIL_ATTACHMENT_BIT))
    result |= GfxFormatFeature::eRenderTarget;

  if (features3.optimalTilingFeatures & VK_FORMAT_FEATURE_2_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR)
    result |= GfxFormatFeature::eShadingRate;

  if (features3.optimalTilingFeatures & VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_FILTER_LINEAR_BIT)
    result |= GfxFormatFeature::eSampleLinear;

  if (result & (GfxFormatFeature::eStorageBuffer | GfxFormatFeature::eStorageImage)) {
    if (storageRead)
      result |= GfxFormatFeature::eShaderStorageRead;

    if (storageAtomic)
      result |= GfxFormatFeature::eShaderStorageAtomic;
  }

  return result;
}


bool GfxVulkanDevice::supportsShadingRate(
        Extent2D                      shadingRate,
        uint32_t                      samples) const {
  if (shadingRate == Extent2D(1u, 1u))
    return true;

  for (const auto& rate : m_shadingRates) {
    if ((rate.fragmentSize.width == shadingRate.at<0>())
     && (rate.fragmentSize.height == shadingRate.at<1>())
     && (rate.sampleCounts & VkSampleCountFlagBits(samples)))
      return true;
  }

  return false;
}


uint64_t GfxVulkanDevice::computeRayTracingBvhSize(
  const GfxRayTracingGeometryDesc&    desc) const {
  GfxVulkanRayTracingBvhInfo info(*this, desc);
  return computeRayTracingBvhSize(info).allocationSize;
}


uint64_t GfxVulkanDevice::computeRayTracingBvhSize(
  const GfxRayTracingInstanceDesc&    desc) const {
  GfxVulkanRayTracingBvhInfo info(*this, desc);
  return computeRayTracingBvhSize(info).allocationSize;
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

  // Get device address if necessary
  VkDeviceAddress va = 0;

  if (bufferInfo.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
    VkBufferDeviceAddressInfo info = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    info.buffer = buffer;

    va = m_vk.vkGetBufferDeviceAddress(m_vk.device, &info);
  }

  return GfxBuffer(std::make_shared<GfxVulkanBuffer>(
    *this, desc, buffer, va, std::move(memorySlice)));
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


GfxDescriptorArray GfxVulkanDevice::createDescriptorArray(
  const GfxDescriptorArrayDesc&       desc) {
  return GfxDescriptorArray(std::make_shared<GfxVulkanDescriptorArray>(*this, desc));
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

  if (imageFormatList.viewFormatCount > 1) {
    imageInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
    imageInfo.pNext = &imageFormatList;
  }

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
    *this, desc, image, std::move(memorySlice)));
}


GfxPresenter GfxVulkanDevice::createPresenter(
  const GfxPresenterDesc&             desc) {
  return GfxPresenter(std::make_shared<GfxVulkanPresenter>(
    shared_from_this(), m_gfx->getWsiBridge(), desc));
}


GfxRayTracingBvh GfxVulkanDevice::createRayTracingBvh(
  const GfxRayTracingGeometryDesc&    desc) {
  GfxVulkanRayTracingBvhInfo info(*this, desc);
  GfxVulkanRayTracingBvhSize size = computeRayTracingBvhSize(info);

  GfxRayTracingBvhDesc subDesc;
  subDesc.debugName = desc.debugName;
  subDesc.type = GfxRayTracingBvhType::eGeometry;
  subDesc.flags = desc.flags;
  subDesc.size = size.allocationSize;

  return createRayTracingBvh(subDesc, size, std::move(info));
}


GfxRayTracingBvh GfxVulkanDevice::createRayTracingBvh(
  const GfxRayTracingInstanceDesc&    desc) {
  GfxVulkanRayTracingBvhInfo info(*this, desc);
  GfxVulkanRayTracingBvhSize size = computeRayTracingBvhSize(info);

  GfxRayTracingBvhDesc subDesc;
  subDesc.debugName = desc.debugName;
  subDesc.type = GfxRayTracingBvhType::eInstance;
  subDesc.flags = desc.flags;
  subDesc.size = size.allocationSize;

  return createRayTracingBvh(subDesc, size, std::move(info));
}


GfxRenderState GfxVulkanDevice::createRenderState(
  const GfxRenderStateDesc&           desc) {
  return GfxRenderState(m_pipelineManager->createRenderState(desc));
}


GfxRenderTargetState GfxVulkanDevice::createRenderTargetState(
  const GfxRenderTargetStateDesc&     desc) {
  return GfxRenderTargetState(m_pipelineManager->createRenderTargetState(desc));
}


GfxSampler GfxVulkanDevice::createSampler(
  const GfxSamplerDesc&               desc) {
  return GfxSampler(std::make_shared<GfxVulkanSampler>(*this, desc));
}


GfxSemaphore GfxVulkanDevice::createSemaphore(
  const GfxSemaphoreDesc&             desc) {
  return GfxSemaphore(std::make_shared<GfxVulkanSemaphore>(
    *this, desc, VK_SEMAPHORE_TYPE_TIMELINE));
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
        uint32_t                      imageId,
        uint64_t                      presentId) {
  VkPresentIdKHR presentIdInfo = { VK_STRUCTURE_TYPE_PRESENT_ID_KHR };
  presentIdInfo.swapchainCount = 1;
  presentIdInfo.pPresentIds = &presentId;

  VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = &semaphore;
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = &swapchain;
  presentInfo.pImageIndices = &imageId;

  if (m_features.khrPresentId.presentId)
    presentIdInfo.pNext = std::exchange(presentInfo.pNext, &presentIdInfo);

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


bool GfxVulkanDevice::supportsFragmentShadingRateWithState(
  const GfxVulkanRenderState&         state) const {
  if (!m_properties.khrFragmentShadingRate.fragmentShadingRateWithConservativeRasterization) {
    VkPipelineRasterizationConservativeStateCreateInfoEXT rsConservativeState = state.getRsConservativeState();

    if (rsConservativeState.conservativeRasterizationMode != VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT)
      return false;
  }

  if (!m_properties.khrFragmentShadingRate.fragmentShadingRateWithSampleMask) {
    VkSampleMask fullMask = (1u << VkSampleMask(state.getSampleCount())) - 1u;
    VkSampleMask currMask = state.getSampleMask();

    if ((currMask & fullMask) != fullMask)
      return false;
  }

  return true;
}


GfxShaderStages GfxVulkanDevice::queryEnabledShaderStages() const {
  GfxShaderStages result = GfxShaderStage::eVertex | GfxShaderStage::eFragment | GfxShaderStage::eCompute;

  if (m_features.core.features.geometryShader)
    result |= GfxShaderStage::eGeometry;

  if (m_features.core.features.tessellationShader)
    result |= GfxShaderStage::eTessControl | GfxShaderStage::eTessEval;

  if (m_features.extMeshShader.meshShader)
    result |= GfxShaderStage::eMesh;

  if (m_features.extMeshShader.taskShader)
    result |= GfxShaderStage::eTask;

  return result;
}


GfxVulkanMemoryTypeMasks GfxVulkanDevice::queryMemoryTypeMasks() const {
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


Extent2D GfxVulkanDevice::determineShadingRateTileSize() const {
  // We can pretty much ignore the maximum supported tile size here
  // since it's guaranteed to be at least 8. Aim for the smallest
  // supported tile size that is square and at least 8.
  return Extent2D(
    std::max(8u, m_properties.khrFragmentShadingRate.minFragmentShadingRateAttachmentTexelSize.width),
    std::max(8u, m_properties.khrFragmentShadingRate.minFragmentShadingRateAttachmentTexelSize.height));
}


std::vector<VkPhysicalDeviceFragmentShadingRateKHR> GfxVulkanDevice::determineShadingRates() const {
  std::vector<VkPhysicalDeviceFragmentShadingRateKHR> rates;

  if (!m_features.khrFragmentShadingRate.pipelineFragmentShadingRate
   || !m_features.khrFragmentShadingRate.attachmentFragmentShadingRate)
    return rates;

  uint32_t rateCount = 0;
  VkResult vr = m_vk.vkGetPhysicalDeviceFragmentShadingRatesKHR(m_vk.adapter, &rateCount, nullptr);

  if (vr != VK_SUCCESS) {
    Log::err("Vulkan: Failed to query available shading rates", vr);
    return rates;
  }

  rates.reserve(rateCount);

  for (uint32_t i = 0; i < rateCount; i++)
    rates.push_back({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_KHR });

  vr = m_vk.vkGetPhysicalDeviceFragmentShadingRatesKHR(m_vk.adapter, &rateCount, rates.data());

  if (vr != VK_SUCCESS) {
    Log::err("Vulkan: Failed to query available shading rates", vr);
    rates.clear();
  }

  return rates;
}


GfxVulkanRayTracingBvhSize GfxVulkanDevice::computeRayTracingBvhSize(
  const GfxVulkanRayTracingBvhInfo&   info) const {
  std::vector<uint32_t> primitiveCounts(info.rangeInfos.size());

  for (size_t i = 0; i < info.rangeInfos.size(); i++)
    primitiveCounts[i] = info.rangeInfos[i].primitiveCount;

  VkAccelerationStructureBuildSizesInfoKHR sizeInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };

  m_vk.vkGetAccelerationStructureBuildSizesKHR(m_vk.device,
    VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
    &info.info, primitiveCounts.data(), &sizeInfo);

  GfxVulkanRayTracingBvhSize result;
  result.allocationSize = sizeInfo.accelerationStructureSize;
  result.scratchSizeForUpdate = sizeInfo.updateScratchSize;
  result.scratchSizeForBuild = sizeInfo.buildScratchSize;

  return result;
}


GfxRayTracingBvh GfxVulkanDevice::createRayTracingBvh(
  const GfxRayTracingBvhDesc&         desc,
  const GfxVulkanRayTracingBvhSize&   size,
        GfxVulkanRayTracingBvhInfo&&  info) {
  VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
  bufferInfo.size = size.allocationSize;
  bufferInfo.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                   | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;

  getQueueSharingInfo(
    &bufferInfo.sharingMode,
    &bufferInfo.queueFamilyIndexCount,
    &bufferInfo.pQueueFamilyIndices);

  GfxVulkanMemoryRequirements requirements = { };
  requirements.dedicated = { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS };
  requirements.core = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, &requirements.dedicated };

  VkDeviceBufferMemoryRequirements requirementInfo = { VK_STRUCTURE_TYPE_DEVICE_BUFFER_MEMORY_REQUIREMENTS };
  requirementInfo.pCreateInfo = &bufferInfo;

  m_vk.vkGetDeviceBufferMemoryRequirements(m_vk.device, &requirementInfo, &requirements.core);

  // If possible, allocate memory first so that we can exit
  // early on failure, without creating a resource object.
  GfxVulkanMemorySlice memorySlice;

  GfxVulkanMemoryAllocationInfo allocationInfo = { };
  allocationInfo.tiling = VK_IMAGE_TILING_LINEAR;
  allocationInfo.memoryTypes = GfxMemoryType::eAny;

  if (!requirements.dedicated.prefersDedicatedAllocation) {
    if (!(memorySlice = m_memoryAllocator->allocateMemory(requirements, allocationInfo)))
      return GfxRayTracingBvh();
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
      return GfxRayTracingBvh();
    }
  }

  VkBindBufferMemoryInfo bind = { VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO };
  bind.buffer = buffer;
  bind.memory = memorySlice.getHandle();
  bind.memoryOffset = memorySlice.getOffset();

  vr = m_vk.vkBindBufferMemory2(m_vk.device, 1, &bind);

  if (vr) {
    m_vk.vkDestroyBuffer(m_vk.device, buffer, nullptr);
    throw VulkanError("Vulkan: Failed to bind buffer memory", vr);
  }

  VkAccelerationStructureCreateInfoKHR rtasInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
  rtasInfo.buffer = buffer;
  rtasInfo.offset = 0;
  rtasInfo.size = size.allocationSize;
  rtasInfo.type = info.info.type;

  VkAccelerationStructureKHR rtas = VK_NULL_HANDLE;
  vr = m_vk.vkCreateAccelerationStructureKHR(m_vk.device, &rtasInfo, nullptr, &rtas);

  if (vr) {
    m_vk.vkDestroyBuffer(m_vk.device, buffer, nullptr);
    throw VulkanError("Vulkan: Failed to create acceleration structure", vr);
  }

  VkAccelerationStructureDeviceAddressInfoKHR vaInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
  vaInfo.accelerationStructure = rtas;

  VkDeviceAddress va = m_vk.vkGetAccelerationStructureDeviceAddressKHR(m_vk.device, &vaInfo);

  return GfxRayTracingBvh(std::make_shared<GfxVulkanRayTracingBvh>(*this,
    desc, std::move(info), size, buffer, rtas, va, std::move(memorySlice)));
}

}
