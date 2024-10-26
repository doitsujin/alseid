#include "gfx_vulkan_wsi.h"
#include "gfx_vulkan_wsi_null.h"

#ifdef ALSEID_WSI_SDL3
#include "gfx_vulkan_wsi_sdl3.h"
#endif

#ifdef ALSEID_WSI_SDL2
#include "gfx_vulkan_wsi_sdl2.h"
#endif

namespace as {

GfxVulkanWsi::GfxVulkanWsi(
  const Wsi&                          wsi,
  const GfxVulkanProcs&               vk)
: IfaceRef<GfxVulkanWsiIface>(initBackend(wsi, vk)) {

}


std::shared_ptr<GfxVulkanWsiIface> GfxVulkanWsi::initBackend(
  const Wsi&                          wsi,
  const GfxVulkanProcs&               vk) {
  if (!wsi)
    return std::make_shared<GfxVulkanNullWsi>();

  switch (wsi->getBackendType()) {
#ifdef ALSEID_WSI_SDL3
    case WsiBackend::eSdl3:
      return std::make_shared<GfxVulkanSdl3Wsi>(wsi, vk);
#endif

#ifdef ALSEID_WSI_SDL2
    case WsiBackend::eSdl2:
      return std::make_shared<GfxVulkanSdl2Wsi>(wsi, vk);
#endif

    default:
      throw Error("No compatible WSI bridge found!");
  }
}

}