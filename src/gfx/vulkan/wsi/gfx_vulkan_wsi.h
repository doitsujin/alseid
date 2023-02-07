#pragma once

#include <memory>

#include "../../../util/util_iface.h"
#include "../../../util/util_types.h"

#include "../../../wsi/wsi.h"
#include "../../../wsi/wsi_window.h"

#include "../gfx_vulkan_loader.h"

namespace as {

/**
 * \brief Vulkan WSI bridge interface
 */
class GfxVulkanWsiIface {

public:

  virtual ~GfxVulkanWsiIface() { }

  /**
   * \brief Loads Vulkan library and queries entry point
   * \returns Pointer to \c vkGetInstanceProcAddr
   */
  virtual PFN_vkGetInstanceProcAddr getVulkanEntryPoint() = 0;

  /**
   * \brief Queries required instance extensions
   *
   * \param [in,out] extensionCount Number of extensions*
   * \param [out] extensionNames Extensions to enable
   */
  virtual void getInstanceExtensions(
          uint32_t*                     extensionCount,
    const char**                        extensionNames) = 0;

  /**
   * \brief Queries surface size of the given window
   *
   * \param [in] window Window to query
   * \returns Drawable surface size
   */
  virtual Extent2D getSurfaceSize(
    const WsiWindow&                    window) = 0;

  /**
   * \brief Checks whether a given queue can do presentation
   *
   * \param [in] adapter Physical device
   * \param [in] queueFamily Queue family index
   * \returns \c true if presentation is supported 
   */
  virtual bool checkSurfaceSupport(
          VkPhysicalDevice              adapter,
          uint32_t                      queueFamily) = 0;

  /**
   * \brief Creates a Vulkan surface for the given window
   *
   * \param [in] window Window
   * \param [out] surface Surface
   * \returns Vulkan result of the operation
   */
  virtual VkResult createSurface(
    const WsiWindow&                    window,
          VkSurfaceKHR*                 surface) = 0;

};


/**
 * \brief Vulkan WSI bridge
 */
class GfxVulkanWsi : public IfaceRef<GfxVulkanWsiIface> {

public:

  GfxVulkanWsi() { }
  GfxVulkanWsi(std::nullptr_t) { }

  /**
   * \brief Initializes Vulkan WSI bridge
   * \param [in] wsi WSI instance
   */
  GfxVulkanWsi(
    const Wsi&                          wsi,
    const GfxVulkanProcs&               vk);

private:

  std::shared_ptr<GfxVulkanWsiIface> initBackend(
    const Wsi&                          wsi,
    const GfxVulkanProcs&               vk);

};

}
