#include "../../util/util_log.h"
#include "../../util/util_math.h"
#include "../../util/util_string.h"

#include "wsi_sdl2.h"
#include "wsi_sdl2_display.h"

namespace as {

constexpr uint32_t WsiSdl2Subsystems = SDL_INIT_VIDEO |
  SDL_INIT_EVENTS | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER;

WsiSdl2::WsiSdl2() {
  Log::info("Initializing SDL2 WSI");

  if (SDL_InitSubSystem(WsiSdl2Subsystems))
    throw SdlError("SDL_InitSubSystem failed");

#ifdef ALSEID_GFX_VULKAN
  if (!SDL_Vulkan_LoadLibrary(nullptr))
    m_hasVulkan = true;
#endif

  // Initialize display count so we don't query
  // it every time enumDisplays is called
  int displayCount = SDL_GetNumVideoDisplays();

  if (displayCount < 1)
    throw SdlError("Failed to query display count");

  m_displayCount = uint32_t(displayCount);

  SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
  SDL_SetHint(SDL_HINT_IME_SUPPORT_EXTENDED_TEXT, "1");
}


WsiSdl2::~WsiSdl2() {
  Log::info("Shutting down SDL2 WSI");

#ifdef ALSEID_GFX_VULKAN
  if (m_hasVulkan)
    SDL_Vulkan_UnloadLibrary();
#endif

  SDL_QuitSubSystem(WsiSdl2Subsystems);
}


WsiBackend WsiSdl2::getBackendType() const {
  return WsiBackend::eSdl2;
}


bool WsiSdl2::checkGfxBackendCompatibility(
        GfxBackend                    backend) {
  if (backend == GfxBackend::eVulkan)
    return m_hasVulkan;

  return false;
}


void WsiSdl2::processEvents(
  const WsiEventProc&                 proc) {
  std::array<SDL_Event, 64> events;
  int32_t count = 0;

  SDL_PumpEvents();

  do {
    count = SDL_PeepEvents(events.data(), events.size(),
      SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT);

    for (int32_t i = 0; i < count; i++) {
      WsiEvent wsiEvent = { };

      if (convertEvent(events[i], wsiEvent))
        proc(wsiEvent);

      // For extended text events we may need to free the
      // buffer after the event callback is done using it
      if (events[i].type == SDL_TEXTEDITING_EXT)
        SDL_free(events[i].editExt.text);
    }
  } while (count > 0);

  if (count < 0)
    throw SdlError("SDL_PeepEvents failed:");
}


WsiDisplay WsiSdl2::enumDisplays(
        uint32_t                      index) {
  if (index >= m_displayCount)
    return WsiDisplay();

  auto display = std::make_shared<WsiSdl2Display>(
    shared_from_this(), int(index));
  return WsiDisplay(std::move(display));
}


WsiWindow WsiSdl2::createWindow(
  const WsiWindowDesc&                desc) {
  auto window = std::make_shared<WsiSdl2Window>(shared_from_this(), desc);

  // Store a weak reference so that we can look up the
  // actual window object during event processing
  std::lock_guard lock(m_windowLutMutex);
  m_windowLut.emplace(std::piecewise_construct,
    std::tuple(window->getWindowId()),
    std::tuple(window));

  return WsiWindow(std::move(window));
}


std::string WsiSdl2::getKeyName(
        WsiScancode                   scancode) {
  SDL_Scancode sdl = SDL_Scancode(scancode);
  SDL_Keycode key = SDL_GetKeyFromScancode(sdl);

  const char* name = SDL_GetKeyName(key);

  if (!name || !name[0])
    return strcat('#', uint32_t(scancode));

  return std::string(name);
}


std::string WsiSdl2::getMouseButtonName(
        WsiMouseButton                button) {
  return strcat('M', tzcnt(uint32_t(button)));
}


void WsiSdl2::unregisterWindow(
        WsiSdl2Window*                window) {
  std::lock_guard lock(m_windowLutMutex);
  m_windowLut.erase(window->getWindowId());
}


bool WsiSdl2::convertEvent(
        SDL_Event&                    sdlEvent,
        WsiEvent&                     wsiEvent) {
  switch (sdlEvent.type) {
    case SDL_QUIT: {
      wsiEvent.type = WsiEventType::eQuitApp;
    } return true;

    case SDL_KEYDOWN:
    case SDL_KEYUP: {
      wsiEvent.type = WsiEventType::eKeyPress;
      wsiEvent.window = windowFromSdl(sdlEvent.key.windowID);
      wsiEvent.info.key.scancode = scancodeFromSdl(sdlEvent.key.keysym.scancode);
      wsiEvent.info.key.modifiers = modifierKeysFromSdl(sdlEvent.key.keysym.mod);
      wsiEvent.info.key.pressed = sdlEvent.key.state == SDL_PRESSED;
      wsiEvent.info.key.repeat = sdlEvent.key.repeat;
    } return wsiEvent.window;

    case SDL_TEXTINPUT: {
      wsiEvent.type = WsiEventType::eTextInput;
      wsiEvent.window = windowFromSdl(sdlEvent.text.windowID);
      wsiEvent.info.text.text = sdlEvent.text.text;
      wsiEvent.info.text.editCursor = 0;
      wsiEvent.info.text.editLength = 0;
    } return wsiEvent.window;

    case SDL_TEXTEDITING: {
      wsiEvent.type = WsiEventType::eTextEdit;
      wsiEvent.window = windowFromSdl(sdlEvent.edit.windowID);
      wsiEvent.info.text.text = sdlEvent.edit.text;
      wsiEvent.info.text.editCursor = sdlEvent.edit.start;
      wsiEvent.info.text.editLength = sdlEvent.edit.length;
    } return wsiEvent.window;

    case SDL_TEXTEDITING_EXT: {
      wsiEvent.type = WsiEventType::eTextEdit;
      wsiEvent.window = windowFromSdl(sdlEvent.editExt.windowID);
      wsiEvent.info.text.text = sdlEvent.editExt.text;
      wsiEvent.info.text.editCursor = sdlEvent.editExt.start;
      wsiEvent.info.text.editLength = sdlEvent.editExt.length;
    } return wsiEvent.window;

    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP: {
      wsiEvent.type = WsiEventType::eMouseButton;
      wsiEvent.window = windowFromSdl(sdlEvent.button.windowID);
      wsiEvent.info.mouseButton.button = mouseButtonFromSdl(sdlEvent.button.button);
      wsiEvent.info.mouseButton.location = Offset2D(sdlEvent.button.x, sdlEvent.button.y);
      wsiEvent.info.mouseButton.pressed = sdlEvent.button.state == SDL_PRESSED;
    } return wsiEvent.window;

    case SDL_MOUSEWHEEL: {
      wsiEvent.type = WsiEventType::eMouseWheel;
      wsiEvent.window = windowFromSdl(sdlEvent.wheel.windowID);
      wsiEvent.info.mouseWheel.delta = Offset2D(sdlEvent.wheel.x, sdlEvent.wheel.y);

      if (sdlEvent.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
        wsiEvent.info.mouseWheel.delta = -wsiEvent.info.mouseWheel.delta;
    } return wsiEvent.window;

    case SDL_MOUSEMOTION: {
      wsiEvent.type = WsiEventType::eMouseMove;
      wsiEvent.window = windowFromSdl(sdlEvent.motion.windowID);

      if (m_mouseMode == WsiMouseMode::eAbsolute)
        wsiEvent.info.mouseMove.absolute = Offset2D(sdlEvent.motion.x, sdlEvent.motion.y);

      wsiEvent.info.mouseMove.relative = Offset2D(sdlEvent.motion.xrel, sdlEvent.motion.yrel);
      wsiEvent.info.mouseMove.buttons = wsiMouseButtonsFromSdl(sdlEvent.motion.state);
    } return wsiEvent.window;

    case SDL_WINDOWEVENT: {
      auto sdlWindow = findWindow(sdlEvent.window.windowID);
      wsiEvent.window = WsiWindow(sdlWindow);

      if (!wsiEvent.window)
        return false;

      switch (sdlEvent.window.event) {
        case SDL_WINDOWEVENT_CLOSE: {
          wsiEvent.type = WsiEventType::eWindowClose;
        } return true;

        case SDL_WINDOWEVENT_TAKE_FOCUS: {
          // Always take focus when offered
          SDL_SetWindowInputFocus(SDL_GetWindowFromID(sdlEvent.window.windowID));
        } return false;

        case SDL_WINDOWEVENT_FOCUS_GAINED: {
          wsiEvent.type = WsiEventType::eWindowFocus;
          wsiEvent.info.windowFocus.hasFocus = true;

          sdlWindow->setFocus(true);

          setMouseMode(sdlWindow->getMouseMode());
          setKeyboardMode(sdlWindow->getKeyboardMode());

          m_focusWindowId = sdlEvent.window.windowID;
        } return true;

        case SDL_WINDOWEVENT_FOCUS_LOST: {
          wsiEvent.type = WsiEventType::eWindowFocus;
          wsiEvent.info.windowFocus.hasFocus = false;

          if (m_focusWindowId == sdlEvent.window.windowID) {
            // Do not reset keyboard mode here as that may cause issues
            setMouseMode(WsiMouseMode::eAbsolute);
            m_focusWindowId = 0;
          }

          sdlWindow->setFocus(false);
        } return true;

        case SDL_WINDOWEVENT_MINIMIZED: {
          wsiEvent.type = WsiEventType::eWindowState;
          wsiEvent.info.windowState.isMinimized = true;
        } return true;

        case SDL_WINDOWEVENT_RESTORED: {
          wsiEvent.type = WsiEventType::eWindowState;
          wsiEvent.info.windowState.isMinimized = false;
        } return true;

        case SDL_WINDOWEVENT_SIZE_CHANGED: {
          wsiEvent.type = WsiEventType::eWindowResize;
          wsiEvent.info.windowResize.extent = Extent2D(
            uint32_t(sdlEvent.window.data1),
            uint32_t(sdlEvent.window.data2));
        } return true;

        default:
          return false;
      }
    }

    default:
      return false;
  }
}


std::shared_ptr<WsiSdl2Window> WsiSdl2::findWindow(
        uint32_t                      windowId) {
  std::lock_guard lock(m_windowLutMutex);

  auto entry = m_windowLut.find(windowId);
  if (entry == m_windowLut.end())
    return nullptr;

  return entry->second.lock();
}


WsiWindow WsiSdl2::windowFromSdl(
        uint32_t                      windowId) {
  return WsiWindow(findWindow(windowId));
}


WsiScancode WsiSdl2::scancodeFromSdl(
        SDL_Scancode                  sdlScancode) {
  // Both SDL and our WSI enums use actual scancodes, so
  // there is no need to do additional translation.
  return WsiScancode(sdlScancode);
}


WsiModifierKeys WsiSdl2::modifierKeysFromSdl(
        uint16_t                      sdlModifiers) {
  WsiModifierKeys result = 0;

  if (sdlModifiers & KMOD_SHIFT)
    result |= WsiModifierKey::eShift;

  if (sdlModifiers & KMOD_CTRL)
    result |= WsiModifierKey::eCtrl;

  if (sdlModifiers & KMOD_ALT)
    result |= WsiModifierKey::eAlt;

  return result;
}


WsiMouseButton WsiSdl2::mouseButtonFromSdl(
        uint8_t                       sdlButton) {
  // SDL mouse button bit masks match our mouse button enum
  return WsiMouseButton(SDL_BUTTON(sdlButton));
}


WsiMouseButtons WsiSdl2::wsiMouseButtonsFromSdl(
        uint32_t                      sdlButtons) {
  // SDL mouse button bit masks match our mouse button enum
  return WsiMouseButtons(sdlButtons);
}


void WsiSdl2::setMouseMode(
        WsiMouseMode                  mode) {
  if (m_mouseMode != mode) {
    m_mouseMode = mode;
    SDL_SetRelativeMouseMode(mode == WsiMouseMode::eRelative ? SDL_TRUE : SDL_FALSE);
  }
}


void WsiSdl2::setKeyboardMode(
        WsiKeyboardMode               mode) {
  if (m_keyboardMode != mode) {
    m_keyboardMode = mode;

    if (mode == WsiKeyboardMode::eRaw)
      SDL_StopTextInput();
    else
      SDL_StartTextInput();
  }

}

}
