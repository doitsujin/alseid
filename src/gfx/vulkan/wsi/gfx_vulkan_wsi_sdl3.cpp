#include "../../../util/util_log.h"

#include "../../../wsi/sdl3/wsi_sdl3.h"

#include "gfx_vulkan_wsi_sdl3.h"

namespace as {

GfxVulkanSdl3Wsi::GfxVulkanSdl3Wsi(
  const Wsi&                          wsi,
  const GfxVulkanProcs&               vk)
: m_wsi (std::static_pointer_cast<WsiSdl3>(wsi.getShared()))
, m_vk  (vk) {
  Log::info("Initializing SDL3 Vulkan WSI bridge");
}


GfxVulkanSdl3Wsi::~GfxVulkanSdl3Wsi() {

}


PFN_vkGetInstanceProcAddr GfxVulkanSdl3Wsi::getVulkanEntryPoint() {
  return reinterpret_cast<PFN_vkGetInstanceProcAddr>(
    SDL_Vulkan_GetVkGetInstanceProcAddr());
}


void GfxVulkanSdl3Wsi::getInstanceExtensions(
        uint32_t*                     extensionCount,
  const char**                        extensionNames) {
  Uint32 count = 0;

  auto* extensions = SDL_Vulkan_GetInstanceExtensions(&count);

  if (!extensions)
    throw Sdl3Error("SDL_Vulkan_GetInstanceExtensions failed");

  if (extensionNames) {
    for (uint32_t i = 0; i < *extensionCount && i < count; i++)
      extensionNames[i] = extensions[i];
  }

  *extensionCount = count;
}


Extent2D GfxVulkanSdl3Wsi::getSurfaceSize(
  const WsiWindow&                    window) {
  auto& sdlWindow = static_cast<WsiSdl3Window&>(*window);

  int w = 0;
  int h = 0;

  SDL_GetWindowSizeInPixels(sdlWindow.getWindowHandle(), &w, &h);
  return Extent2D(uint32_t(w), uint32_t(h));
}


bool GfxVulkanSdl3Wsi::checkSurfaceSupport(
        VkPhysicalDevice              adapter,
        uint32_t                      queueFamily) {
  return SDL_Vulkan_GetPresentationSupport(m_vk.instance, adapter, queueFamily);
}


VkResult GfxVulkanSdl3Wsi::createSurface(
  const WsiWindow&                    window,
        VkSurfaceKHR*                 surface) {
  auto& sdlWindow = static_cast<WsiSdl3Window&>(*window);

  bool success = SDL_Vulkan_CreateSurface(
    sdlWindow.getWindowHandle(), m_vk.instance, nullptr, surface);

  // We don't get a precise error code from SDL so just make one up
  return success ? VK_SUCCESS : VK_ERROR_NATIVE_WINDOW_IN_USE_KHR;
}

}
