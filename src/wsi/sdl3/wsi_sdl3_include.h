#pragma once

#ifdef ALSEID_GFX_VULKAN
#include "../../gfx/vulkan/gfx_vulkan_include.h"
#endif

#include <SDL3/SDL.h>

#ifdef ALSEID_GFX_VULKAN
#include <SDL3/SDL_vulkan.h>
#endif

#include "../../util/util_error.h"

namespace as {

class Sdl3Error : public Error {

public:

  Sdl3Error(const char* msg) noexcept {
    std::snprintf(m_message, sizeof(m_message), "%s: %s", msg, SDL_GetError());
  }

};

}
