#pragma once

#include <memory>
#include <mutex>
#include <unordered_map>

#include "wsi_sdl3_window.h"

#include "../wsi.h"

namespace as {

/**
 * \brief SDL3 WSI back-end
 */
class WsiSdl3 : public WsiIface
, public std::enable_shared_from_this<WsiSdl3> {
public:

  WsiSdl3();

  ~WsiSdl3();

  /**
   * \brief Queries backend type
   * \returns Backend type
   */
  WsiBackend getBackendType() const override;

  /**
   * \brief Checks graphics backend compatibility
   *
   * \param [in] backend Graphics backend
   * \returns \c true if the given graphics backend can be used
   */
  bool checkGfxBackendCompatibility(
          GfxBackend                    backend) override;

  /**
   * \brief Enumerates available displays
   *
   * Index 0 is reserved for the primary display.
   * \param [in] index Display index
   * \returns Display object
   */
  WsiDisplay enumDisplays(
          uint32_t                      index) override;

  /**
   * \brief Creates a window
   *
   * \param [in] desc Window description
   * \returns Newly created window
   */
  WsiWindow createWindow(
    const WsiWindowDesc&                desc) override;

  /**
   * \brief Queries key name
   *
   * \param [in] scancode Key scancode
   * \returns Human-readable name of the key
   */
  std::string getKeyName(
          WsiScancode                   scancode) override;

  /**
   * \brief Queries name of a given mouse button
   * 
   * \param [in] button Mouse button
   * \returns Human-readable name of the button
   */
  std::string getMouseButtonName(
          WsiMouseButton                button) override;

  /**
   * \brief Processes queued events
   * \param [in] proc Event callback
   */
  void processEvents(
    const WsiEventProc&                 proc) override;

  /**
   * \brief Shows info message
   *
   * \param [in] severity Message severity
   * \param [in] title Message box title
   * \param [in] message Message
   */
  void showMessage(
          LogSeverity                   severity,
    const std::string&                  title,
    const std::string&                  message) override;

  /**
   * \brief Unregisters window
   *
   * This gets called automatically when
   * a window is being destroyed.
   * \param [in] window The window
   */
  void unregisterWindow(
          WsiSdl3Window*                window);

  /**
   * \brief Sets mouse mode
   * \param [in] mode Mouse mode
   */
  void setMouseMode(
          WsiMouseMode                  mode);

  /**
   * \brief Sets keyboard mode
   * \param [in] mode Keyboard mode
   */
  void setKeyboardMode(
          WsiKeyboardMode               mode);

private:

  bool m_hasVulkan = false;

  std::mutex                      m_windowLutMutex;
  std::unordered_map<uint32_t,
    std::weak_ptr<WsiSdl3Window>> m_windowLut;

  bool convertEvent(
          SDL_Event&                    sdlEvent,
          WsiEvent&                     wsiEvent);

  std::shared_ptr<WsiSdl3Window> findWindow(
          uint32_t                      windowId);

  WsiWindow windowFromSdl(
          uint32_t                      windowId);

  static WsiScancode scancodeFromSdl(
          SDL_Scancode                  sdlScancode);

  static WsiModifierKeys modifierKeysFromSdl(
          uint16_t                      sdlModifiers);

  static WsiMouseButton mouseButtonFromSdl(
          uint8_t                       sdlButton);

  static WsiMouseButtons wsiMouseButtonsFromSdl(
          uint32_t                      sdlButtons);

};

}
