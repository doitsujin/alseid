#pragma once

#include <array>
#include <optional>
#include <vector>

#include "../gfx_device.h"

#include "gfx_vulkan_loader.h"

#include "wsi/gfx_vulkan_wsi.h"

namespace as {

/**
 * \brief Queue info
 *
 * Stores queue metadata.
 */
struct GfxVulkanQueueMetadata {
  /** Vulkan queue family index */
  uint32_t queueFamily;
  /** Vulkan queue index within the queue family.
   *  Used when retrieving the Vulkan queue object. */
  uint32_t queueIndexInFamily;
  /** Internal queue index. Used to look up existing
   *  queue objects during device creation. */
  uint32_t queueIndexInDevice;
  /** Queue prority within the queue family */
  float priority;
};

/**
 * \brief Vulkan queue properties
 */
struct GfxVulkanQueueProperties {
  /** Vulkan queue flags */
  VkQueueFlags queueFlags;
  /** Total number of queues */
  uint32_t queueCount;
  /** Number of queues used */
  uint32_t queuesUsed;
};

/**
 * \brief Vulkan queue object
 */
struct GfxVulkanQueue {
  /** Queue handle */
  VkQueue queue;
  /** Queue family index */
  uint32_t queueFamily;
};

/**
 * \brief Queue mapping and create info builder
 *
 * Convenience class that is used internally to
 * aid with device and queue creation.
 */
class GfxVulkanQueueMapping {
  constexpr static uint32_t MaxQueueCount = uint32_t(GfxQueue::eQueueCount);
public:

  GfxVulkanQueueMapping(
    const GfxVulkanProcs&               vk,
    const GfxVulkanWsi&                 wsi);

  ~GfxVulkanQueueMapping();

  /**
   * \brief Queries metadata of a given queue
   *
   * Note that multiple queues may return metadata
   * for the same hardware queue.
   * \param [in] queue Queue to query
   * \returns Queue info
   */
  std::optional<GfxVulkanQueueMetadata> getQueueMetadata(
          GfxQueue                      queue) const;

  /**
   * \brief Queries queue create infos
   *
   * \param [out] pQueueCreateCount Filled with the number of queues to create
   * \param [out] ppQueueCreateInfos Filled with a pointer to queue create infos
   */
  void getQueueCreateInfos(
          uint32_t*                     pQueueCreateCount,
    const VkDeviceQueueCreateInfo**     ppQueueCreateInfos) const;

private:

  uint32_t m_queueCount       = 0;
  uint32_t m_queueCreateCount = 0;

  std::vector<GfxVulkanQueueProperties> m_deviceQueueProperties;

  std::array<GfxVulkanQueueMetadata,  MaxQueueCount> m_queueMetadata     = { };
  std::array<uint32_t,                MaxQueueCount> m_queueMap          = { };
  std::array<VkDeviceQueueCreateInfo, MaxQueueCount> m_queueCreateInfos  = { };
  std::array<float,                   MaxQueueCount> m_queuePriorities   = { };

  void mapQueue(
          GfxQueue                      queue,
          uint32_t                      index);

  uint32_t reserveQueue(
          float                         priority,
          VkQueueFlags                  queueFlagMask,
          VkQueueFlags                  queueFlags);

  uint32_t reserveQueueFromFamily(
          uint32_t                      queueFamily,
          float                         priority);

};

}