#pragma once

#include <memory>
#include <string>

#include "../gfx/gfx_backend.h"

#include "wsi_display.h"

namespace as {

class Wsi;

/**
 * \brief Window mode
 */
enum class WsiWindowMode : uint32_t {
  /** Windowed mode */
  eWindowed         = 0,
  /** Borderless window. This will cover the entire
   *  target display, but not perform a mode switch. */
  eBorderless       = 1,
  /** Fullscreen mode. Will perform a mode switch. */
  eFullscreen       = 2,
};


/**
 * \brief Mouse mode
 */
enum class WsiMouseMode : uint32_t {
  /** Absolute mode, cursor is shown */
  eAbsolute         = 0,
  /** Relative mmode, cursor is hidden */
  eRelative         = 1,
};


/**
 * \brief Keyboard mode
 */
enum class WsiKeyboardMode : uint32_t {
  /** Send raw key press events */
  eRaw              = 0,
  /** Send text input events */
  eText             = 1,
};


/**
 * \brief Window description
 *
 * Stores parameters for window creation. Newly created windows
 * will be windowed on desktop platforms, but may be fullscreen
 * on other platforms.
 */
struct WsiWindowDesc {
  /** Window title to display on the window decoration. */
  std::string title;
  /** Window extent, in desktop coordinates. Note that this is
   *  not necessarily identical to the drawable surface size.
   *  Ignored on platforms that only support fullscreen windows. */
  Extent2D extent = Extent2D(1280, 720);
  /** Graphics backend to make the window compatible with. If this
   *  is set to default, no graphics backend will be compatible. */
  GfxBackend surfaceType = GfxBackend::eDefault;
};


/**
 * \brief Window description
 */
struct WsiWindowProperties {
  /** Current window mode */
  WsiWindowMode mode = WsiWindowMode::eWindowed;
  /** Current window extent, in desktop coordinates. Note that
   *  is not necessarily identical to the drawable surface size.*/
  Extent2D extent = Extent2D(0, 0);
};


/**
 * \brief Window interface
 */
class WsiWindowIface {

public:

  virtual ~WsiWindowIface() { }

  /**
   * \brief Checks whether a given graphics backend is supported
   *
   * Only returns \c true for the backend that the window was created for.
   * \param [in] surfaceType Graphics backend type
   * \returns \c true if surfaces for the given backend can be used
   */
  virtual bool supportsSurfaceType(
          GfxBackend                    surfaceType) = 0;

  /**
   * \brief Queries current window properties
   *
   * Checks the current window mode and size.
   * \returns Current window properties
   */
  virtual WsiWindowProperties getCurrentProperties() = 0;

  /**
   * \brief Resizes the window
   *
   * Only has an effect in windowed mode.
   * \param [in] extent New window extent
   * \returns \c true on success, \c false on error
   */
  virtual bool resize(
    const Extent2D&                     extent) = 0;

  /**
   * \brief Changes window to windowed mode
   * \returns \c true on success, \c false on error
   */
  virtual bool setWindowed() = 0;

  /**
   * \brief Changes window to borderless mode
   *
   * \param [in] display Optionally specifies
   *    the display to maximize the window on.
   * \returns \c true on success, \c false on error
   */
  virtual bool setBorderless(
    const WsiDisplay&                   display) = 0;

  /**
   * \brief Changes window to fullscreen mode
   *
   * \param [in] display Specifies the display to use.
   *    Unlike in \ref setBorderless, this is not optional.
   * \param [in] displayMode Display mode to use
   * \returns \c true on success, \c false on error
   */
  virtual bool setFullscreen(
    const WsiDisplay&                   display,
    const WsiDisplayMode&               displayMode) = 0;

  /**
   * \brief Sets mouse mode for the given window
   *
   * The mouse mode will be applied when the window gains focus.
   * \param [in] mouseMode Mouse mode
   */
  virtual void setMouseMode(
          WsiMouseMode                  mouseMode) = 0;

  /**
   * \brief Sets keyboard mode for the given window
   *
   * The keyboard mode will be applied when the window gains focus.
   * \param [in] keyboardMode Keyboard mode
   */
  virtual void setKeyboardMode(
          WsiKeyboardMode               keyboardMode) = 0;

  /**
   * \brief Changes window title
   * \param [in] title Window title
   */
  virtual void setTitle(
    const std::string&                  title) = 0;

};


using WsiWindow = IfaceRef<WsiWindowIface>;

}
