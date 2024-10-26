#pragma once

#include "../wsi.h"
#include "../wsi_window.h"

#include "wsi_sdl3_include.h"

namespace as {

class WsiSdl3;

/**
 * \brief SDL3 window
 */
class WsiSdl3Window : public WsiWindowIface {

public:

  /**
   * \brief Creates SDL2 window
   *
   * \param [in] wsi WSI instance reference
   * \param [in] desc Window description
   */
  WsiSdl3Window(
          std::shared_ptr<WsiSdl3>      wsi,
    const WsiWindowDesc&                desc);

  /**
   * \brief Destroys SDL2 window
   *
   * Destroys the window and unregisters it
   * from the WSI instance as necessary.
   */
  ~WsiSdl3Window();

  /**
   * \brief Queries SDL2 window handle
   * \returns SDL2 window handle
   */
  SDL_Window* getWindowHandle() const {
    return m_window;
  }

  /**
   * \brief Queries SDL2 window ID
   * \returns SDL2 window ID
   */
  uint32_t getWindowId() const {
    return m_windowId;
  }

  /**
   * \brief Checks whether a given graphics backend is supported
   *
   * \param [in] surfaceType Graphics backend type
   * \returns \c true if surfaces for the given backend can be used
   */
  bool supportsSurfaceType(
          GfxBackend                    surfaceType) override;

  /**
   * \brief Queries current window properties
   * \returns Window properties
   */
  WsiWindowProperties getCurrentProperties() override;

  /**
   * \brief Resizes the window
   *
   * \param [in] extent New window extent
   * \returns \c true on success, \c false on error
   */
  bool resize(
    const Extent2D&                     extent) override;

  /**
   * \brief Changes window to windowed mode
   * \returns \c true on success, \c false on error
   */
  bool setWindowed() override;

  /**
   * \brief Changes window to borderless mode
   *
   * \param [in] display The target display
   * \returns \c true on success, \c false on error
   */
  bool setBorderless(
    const WsiDisplay&                   display) override;

  /**
   * \brief Changes window to fullscreen mode
   *
   * \param [in] display The target display
   * \param [in] displayMode Display mode to use
   * \returns \c true on success, \c false on error
   */
  bool setFullscreen(
    const WsiDisplay&                   display,
    const WsiDisplayMode&               displayMode) override;

  /**
   * \brief Sets mouse mode for the given window
   * \param [in] mouseMode Mouse mode
   */
  void setMouseMode(
          WsiMouseMode                  mouseMode) override;

  /**
   * \brief Sets keyboard mode for the given window
   * \param [in] keyboardMode Keyboard mode
   */
  void setKeyboardMode(
          WsiKeyboardMode               keyboardMode) override;

  /**
   * \brief Changes window title
   * \param [in] title Window title
   */
  void setTitle(
    const std::string&                  title) override;

  /**
   * \brief Sets focus state
   */
  void setFocus(bool hasFocus) {
    m_hasFocus = hasFocus;
  }

  /**
   * \brief Queries mouse mode
   * \returns Mouse mode
   */
  WsiMouseMode getMouseMode() {
    return m_mouseMode;
  }

  /**
   * \brief Queries keyboard mode
   * \returns Keyboard mode
   */
  WsiKeyboardMode getKeyboardMode() {
    return m_keyboardMode;
  }

private:

  std::shared_ptr<WsiSdl3> m_wsi;
  GfxBackend               m_surfaceType;

  SDL_Window*     m_window    = nullptr;
  uint32_t        m_windowId  = 0u;
  bool            m_hasFocus  = false;

  WsiMouseMode    m_mouseMode     = WsiMouseMode::eAbsolute;
  WsiKeyboardMode m_keyboardMode  = WsiKeyboardMode::eRaw;

};

}
