#include "../../../util/util_log.h"

#include "gfx_vulkan_wsi_null.h"

namespace as {

GfxVulkanNullWsi::GfxVulkanNullWsi() {
  Log::info("Initializing headless Vulkan WSI bridge");
}


GfxVulkanNullWsi::~GfxVulkanNullWsi() {

}


PFN_vkGetInstanceProcAddr GfxVulkanNullWsi::getVulkanEntryPoint() {
  return nullptr; // TODO
}


void GfxVulkanNullWsi::getInstanceExtensions(
        uint32_t*                     extensionCount,
  const char**                        extensionNames) {
  *extensionCount = 0;
}


Extent2D GfxVulkanNullWsi::getSurfaceSize(
  const WsiWindow&                    window) {
  return Extent2D(0u, 0u);    
}


bool GfxVulkanNullWsi::checkSurfaceSupport(
        VkPhysicalDevice              adapter,
        uint32_t                      queueFamily) {
  return false;
}


VkResult GfxVulkanNullWsi::createSurface(
  const WsiWindow&                    window,
        VkSurfaceKHR*                 surface) {
  // It should never be possible to call this since a window
  // is required, but return some bogus error just in case
  return VK_ERROR_INCOMPATIBLE_DISPLAY_KHR;
}

}
