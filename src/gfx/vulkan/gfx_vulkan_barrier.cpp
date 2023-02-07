#include "gfx_vulkan_barrier.h"

namespace as {

const VkAccessFlags2 GfxVulkanBarrierBatch::WriteAccessMask =
  VK_ACCESS_2_SHADER_WRITE_BIT |
  VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT |
  VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
  VK_ACCESS_2_TRANSFER_WRITE_BIT |
  VK_ACCESS_2_HOST_WRITE_BIT |
  VK_ACCESS_2_MEMORY_WRITE_BIT |
  VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT |
  VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;

GfxVulkanBarrierBatch::GfxVulkanBarrierBatch() {

}


GfxVulkanBarrierBatch::~GfxVulkanBarrierBatch() {

}


void GfxVulkanBarrierBatch::addMemoryBarrier(
  const VkMemoryBarrier2&             barrier) {
  m_memoryBarrier.srcStageMask |= barrier.srcStageMask;
  m_memoryBarrier.srcAccessMask |= barrier.srcAccessMask & WriteAccessMask;
  m_memoryBarrier.dstStageMask |= barrier.dstStageMask;
  m_memoryBarrier.dstAccessMask |= barrier.dstAccessMask;
}


void GfxVulkanBarrierBatch::addImageBarrier(
  const GfxVulkanProcs&               vk,
        VkCommandBuffer               cmd,
  const VkImageMemoryBarrier2&        barrier) {
  auto eq = m_imageLookup.equal_range(barrier.image);

  for (auto i = eq.first; i != eq.second; i++) {
    const VkImageSubresourceRange& a = i->second;
    const VkImageSubresourceRange& b = barrier.subresourceRange;

    bool overlap = (a.aspectMask & b.aspectMask)
      && a.baseMipLevel < b.baseMipLevel + b.levelCount
      && a.baseMipLevel + a.levelCount > b.baseMipLevel
      && a.baseArrayLayer < b.baseArrayLayer + b.layerCount
      && a.baseArrayLayer + a.layerCount > b.baseArrayLayer;

    if (overlap) {
      flush(vk, cmd);
      break;
    }
  }

  m_imageBarriers.emplace_back(barrier).srcAccessMask &= WriteAccessMask;
  m_imageLookup.insert({ barrier.image, barrier.subresourceRange });
}


void GfxVulkanBarrierBatch::flush(
  const GfxVulkanProcs&               vk,
        VkCommandBuffer               cmd) {
  VkDependencyInfo depInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };

  if (m_memoryBarrier.srcStageMask | m_memoryBarrier.dstStageMask) {
    depInfo.memoryBarrierCount = 1;
    depInfo.pMemoryBarriers = &m_memoryBarrier;
  }

  if (!m_imageBarriers.empty()) {
    depInfo.imageMemoryBarrierCount = m_imageBarriers.size();
    depInfo.pImageMemoryBarriers = m_imageBarriers.data();
  }

  if (depInfo.memoryBarrierCount + depInfo.imageMemoryBarrierCount)
    vk.vkCmdPipelineBarrier2(cmd, &depInfo);

  // Reset internal data structures
  m_memoryBarrier.srcStageMask = 0;
  m_memoryBarrier.srcAccessMask = 0;
  m_memoryBarrier.dstStageMask = 0;
  m_memoryBarrier.dstAccessMask = 0;

  m_imageBarriers.clear();
  m_imageLookup.clear();
}

}
