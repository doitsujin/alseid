#include "wsi_sdl2.h"
#include "wsi_sdl2_display.h"
#include "wsi_sdl2_window.h"

namespace as {

WsiSdl2Window::WsiSdl2Window(
        std::shared_ptr<WsiSdl2>      wsi,
  const WsiWindowDesc&                desc)
: m_wsi         (std::move(wsi))
, m_surfaceType (desc.surfaceType) {
  const char* title = "SDL2 Window";

  if (!desc.title.empty())
    title = desc.title.c_str();

  uint32_t windowFlags =
    SDL_WINDOW_RESIZABLE |
    SDL_WINDOW_ALLOW_HIGHDPI;

  if (m_surfaceType == GfxBackend::eVulkan)
    windowFlags |= SDL_WINDOW_VULKAN;

  m_window = SDL_CreateWindow(title,
    SDL_WINDOWPOS_CENTERED,
    SDL_WINDOWPOS_CENTERED,
    desc.extent.at<0>(),
    desc.extent.at<1>(),
    windowFlags);

  if (!m_window)
    throw SdlError("SDL_CreateWindow");

  m_windowId = SDL_GetWindowID(m_window);

  if (!m_windowId) {
    SDL_DestroyWindow(m_window);
    throw SdlError("SDL_GetWindowID");
  }
}


WsiSdl2Window::~WsiSdl2Window() {
  m_wsi->unregisterWindow(this);

  SDL_DestroyWindow(m_window);
}


bool WsiSdl2Window::supportsSurfaceType(
        GfxBackend                    surfaceType) {
  return m_surfaceType == surfaceType;          
}


WsiWindowProperties WsiSdl2Window::getCurrentProperties() {
  WsiWindowProperties properties;

  // Determine current window mode
  uint32_t flags = SDL_GetWindowFlags(m_window);

  if (flags & SDL_WINDOW_FULLSCREEN)
    properties.mode = WsiWindowMode::eFullscreen;
  else if (flags & SDL_WINDOW_FULLSCREEN_DESKTOP)
    properties.mode = WsiWindowMode::eBorderless;
  else
    properties.mode = WsiWindowMode::eWindowed;

  // Determine current window extent
  int w = 0;
  int h = 0;

  SDL_GetWindowSize(m_window, &w, &h);
  properties.extent = Extent2D(uint32_t(w), uint32_t(h));
  return properties;
}


bool WsiSdl2Window::resize(
  const Extent2D&                     extent) {
  uint32_t flags = SDL_GetWindowFlags(m_window);

  if (flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP))
    return false;

  SDL_SetWindowSize(m_window, extent.at<0>(), extent.at<1>());
  return true;
}


bool WsiSdl2Window::setWindowed() {
  return !SDL_SetWindowFullscreen(m_window, 0);
}


bool WsiSdl2Window::setBorderless(
  const WsiDisplay&                   display) {
  if (SDL_SetWindowFullscreen(m_window, 0))
    return false;

  // Move window to the given display
  if (display) {
    auto& dpy = static_cast<WsiSdl2Display&>(*display);
    SDL_Rect area = { };

    if (SDL_GetDisplayBounds(dpy.getIndex(), &area))
      return false;

    SDL_SetWindowPosition(m_window, area.x, area.y);
  }

  return !SDL_SetWindowFullscreen(m_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
}


bool WsiSdl2Window::setFullscreen(
  const WsiDisplay&                   display,
  const WsiDisplayMode&               displayMode) {
  if (SDL_SetWindowFullscreen(m_window, 0))
    return false;

  // Move window to the given display
  auto& dpy = static_cast<WsiSdl2Display&>(*display);
  SDL_Rect area = { };

  if (SDL_GetDisplayBounds(dpy.getIndex(), &area))
    return false;

  SDL_SetWindowPosition(m_window, area.x, area.y);

  // Ensure that we always set up a valid display mode
  SDL_DisplayMode desiredMode = displayModeToSdl(displayMode);
  SDL_DisplayMode closestMode = { };

  if (!SDL_GetClosestDisplayMode(dpy.getIndex(), &desiredMode, &closestMode))
    return false;

  // Set up the display mode and transition the window
  if (SDL_SetWindowDisplayMode(m_window, &closestMode))
    return false;

  return !SDL_SetWindowFullscreen(m_window, SDL_WINDOW_FULLSCREEN);
}


void WsiSdl2Window::setMouseMode(
        WsiMouseMode                  mouseMode) {
  if (m_mouseMode != mouseMode) {
    m_mouseMode = mouseMode;

    if (m_hasFocus)
      m_wsi->setMouseMode(m_mouseMode);
  }
}


void WsiSdl2Window::setKeyboardMode(
        WsiKeyboardMode               keyboardMode) {
  if (m_keyboardMode != keyboardMode) {
    m_keyboardMode = keyboardMode;

    if (m_hasFocus)
      m_wsi->setKeyboardMode(keyboardMode);
  }            
}


void WsiSdl2Window::setTitle(
  const std::string&                  title) {
  const char* str = "SDL2 Window";

  if (!title.empty())
    str = title.c_str();

  SDL_SetWindowTitle(m_window, str);
}

}
