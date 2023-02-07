#ifdef ALSEID_WSI_SDL2
#include "./sdl2/wsi_sdl2.h"
#endif

#include "wsi.h"

namespace as {

Wsi::Wsi(WsiBackend backend)
: IfaceRef<WsiIface>(initBackend(backend)) {

}


std::shared_ptr<WsiIface> Wsi::initBackend(
        WsiBackend                    backend) {
  switch (backend) {
    case WsiBackend::eDefault:

#ifdef ALSEID_WSI_SDL2
    case WsiBackend::eSdl2:
      return std::make_shared<WsiSdl2>();
#endif

    default:
      throw Error("Selected WSI backend not supported.");
  }
}

}
