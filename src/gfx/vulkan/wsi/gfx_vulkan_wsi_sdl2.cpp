#include "../../../util/util_log.h"

#include "../../../wsi/sdl2/wsi_sdl2.h"

#include "gfx_vulkan_wsi_sdl2.h"

namespace as {

GfxVulkanSdl2Wsi::GfxVulkanSdl2Wsi(
  const Wsi&                          wsi,
  const GfxVulkanProcs&               vk)
: m_wsi (std::static_pointer_cast<WsiSdl2>(wsi.getShared()))
, m_vk  (vk) {
  Log::info("Initializing SDL2 Vulkan WSI bridge");
}


GfxVulkanSdl2Wsi::~GfxVulkanSdl2Wsi() {
  if (m_dummySurface)
    m_vk.vkDestroySurfaceKHR(m_vk.instance, m_dummySurface, VK_NULL_HANDLE);

  if (m_dummyWindow)
    SDL_DestroyWindow(m_dummyWindow);
}


PFN_vkGetInstanceProcAddr GfxVulkanSdl2Wsi::getVulkanEntryPoint() {
  return reinterpret_cast<PFN_vkGetInstanceProcAddr>(
    SDL_Vulkan_GetVkGetInstanceProcAddr());
}


void GfxVulkanSdl2Wsi::getInstanceExtensions(
        uint32_t*                     extensionCount,
  const char**                        extensionNames) {
  if (!SDL_Vulkan_GetInstanceExtensions(nullptr, extensionCount, extensionNames))
    throw SdlError("SDL_Vulkan_GetInstanceExtensions failed");
}


Extent2D GfxVulkanSdl2Wsi::getSurfaceSize(
  const WsiWindow&                    window) {
  auto& sdlWindow = static_cast<WsiSdl2Window&>(*window);

  int w = 0;
  int h = 0;

  SDL_Vulkan_GetDrawableSize(sdlWindow.getWindowHandle(), &w, &h);
  return Extent2D(uint32_t(w), uint32_t(h));
}


bool GfxVulkanSdl2Wsi::checkSurfaceSupport(
        VkPhysicalDevice              adapter,
        uint32_t                      queueFamily) {
  std::lock_guard lock(m_mutex);

  VkResult vr = VK_ERROR_SURFACE_LOST_KHR;
  VkBool32 status = VK_FALSE;

  if (m_dummySurface) {
    vr = m_vk.vkGetPhysicalDeviceSurfaceSupportKHR(
      adapter, queueFamily, m_dummySurface, &status);
  }

  if (!m_dummyWindow) {
    m_dummyWindow = SDL_CreateWindow(nullptr,
      SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
      64, 64, SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN);

    if (!m_dummyWindow)
      throw SdlError("Vulkan: Failed to create SDL dummy window");
  }

  while (vr == VK_ERROR_SURFACE_LOST_KHR) {
    if (m_dummySurface)
      m_vk.vkDestroySurfaceKHR(m_vk.instance, m_dummySurface, nullptr);

    SDL_bool success = SDL_Vulkan_CreateSurface(
      m_dummyWindow, m_vk.instance, &m_dummySurface);

    if (!success)
      throw SdlError("Vulkan: Failed to create dummy surface");

    vr = m_vk.vkGetPhysicalDeviceSurfaceSupportKHR(
      adapter, queueFamily, m_dummySurface, &status);
  }

  if (vr != VK_SUCCESS)
    throw VulkanError("Vulkan: Failed to query surface support", vr);

  return status;
}


VkResult GfxVulkanSdl2Wsi::createSurface(
  const WsiWindow&                    window,
        VkSurfaceKHR*                 surface) {
  auto& sdlWindow = static_cast<WsiSdl2Window&>(*window);

  SDL_bool success = SDL_Vulkan_CreateSurface(
    sdlWindow.getWindowHandle(), m_vk.instance, surface);

  // We don't get a precise error code from SDL so just make one up
  return success ? VK_SUCCESS : VK_ERROR_NATIVE_WINDOW_IN_USE_KHR;
}

}
