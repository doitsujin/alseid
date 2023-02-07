#pragma once

#include <unordered_map>
#include <vector>

#include "gfx_vulkan_loader.h"

namespace as {

/**
 * \brief Barrier batch helper class
 */
class GfxVulkanBarrierBatch {

public:

  GfxVulkanBarrierBatch();

  ~GfxVulkanBarrierBatch();

  /**
   * \brief Adds a global memory barrier
   *
   * Memory barriers are always batched.
   * \param [in] vk Vulkan dispatch table
   * \param [in] cmd Vulkan command buffer
   * \param [in] barrier Memory barrier to add
   */
  void addMemoryBarrier(
    const VkMemoryBarrier2&             barrier);

  /**
   * \brief Adds an image memory barrier
   *
   * If any existing image barrier in the batch has overlapping
   * subresources, the batch will be flushed and the new barrier
   * will be added into a new batch.
   * \param [in] vk Vulkan dispatch table
   * \param [in] cmd Vulkan command buffer
   * \param [in] barrier Image barrier to add
   */
  void addImageBarrier(
    const GfxVulkanProcs&               vk,
          VkCommandBuffer               cmd,
    const VkImageMemoryBarrier2&        barrier);

  /**
   * \brief Records barriers into a command buffer
   *
   * \param [in] vk Vulkan dispatch table
   * \param [in] cmd Vulkan command buffer
   */
  void recordCommands(
    const GfxVulkanProcs&               vk,
          VkCommandBuffer               cmd) {
    if ((m_memoryBarrier.srcStageMask | m_memoryBarrier.dstStageMask)
     || (!m_imageBarriers.empty()))
      flush(vk, cmd);
  }

private:

  static const VkAccessFlags2 WriteAccessMask;

  VkMemoryBarrier2 m_memoryBarrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };

  std::vector<VkImageMemoryBarrier2> m_imageBarriers;

  std::unordered_multimap<VkImage,
    VkImageSubresourceRange> m_imageLookup;

  void flush(
    const GfxVulkanProcs&               vk,
          VkCommandBuffer               cmd);

};

}
