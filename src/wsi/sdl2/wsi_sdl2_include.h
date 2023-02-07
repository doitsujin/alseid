#pragma once

#ifdef ALSEID_GFX_VULKAN
#include "../../gfx/vulkan/gfx_vulkan_include.h"
#endif

#include <SDL2/SDL.h>

#ifdef ALSEID_GFX_VULKAN
#include <SDL2/SDL_vulkan.h>
#endif

#include "../../util/util_error.h"

namespace as {

class SdlError : public Error {

public:

  SdlError(const char* msg) noexcept {
    std::snprintf(m_message, sizeof(m_message), "%s: %s", msg, SDL_GetError());
  }

};

}
