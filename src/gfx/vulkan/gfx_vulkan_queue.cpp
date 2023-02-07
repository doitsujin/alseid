#include "../../util/util_log.h"
#include "../../util/util_string.h"

#include "gfx_vulkan_queue.h"

namespace as {

GfxVulkanQueueMapping::GfxVulkanQueueMapping(
  const GfxVulkanProcs&               vk,
  const GfxVulkanWsi&                 wsi) {
  // Query Vulkan queue properties from the adapter and convert
  // them into a format that we can more easily work with
  uint32_t queuePropertyCount = 0;
  vk.vkGetPhysicalDeviceQueueFamilyProperties(vk.adapter, &queuePropertyCount, nullptr);

  std::vector<VkQueueFamilyProperties> queueProperties(queuePropertyCount);
  vk.vkGetPhysicalDeviceQueueFamilyProperties(vk.adapter, &queuePropertyCount, queueProperties.data());

  m_deviceQueueProperties.resize(queuePropertyCount);

  for (uint32_t i = 0; i < queuePropertyCount; i++) {
    auto& dstInfo = m_deviceQueueProperties[i];
    auto& srcInfo = queueProperties[i];

    dstInfo.queueFlags = srcInfo.queueFlags;
    dstInfo.queueCount = srcInfo.queueCount;
    dstInfo.queuesUsed = 0;
  }

  std::fill(m_queueMap.begin(), m_queueMap.end(), VK_QUEUE_FAMILY_IGNORED);

  // Unconditionally allocate a graphics queue, regardless of given
  // flags. This will make the fallback logic significantly simpler.
  uint32_t graphicsQueue = reserveQueue(1.0f,
    VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT,
    VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);

  if (graphicsQueue == VK_QUEUE_FAMILY_IGNORED)
    throw VulkanError("Vulkan: No graphics queue found on device.", VK_SUCCESS);

  mapQueue(GfxQueue::eGraphics, graphicsQueue);

  // Reserve asynchronous background compute queue
  uint32_t computeBackgroundQueue = reserveQueue(0.0f,
    VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT,
    VK_QUEUE_COMPUTE_BIT);

  if (computeBackgroundQueue == VK_QUEUE_FAMILY_IGNORED)
    computeBackgroundQueue = graphicsQueue;

  mapQueue(GfxQueue::eComputeBackground, computeBackgroundQueue);

  // Reserve a high-priority synchronous compute queue. We do this after
  // reserving the background queue since the background queue being truly
  // asynchronous is more important than the regular compute queue.
  uint32_t computeQueue = reserveQueue(1.0f,
    VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT,
    VK_QUEUE_COMPUTE_BIT);

  if (computeQueue == VK_QUEUE_FAMILY_IGNORED)
    computeQueue = graphicsQueue;

  mapQueue(GfxQueue::eCompute, computeQueue);

  // Reserve asynchronous transfer queue and fall back to the
  // asynchronous compute if we can't find a dedicated queue.
  uint32_t uploadQueue = reserveQueue(0.0f,
    VK_QUEUE_TRANSFER_BIT,
    VK_QUEUE_TRANSFER_BIT);

  if (uploadQueue == VK_QUEUE_FAMILY_IGNORED) {
    uploadQueue = reserveQueue(0.0f,
      VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT,
      VK_QUEUE_COMPUTE_BIT);
  }

  if (uploadQueue == VK_QUEUE_FAMILY_IGNORED) {
    uploadQueue = computeBackgroundQueue != VK_QUEUE_FAMILY_IGNORED
      ? computeBackgroundQueue
      : graphicsQueue;
  }

  mapQueue(GfxQueue::eTransferUpload, uploadQueue);

  // Reserve readback queue and fall back to the graphics
  // queue if we can't find a dedicated queue.
  uint32_t readbackQueue = reserveQueue(1.0f,
    VK_QUEUE_TRANSFER_BIT,
    VK_QUEUE_TRANSFER_BIT);

  if (readbackQueue == VK_QUEUE_FAMILY_IGNORED) {
    readbackQueue = reserveQueue(0.0f,
      VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT,
      VK_QUEUE_COMPUTE_BIT);
  }

  if (readbackQueue == VK_QUEUE_FAMILY_IGNORED)
    readbackQueue = graphicsQueue;

  mapQueue(GfxQueue::eTransferReadback, readbackQueue);

  // Reserve a dedicated sparse binding queue on the device.
  // If no such queue exists, try to find an existing one.
  uint32_t sparseBindingQueue = reserveQueue(1.0f,
    VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT | VK_QUEUE_SPARSE_BINDING_BIT,
    VK_QUEUE_SPARSE_BINDING_BIT);

  if (sparseBindingQueue == VK_QUEUE_FAMILY_IGNORED) {
    for (uint32_t i = 0; i < m_queueCount; i++) {
      uint32_t queueFamily = m_queueMetadata[i].queueFamily;

      if (m_deviceQueueProperties[queueFamily].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) {
        sparseBindingQueue = i;
        break;
      }
    }
  }

  mapQueue(GfxQueue::eSparseBinding, sparseBindingQueue);

  // Check if the preferred existing queue can present to the given
  // surface. If not, allocate a presentation queue as necessary.
  // Note that the presentation queue is only actully used if the
  // app presents to a queue that does not support presentation.
  uint32_t presentQueue = VK_QUEUE_FAMILY_IGNORED;

  if (wsi->checkSurfaceSupport(vk.adapter, m_queueMetadata[graphicsQueue].queueFamily))
    presentQueue = graphicsQueue;

  if (presentQueue == VK_QUEUE_FAMILY_IGNORED) {
    for (uint32_t i = 0; i < m_queueCount; i++) {
      if (wsi->checkSurfaceSupport(vk.adapter, m_queueMetadata[i].queueFamily)) {
        presentQueue = i;
        break;
      }
    }
  }

  if (presentQueue == VK_QUEUE_FAMILY_IGNORED) {
    for (uint32_t i = 0; i < queuePropertyCount; i++) {
      if (wsi->checkSurfaceSupport(vk.adapter, i)) {
        presentQueue = reserveQueueFromFamily(i, 1.0f);
        break;
      }
    }
  }

  mapQueue(GfxQueue::ePresent, presentQueue);

  // Fill in queue create infos per queue family, as required by the Vulkan API.
  uint32_t queuePriorityIndex = 0;

  for (uint32_t i = 0; i < queuePropertyCount; i++) {
    VkDeviceQueueCreateInfo info = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    info.queueFamilyIndex = i;
    info.pQueuePriorities = &m_queuePriorities[queuePriorityIndex];

    for (uint32_t j = 0; j < m_queueCount; j++) {
      const auto& metadata = m_queueMetadata[j];

      if (metadata.queueFamily != info.queueFamilyIndex)
        continue;

      info.queueCount = std::max(info.queueCount, metadata.queueIndexInFamily + 1);
      m_queuePriorities[queuePriorityIndex++] = metadata.priority;
    }

    if (info.queueCount)
      m_queueCreateInfos[m_queueCreateCount++] = info;
  }
}


GfxVulkanQueueMapping::~GfxVulkanQueueMapping() {

}


std::optional<GfxVulkanQueueMetadata> GfxVulkanQueueMapping::getQueueMetadata(
        GfxQueue                      queue) const {
  uint32_t index = m_queueMap[uint32_t(queue)];

  if (index == VK_QUEUE_FAMILY_IGNORED)
    return std::nullopt;

  return m_queueMetadata[index];
}


void GfxVulkanQueueMapping::getQueueCreateInfos(
        uint32_t*                     pQueueCreateCount,
  const VkDeviceQueueCreateInfo**     ppQueueCreateInfos) const {
  if (pQueueCreateCount)
    *pQueueCreateCount = m_queueCreateCount;

  if (ppQueueCreateInfos)
    *ppQueueCreateInfos = m_queueCreateInfos.data();
}


void GfxVulkanQueueMapping::mapQueue(
        GfxQueue                      queue,
        uint32_t                      index) {
  static const std::array<const char*, MaxQueueCount> queueNames = {{
    "eGraphics:          ",
    "eCompute:           ",
    "eComputeBackground: ",
    "eTransferUpload:    ",
    "eTransferReadback:  ",
    "eSparseBinding:     ",
    "ePresent:           ",
  }};

  m_queueMap[uint32_t(queue)] = index;

  Log::info("Vulkan: GfxQueue::", queueNames[uint32_t(queue)], index == VK_QUEUE_FAMILY_IGNORED ? "n/a"
    : strcat(m_queueMetadata[index].queueFamily, ":", m_queueMetadata[index].queueIndexInFamily));
}


uint32_t GfxVulkanQueueMapping::reserveQueue(
        float                         priority,
        VkQueueFlags                  queueFlagMask,
        VkQueueFlags                  queueFlags) {
  // Allocate a queue from the first matching queue family
  for (uint32_t i = 0; i < m_deviceQueueProperties.size(); i++) {
    auto& family = m_deviceQueueProperties[i];

    if ((family.queueFlags & queueFlagMask) == queueFlags) {
      uint32_t index = reserveQueueFromFamily(i, priority);

      if (index != VK_QUEUE_FAMILY_IGNORED)
        return index;
    }
  }

  return VK_QUEUE_FAMILY_IGNORED;
}


uint32_t GfxVulkanQueueMapping::reserveQueueFromFamily(
        uint32_t                      queueFamily,
        float                         priority) {
  auto& family = m_deviceQueueProperties[queueFamily];

  if (family.queuesUsed == family.queueCount)
    return VK_QUEUE_FAMILY_IGNORED;

  uint32_t index = m_queueCount++;

  auto& info = m_queueMetadata[index];
  info.queueFamily = queueFamily;
  info.queueIndexInFamily = family.queuesUsed++;
  info.queueIndexInDevice = index;
  info.priority = priority;
  return index;
}

}
