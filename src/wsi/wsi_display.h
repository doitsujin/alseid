#pragma once

#include "../util/util_iface.h"
#include "../util/util_types.h"

#include <optional>

namespace as {

/**
 * \brief Display mode
 */
struct WsiDisplayMode {
  /** Display resolution, in pixels */
  Extent2D resolution;
  /** Refresh rate, in 1/1000 Hz */
  uint32_t refreshRate;
};


/**
 * \brief Display interface
 */
class WsiDisplayIface {

public:

  virtual ~WsiDisplayIface() { }

  /**
   * \brief Queries human-readable display name
   * \returns Display name
   */
  virtual std::string getName() = 0;

  /**
   * \brief Queries desktop area of the display
   *
   * The desktop area is the rectangle, in desktop
   * coordinates, that is covered by this display.
   * \returns Desktop area of the display
   */
  virtual Rect2D getDesktopArea() = 0;

  /**
   * \brief Queries current display mode
   *
   * The current mode may change when setting a
   * window to fullscreen mode on this display.
   * \returns Current display mdoe
   */
  virtual WsiDisplayMode getCurrentDisplayMode() = 0;

  /**
   * \brief Queries default display mode
   *
   * The display mode used on the desktop.
   * \returns Default display mdoe
   */
  virtual WsiDisplayMode getDefaultDisplayMode() = 0;

  /**
   * \brief Enumerates available display modes
   *
   * Display modes are ordered by width, height
   * and refresh rate, in descending order.
   * \param [in] index Display mode index
   * \returns The display mode, or \c nullopt
   *    if \c index is out of bounds.
   */
  virtual std::optional<WsiDisplayMode> enumDisplayModes(
          uint32_t                      index) = 0;

};


using WsiDisplay = IfaceRef<WsiDisplayIface>;

}
