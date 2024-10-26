#pragma once

#include "../../../wsi/sdl3/wsi_sdl3_include.h"

#include "gfx_vulkan_wsi.h"

namespace as {

class WsiSdl3;

/**
 * \brief Vulkan SDL3 bridge
 */
class GfxVulkanSdl3Wsi : public GfxVulkanWsiIface {

public:

  GfxVulkanSdl3Wsi(
    const Wsi&                          wsi,
    const GfxVulkanProcs&               vk);

  ~GfxVulkanSdl3Wsi();

  /**
   * \brief Loads Vulkan library and queries entry point
   * \returns Pointer to \c vkGetInstanceProcAddr
   */
  PFN_vkGetInstanceProcAddr getVulkanEntryPoint() override;

  /**
   * \brief Queries required instance extensions
   *
   * \param [in,out] extensionCount Number of extensions*
   * \param [out] extensionNames Extensions to enable
   */
  void getInstanceExtensions(
          uint32_t*                     extensionCount,
    const char**                        extensionNames) override;

  /**
   * \brief Queries surface size of the given window
   *
   * \param [in] window Window to query
   * \returns Drawable surface size
   */
  Extent2D getSurfaceSize(
    const WsiWindow&                    window) override;

  /**
   * \brief Checks whether a given queue can do presentation
   *
   * \param [in] adapter Physical device
   * \param [in] queueFamily Queue family index
   * \returns \c true if presentation is supported 
   */
  bool checkSurfaceSupport(
          VkPhysicalDevice              adapter,
          uint32_t                      queueFamily) override;

  /**
   * \brief Creates a Vulkan surface for the given window
   *
   * \param [in] window Window
   * \param [out] surface Surface
   * \returns Vulkan result of the operation
   */
  VkResult createSurface(
    const WsiWindow&                    window,
          VkSurfaceKHR*                 surface) override;

private:

  std::shared_ptr<WsiSdl3>  m_wsi;
  const GfxVulkanProcs&     m_vk;

};

}
