#pragma once

#include "../gfx_sampler.h"

#include "gfx_vulkan_descriptor_handle.h"
#include "gfx_vulkan_device.h"
#include "gfx_vulkan_memory.h"

namespace as {

/**
 * \brief Vulkan sampler
 */
class GfxVulkanSampler : public GfxSamplerIface {

public:

  GfxVulkanSampler(
          GfxVulkanDevice&              device,
    const GfxSamplerDesc&               desc);

  ~GfxVulkanSampler();

  /**
   * \brief Retrieves Vulkan sampler handle
   * \returns Vulkan sampler handle
   */
  VkSampler getHandle() const {
    return m_sampler;
  }

  /**
   * \brief Retrieves sampler descriptor
   *
   * The resulting descriptor can be used to bind the sampler
   * to a shader pipeline. Descriptors may be cached as long
   * as they are not used after the object gets destroyed.
   * \returns Sampler descriptor
   */
  GfxDescriptor getDescriptor() const override;

private:

  GfxVulkanDevice& m_device;

  VkSampler m_sampler = VK_NULL_HANDLE;

};

}
