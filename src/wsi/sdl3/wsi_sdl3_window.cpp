#include "wsi_sdl3.h"
#include "wsi_sdl3_display.h"
#include "wsi_sdl3_window.h"

namespace as {

WsiSdl3Window::WsiSdl3Window(
        std::shared_ptr<WsiSdl3>      wsi,
  const WsiWindowDesc&                desc)
: m_wsi         (std::move(wsi))
, m_surfaceType (desc.surfaceType) {
  const char* title = "SDL2 Window";

  if (!desc.title.empty())
    title = desc.title.c_str();

  uint32_t windowFlags =
    SDL_WINDOW_RESIZABLE |
    SDL_WINDOW_HIGH_PIXEL_DENSITY;

  if (m_surfaceType == GfxBackend::eVulkan)
    windowFlags |= SDL_WINDOW_VULKAN;

  m_window = SDL_CreateWindow(title,
    desc.extent.at<0>(),
    desc.extent.at<1>(),
    windowFlags);

  if (!m_window)
    throw Sdl3Error("SDL_CreateWindow");

  m_windowId = SDL_GetWindowID(m_window);

  if (!m_windowId) {
    SDL_DestroyWindow(m_window);
    throw Sdl3Error("SDL_GetWindowID");
  }
}


WsiSdl3Window::~WsiSdl3Window() {
  m_wsi->unregisterWindow(this);

  SDL_DestroyWindow(m_window);
}


bool WsiSdl3Window::supportsSurfaceType(
        GfxBackend                    surfaceType) {
  return m_surfaceType == surfaceType;          
}


WsiWindowProperties WsiSdl3Window::getCurrentProperties() {
  WsiWindowProperties properties;

  // Determine current window mode
  uint32_t flags = SDL_GetWindowFlags(m_window);

  if (flags & SDL_WINDOW_FULLSCREEN) {
    properties.mode = SDL_GetWindowFullscreenMode(m_window)
      ? WsiWindowMode::eFullscreen
      : WsiWindowMode::eBorderless;
  } else {
    properties.mode = WsiWindowMode::eWindowed;
  }

  // Determine current window extent
  int w = 0;
  int h = 0;

  SDL_GetWindowSize(m_window, &w, &h);
  properties.extent = Extent2D(uint32_t(w), uint32_t(h));
  return properties;
}


bool WsiSdl3Window::resize(
  const Extent2D&                     extent) {
  uint32_t flags = SDL_GetWindowFlags(m_window);

  if (flags & SDL_WINDOW_FULLSCREEN)
    return false;

  SDL_SetWindowSize(m_window, extent.at<0>(), extent.at<1>());
  return true;
}


bool WsiSdl3Window::setWindowed() {
  return SDL_SetWindowFullscreen(m_window, false);
}


bool WsiSdl3Window::setBorderless(
  const WsiDisplay&                   display) {
  if (SDL_SetWindowFullscreen(m_window, 0))
    return false;

  // Move window to the given display
  if (display) {
    auto& dpy = static_cast<WsiSdl3Display&>(*display);
    SDL_Rect area = { };

    if (SDL_GetDisplayBounds(dpy.getId(), &area))
      return false;

    SDL_SetWindowPosition(m_window, area.x, area.y);
  }

  return SDL_SetWindowFullscreenMode(m_window, nullptr)
      && SDL_SetWindowFullscreen(m_window, true);
}


bool WsiSdl3Window::setFullscreen(
  const WsiDisplay&                   display,
  const WsiDisplayMode&               displayMode) {
  if (SDL_SetWindowFullscreen(m_window, 0))
    return false;

  // Move window to the given display
  auto& dpy = static_cast<WsiSdl3Display&>(*display);
  SDL_Rect area = { };

  if (SDL_GetDisplayBounds(dpy.getId(), &area))
    return false;

  SDL_SetWindowPosition(m_window, area.x, area.y);

  // Ensure that we always set up a valid display mode
  SDL_DisplayMode closestMode = { };

  if (!SDL_GetClosestFullscreenDisplayMode(dpy.getId(),
      displayMode.resolution.at<0>(),
      displayMode.resolution.at<1>(),
      float(displayMode.refreshRate) / 1000.0f, true, &closestMode))
    return false;

  // Set up the display mode and transition the window
  return SDL_SetWindowFullscreenMode(m_window, &closestMode)
      && SDL_SetWindowFullscreen(m_window, true);
}


void WsiSdl3Window::setMouseMode(
        WsiMouseMode                  mouseMode) {
  SDL_SetWindowRelativeMouseMode(m_window,
    mouseMode == WsiMouseMode::eRelative);
}


void WsiSdl3Window::setKeyboardMode(
        WsiKeyboardMode               keyboardMode) {
  if (m_keyboardMode == keyboardMode)
    return;

  if (keyboardMode == WsiKeyboardMode::eText)
    SDL_StartTextInput(m_window);
  else
    SDL_StopTextInput(m_window);

  m_keyboardMode = keyboardMode;
}


void WsiSdl3Window::setTitle(
  const std::string&                  title) {
  const char* str = "SDL3 Window";

  if (!title.empty())
    str = title.c_str();

  SDL_SetWindowTitle(m_window, str);
}

}
