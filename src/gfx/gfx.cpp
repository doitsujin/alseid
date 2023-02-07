#include <vector>

#ifdef ALSEID_GFX_VULKAN
#include "./vulkan/gfx_vulkan.h"
#endif

#include "../util/util_array.h"
#include "../util/util_error.h"
#include "../util/util_log.h"

#include "gfx.h"

namespace as {

const GfxFormatMetadataMap g_formatInfo;


Gfx::Gfx(
        GfxBackend                    backend,
  const Wsi&                          wsi,
        GfxInstanceFlags              flags)
: IfaceRef<GfxIface>(initBackend(backend, wsi, flags)) {

}


std::shared_ptr<GfxIface> Gfx::initBackend(
        GfxBackend                    backend,
  const Wsi&                          wsi,
        GfxInstanceFlags              flags) {
  const auto backends = make_array<GfxBackend>(
    backend,
    GfxBackend::eVulkan);

  for (size_t i = 0; i < backends.size(); i++) {    
    if ((backends[i] == GfxBackend::eDefault)
     || (backends[i] == backends[0] && i))
      continue;

    if (wsi && !wsi->checkGfxBackendCompatibility(backends[i]))
      continue;

    try {
      switch (backends[i]) {
#ifdef ALSEID_GFX_VULKAN
        case GfxBackend::eVulkan:
          return std::make_shared<GfxVulkan>(wsi, flags);
#endif

        default:
          continue;
      }
    } catch (const Error& e) {
      Log::err(e.what());
    }
  }

  throw Error("Failed to initialize graphics system");
}


const GfxFormatInfo& Gfx::getFormatInfo(
        GfxFormat                     format) {
  return g_formatInfo.get(format);
}

}
