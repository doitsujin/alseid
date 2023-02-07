#pragma once

#include <memory>
#include <vector>

#include "../gfx.h"

#include "gfx_vulkan_adapter.h"
#include "gfx_vulkan_loader.h"

#include "wsi/gfx_vulkan_wsi.h"

namespace as {

/**
 * \brief Vulkan graphics backend
 */
class GfxVulkan : public GfxIface
, public std::enable_shared_from_this<GfxVulkan> {
public:

  GfxVulkan(
    const Wsi&                          wsi,
          GfxInstanceFlags              flags);

  ~GfxVulkan();

  /**
   * \brief Queries Vulkan functions
   * \returns Vulkan functions
   */
  const GfxVulkanProcs& vk() const {
    return m_vk;
  }

  /**
   * \brief Queries Vulkan WSI bridge
   * \returns Vulkan WSI bridge
   */
  const GfxVulkanWsi& getWsiBridge() const {
    return m_wsiBridge;
  }

  /**
   * \brief Queries instance flags
   * \returns Instance flags
   */
  GfxInstanceFlags getInstanceFlags() const {
    return m_flags;
  }

  /**
   * \brief Queries backend type
   * \returns Backend type
   */
  GfxBackend getBackendType() const override;

  /**
   * \brief Enumerates graphics adapters
   *
   * \param [in] index Adapter index
   * \returns Adapter object
   */
  GfxAdapter enumAdapters(
          uint32_t                      index) override;

  /**
   * \brief Creates a logical device
   *
   * \param [in] adapter Adapter to create device for
   * \returns Device object
   */
  GfxDevice createDevice(
    const GfxAdapter&                   adapter) override;

private:

  GfxVulkanWsi      m_wsiBridge;
  GfxVulkanProcs    m_vk;
  GfxInstanceFlags  m_flags;

  VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;

  std::vector<std::shared_ptr<GfxVulkanAdapter>> m_adapters;

  void destroyObjects();

};

}
