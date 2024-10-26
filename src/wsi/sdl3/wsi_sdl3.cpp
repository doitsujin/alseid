#include "../../util/util_log.h"
#include "../../util/util_math.h"
#include "../../util/util_string.h"

#include "wsi_sdl3.h"
#include "wsi_sdl3_display.h"

namespace as {

constexpr SDL_InitFlags WsiSdl3Subsystems = SDL_INIT_VIDEO |
  SDL_INIT_EVENTS | SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD;

WsiSdl3::WsiSdl3() {
  Log::info("Initializing SDL3 WSI");

  if (!SDL_InitSubSystem(WsiSdl3Subsystems))
    throw Sdl3Error("SDL_InitSubSystem failed");

#ifdef ALSEID_GFX_VULKAN
  m_hasVulkan = SDL_Vulkan_LoadLibrary(nullptr);
#endif
}


WsiSdl3::~WsiSdl3() {
  Log::info("Shutting down SDL3 WSI");

#ifdef ALSEID_GFX_VULKAN
  if (m_hasVulkan)
    SDL_Vulkan_UnloadLibrary();
#endif

  SDL_QuitSubSystem(WsiSdl3Subsystems);
}


WsiBackend WsiSdl3::getBackendType() const {
  return WsiBackend::eSdl3;
}


bool WsiSdl3::checkGfxBackendCompatibility(
        GfxBackend                    backend) {
  if (backend == GfxBackend::eVulkan)
    return m_hasVulkan;

  return false;
}


void WsiSdl3::processEvents(
  const WsiEventProc&                 proc) {
  std::array<SDL_Event, 64> events;
  int32_t count = 0;

  SDL_PumpEvents();

  do {
    count = SDL_PeepEvents(events.data(), events.size(),
      SDL_GETEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST);

    for (int32_t i = 0; i < count; i++) {
      WsiEvent wsiEvent = { };

      if (convertEvent(events[i], wsiEvent))
        proc(wsiEvent);

      // For extended text events we may need to free the
      // buffer after the event callback is done using it
      if (events[i].type == SDL_EVENT_TEXT_EDITING)
        SDL_free(const_cast<char*>(events[i].edit.text));
    }
  } while (count > 0);

  if (count < 0)
    throw Sdl3Error("SDL_PeepEvents failed:");
}


WsiDisplay WsiSdl3::enumDisplays(
        uint32_t                      index) {
  int count = 0;

  SDL_DisplayID* displayIds = SDL_GetDisplays(&count);
  SDL_DisplayID displayId = 0;

  if (int(index) < count)
    displayId = displayIds[index];

  SDL_free(displayIds);

  if (!displayId)
    return WsiDisplay();

  auto display = std::make_shared<WsiSdl3Display>(
    shared_from_this(), displayId);
  return WsiDisplay(std::move(display));
}


WsiWindow WsiSdl3::createWindow(
  const WsiWindowDesc&                desc) {
  auto window = std::make_shared<WsiSdl3Window>(shared_from_this(), desc);

  // Store a weak reference so that we can look up the
  // actual window object during event processing
  std::lock_guard lock(m_windowLutMutex);
  m_windowLut.emplace(std::piecewise_construct,
    std::tuple(window->getWindowId()),
    std::tuple(window));

  return WsiWindow(std::move(window));
}


std::string WsiSdl3::getKeyName(
        WsiScancode                   scancode) {
  SDL_Scancode sdl = SDL_Scancode(scancode);
  SDL_Keycode key = SDL_GetKeyFromScancode(sdl, 0, false);

  const char* name = SDL_GetKeyName(key);

  if (!name || !name[0])
    return strcat('#', uint32_t(scancode));

  return std::string(name);
}


std::string WsiSdl3::getMouseButtonName(
        WsiMouseButton                button) {
  return strcat('M', tzcnt(uint32_t(button)));
}


void WsiSdl3::showMessage(
        LogSeverity                   severity,
  const std::string&                  title,
  const std::string&                  message) {
  Log::message(severity, strcat(title, "\n", message));

  Uint32 flags = 0;

  switch (severity) {
    case LogSeverity::eError:
      flags |= SDL_MESSAGEBOX_ERROR;
      break;

    case LogSeverity::eWarn:
      flags |= SDL_MESSAGEBOX_WARNING;
      break;

    default:
      flags |= SDL_MESSAGEBOX_INFORMATION;
  }

  SDL_ShowSimpleMessageBox(flags, title.c_str(), message.c_str(), nullptr);
}


void WsiSdl3::unregisterWindow(
        WsiSdl3Window*                window) {
  std::lock_guard lock(m_windowLutMutex);
  m_windowLut.erase(window->getWindowId());
}


bool WsiSdl3::convertEvent(
        SDL_Event&                    sdlEvent,
        WsiEvent&                     wsiEvent) {
  switch (sdlEvent.type) {
    case SDL_EVENT_QUIT: {
      wsiEvent.type = WsiEventType::eQuitApp;
    } return true;

    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
      wsiEvent.type = WsiEventType::eKeyPress;
      wsiEvent.window = windowFromSdl(sdlEvent.key.windowID);
      wsiEvent.info.key.scancode = scancodeFromSdl(sdlEvent.key.scancode);
      wsiEvent.info.key.modifiers = modifierKeysFromSdl(sdlEvent.key.mod);
      wsiEvent.info.key.pressed = sdlEvent.key.down;
      wsiEvent.info.key.repeat = sdlEvent.key.repeat;
    } return wsiEvent.window;

    case SDL_EVENT_TEXT_INPUT: {
      wsiEvent.type = WsiEventType::eTextInput;
      wsiEvent.window = windowFromSdl(sdlEvent.text.windowID);
      wsiEvent.info.text.text = sdlEvent.text.text;
      wsiEvent.info.text.editCursor = 0;
      wsiEvent.info.text.editLength = 0;
    } return wsiEvent.window;

    case SDL_EVENT_TEXT_EDITING: {
      wsiEvent.type = WsiEventType::eTextEdit;
      wsiEvent.window = windowFromSdl(sdlEvent.edit.windowID);
      wsiEvent.info.text.text = sdlEvent.edit.text;
      wsiEvent.info.text.editCursor = sdlEvent.edit.start;
      wsiEvent.info.text.editLength = sdlEvent.edit.length;
    } return wsiEvent.window;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
      wsiEvent.type = WsiEventType::eMouseButton;
      wsiEvent.window = windowFromSdl(sdlEvent.button.windowID);
      wsiEvent.info.mouseButton.button = mouseButtonFromSdl(sdlEvent.button.button);
      wsiEvent.info.mouseButton.location = Offset2D(sdlEvent.button.x, sdlEvent.button.y);
      wsiEvent.info.mouseButton.pressed = sdlEvent.button.down;
    } return wsiEvent.window;

    case SDL_EVENT_MOUSE_WHEEL: {
      wsiEvent.type = WsiEventType::eMouseWheel;
      wsiEvent.window = windowFromSdl(sdlEvent.wheel.windowID);
      wsiEvent.info.mouseWheel.delta = Offset2D(sdlEvent.wheel.x, sdlEvent.wheel.y);

      if (sdlEvent.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
        wsiEvent.info.mouseWheel.delta = -wsiEvent.info.mouseWheel.delta;
    } return wsiEvent.window;

    case SDL_EVENT_MOUSE_MOTION: {
      auto sdlWindow = findWindow(sdlEvent.motion.windowID);

      wsiEvent.type = WsiEventType::eMouseMove;
      wsiEvent.window = WsiWindow(sdlWindow);

      if (sdlWindow && sdlWindow->getMouseMode() == WsiMouseMode::eAbsolute)
        wsiEvent.info.mouseMove.absolute = Offset2D(sdlEvent.motion.x, sdlEvent.motion.y);

      wsiEvent.info.mouseMove.relative = Offset2D(sdlEvent.motion.xrel, sdlEvent.motion.yrel);
      wsiEvent.info.mouseMove.buttons = wsiMouseButtonsFromSdl(sdlEvent.motion.state);
    } return wsiEvent.window;

    case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
      wsiEvent.type = WsiEventType::eWindowClose;
      wsiEvent.window = WsiWindow(findWindow(sdlEvent.window.windowID));
    } return wsiEvent.window;

    case SDL_EVENT_WINDOW_FOCUS_GAINED: {
      wsiEvent.type = WsiEventType::eWindowFocus;
      wsiEvent.info.windowFocus.hasFocus = true;
      wsiEvent.window = WsiWindow(findWindow(sdlEvent.window.windowID));
    } return wsiEvent.window;

    case SDL_EVENT_WINDOW_FOCUS_LOST: {
      wsiEvent.type = WsiEventType::eWindowFocus;
      wsiEvent.info.windowFocus.hasFocus = false;
      wsiEvent.window = WsiWindow(findWindow(sdlEvent.window.windowID));
    } return wsiEvent.window;

    case SDL_EVENT_WINDOW_MINIMIZED: {
      wsiEvent.type = WsiEventType::eWindowState;
      wsiEvent.info.windowState.isMinimized = true;
      wsiEvent.window = WsiWindow(findWindow(sdlEvent.window.windowID));
    } return wsiEvent.window;

    case SDL_EVENT_WINDOW_RESTORED: {
      wsiEvent.type = WsiEventType::eWindowState;
      wsiEvent.info.windowState.isMinimized = false;
      wsiEvent.window = WsiWindow(findWindow(sdlEvent.window.windowID));
    } return wsiEvent.window;

    case SDL_EVENT_WINDOW_RESIZED: {
      wsiEvent.type = WsiEventType::eWindowResize;
      wsiEvent.info.windowResize.extent = Extent2D(
        uint32_t(sdlEvent.window.data1),
        uint32_t(sdlEvent.window.data2));
      wsiEvent.window = WsiWindow(findWindow(sdlEvent.window.windowID));
    } return wsiEvent.window;

    default:
      return false;
  }
}


std::shared_ptr<WsiSdl3Window> WsiSdl3::findWindow(
        uint32_t                      windowId) {
  std::lock_guard lock(m_windowLutMutex);

  auto entry = m_windowLut.find(windowId);
  if (entry == m_windowLut.end())
    return nullptr;

  return entry->second.lock();
}


WsiWindow WsiSdl3::windowFromSdl(
        uint32_t                      windowId) {
  return WsiWindow(findWindow(windowId));
}


WsiScancode WsiSdl3::scancodeFromSdl(
        SDL_Scancode                  sdlScancode) {
  // Both SDL and our WSI enums use actual scancodes, so
  // there is no need to do additional translation.
  return WsiScancode(sdlScancode);
}


WsiModifierKeys WsiSdl3::modifierKeysFromSdl(
        uint16_t                      sdlModifiers) {
  WsiModifierKeys result = 0;

  if (sdlModifiers & SDL_KMOD_SHIFT)
    result |= WsiModifierKey::eShift;

  if (sdlModifiers & SDL_KMOD_CTRL)
    result |= WsiModifierKey::eCtrl;

  if (sdlModifiers & SDL_KMOD_ALT)
    result |= WsiModifierKey::eAlt;

  return result;
}


WsiMouseButton WsiSdl3::mouseButtonFromSdl(
        uint8_t                       sdlButton) {
  // SDL mouse button bit masks match our mouse button enum
  return WsiMouseButton(SDL_BUTTON_MASK(sdlButton));
}


WsiMouseButtons WsiSdl3::wsiMouseButtonsFromSdl(
        uint32_t                      sdlButtons) {
  // SDL mouse button bit masks match our mouse button enum
  return WsiMouseButtons(sdlButtons);
}

}
