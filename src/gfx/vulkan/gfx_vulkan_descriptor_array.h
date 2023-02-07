#pragma once

#include "../gfx_descriptor_array.h"

#include "gfx_vulkan_descriptor_handle.h"
#include "gfx_vulkan_device.h"

namespace as {

/**
 * \brief Vulkan descriptor array
 *
 * Consists of a single descriptor set and pool.
 */
class GfxVulkanDescriptorArray : public GfxDescriptorArrayIface {

public:

  GfxVulkanDescriptorArray(
          std::shared_ptr<GfxVulkanDevice> device,
    const GfxDescriptorArrayDesc&       desc);

  ~GfxVulkanDescriptorArray();

  /**
   * \brief Retrieves Vulkan descriptor set
   * \returns Vulkan descriptor set
   */
  VkDescriptorSet getHandle() const {
    return m_set;
  }

  /**
   * \brief Writes descriptors
   *
   * \param [in] index First descriptor to write 
   * \param [in] count Number of descriptors to write 
   * \param [in] descriptors Descriptor array 
   */
  void setDescriptors(
          uint32_t                      index,
          uint32_t                      count,
    const GfxDescriptor*                descriptors) override;

private:

  std::shared_ptr<GfxVulkanDevice> m_device;

  VkDescriptorType  m_type  = VK_DESCRIPTOR_TYPE_MAX_ENUM;
  uint32_t          m_size  = 0;

  VkDescriptorPool  m_pool  = VK_NULL_HANDLE;
  VkDescriptorSet   m_set   = VK_NULL_HANDLE;

  VkSampler         m_sampler = VK_NULL_HANDLE;

};

}
