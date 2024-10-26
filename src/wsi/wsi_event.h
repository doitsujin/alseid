#pragma once

#include <functional>

#include "../util/util_flags.h"
#include "../util/util_types.h"

#include "wsi_window.h"

namespace as {

/**
 * \brief Event type
 */
enum class WsiEventType : uint32_t {
  eQuitApp,       ///< Requested to quit the application
  eWindowClose,   ///< A given window is requested to be closed by the user
  eWindowResize,  ///< A given window was resized
  eWindowState,   ///< A given window was minimized or restored
  eWindowFocus,   ///< A given window gained or lost focus
  eMouseButton,   ///< A mouse button was pressed or released on a given window
  eMouseWheel,    ///< The mouse wheel was used on a given window
  eMouseMove,     ///< The mouse was moved over a given window
  eTextEdit,      ///< Text input was performed but not yet committed
  eTextInput,     ///< Text input was committed
  eKeyPress,      ///< A keyboard key was pressed or released
};

/**
 * \brief Mouse button
 *
 * Non-exhaustive list of mouse buttons. The main purpose of providing
 * these values is to facilitate UI navigation and to provide a useful
 * default for input handling.
 *
 * Input events may return bits that are not defined by this enum.
 */
enum class WsiMouseButton : uint32_t {
  eLeft               = (1u << 0),
  eMiddle             = (1u << 1),
  eRight              = (1u << 2),
  eExtra1             = (1u << 3),
  eExtra2             = (1u << 4),
  eFlagEnum           = 0
};

using WsiMouseButtons = Flags<WsiMouseButton>;

/**
 * \brief Keyboard scancode
 *
 * Non-exhaustive list of scancodes. The main purpose of this is to
 * allow applications to pre-define useful keyboard layouts and to
 * provide access to useful keys such as escape/return and arrow keys.
 *
 * Input events may return scancodes that are not defined by this enum.
 */
enum class WsiScancode : uint32_t {
  eUnknown                = 0,

  eA                      = 4,
  eB                      = 5,
  eC                      = 6,
  eD                      = 7,
  eE                      = 8,
  eF                      = 9,
  eG                      = 10,
  eH                      = 11,
  eI                      = 12,
  eJ                      = 13,
  eK                      = 14,
  eL                      = 15,
  eM                      = 16,
  eN                      = 17,
  eO                      = 18,
  eP                      = 19,
  eQ                      = 20,
  eR                      = 21,
  eS                      = 22,
  eT                      = 23,
  eU                      = 24,
  eV                      = 25,
  eW                      = 26,
  eX                      = 27,
  eY                      = 28,
  eZ                      = 29,

  e1                      = 30,
  e2                      = 31,
  e3                      = 32,
  e4                      = 33,
  e5                      = 34,
  e6                      = 35,
  e7                      = 36,
  e8                      = 37,
  e9                      = 38,
  e0                      = 39,

  eReturn                 = 40,
  eEscape                 = 41,
  eBackspace              = 42,
  eTab                    = 43,
  eSpace                  = 44,

  eComma                  = 54,
  ePeriod                 = 55,

  eF1                     = 58,
  eF2                     = 59,
  eF3                     = 60,
  eF4                     = 61,
  eF5                     = 62,
  eF6                     = 63,
  eF7                     = 64,
  eF8                     = 65,
  eF9                     = 66,
  eF10                    = 67,
  eF11                    = 68,
  eF12                    = 69,

  eInsert                 = 73,
  eHome                   = 74,
  ePageUp                 = 75,
  eDelete                 = 76,
  eEnd                    = 77,
  ePageDown               = 78,
  eRight                  = 79,
  eLeft                   = 80,
  eDown                   = 81,
  eUp                     = 82,

  eKpDivide               = 84,
  eKpMultiply             = 85,
  eKpMins                 = 86,
  eKpPlus                 = 87,
  eKpEnter                = 88,
  eKp1                    = 89,
  eKp2                    = 90,
  eKp3                    = 91,
  eKp4                    = 92,
  eKp5                    = 93,
  eKp6                    = 94,
  eKp7                    = 95,
  eKp8                    = 96,
  eKp9                    = 97,
  eKp0                    = 98,
  eKpPeriod               = 99,
};


/**
 * \brief Keyboard modifier keys
 */
enum class WsiModifierKey : uint32_t {
  eShift              = (1u << 0),
  eCtrl               = (1u << 1),
  eAlt                = (1u << 2),
  eFlagEnum           = 0
};

using WsiModifierKeys = Flags<WsiModifierKey>;


/**
 * \brief Window size event data
 *
 * Stores data for events of the type
 * \c WsiEvent::eWindowResize.
 */
struct WsiWindowResizeEvent {
  /** New window size, in desktop coordinates */
  Extent2D extent;
};


/**
 * \brief Window focus event data
 *
 * Stores data for events of the type
 * \c WsiEvent::eWindowFocus.
 */
struct WsiWindowFocusEvent {
  /** Whether or not the window has focus */
  bool hasFocus;
};


/**
 * \brief Window state event data
 *
 * Stores data for events of the type
 * \c WsiEvent::eWindowState.
 */
struct WsiWindowStateEvent {
  /** Whether or not the window is minimized */
  bool isMinimized;
};


/**
 * \brief Mouse button event data
 *
 * Stores data for events of the type
 * \c WsiEvent::eMouseButton.
 */
struct WsiMouseButtonEvent {
  /** Mouse button that was pressed or released */
  WsiMouseButton button;
  /** Location where the button was pressed relative to the
   *  top-left corner of the window, in desktop coordinates. */
  Offset2D location;
  /** Mouse button state */
  bool pressed;
};


/**
 * \brief Mouse wheel event data
 */
struct WsiMouseWheelEvent {
  /** Mouse wheel movement in two dimensions */
  Offset2D delta;
};


/**
 * \brief Mouse motion event data
 *
 * Stores data for events of the type
 * \c WsiEvent::eMouseMove.
 */
struct WsiMouseMoveEvent {
  /** Cursor position, relative to the top-left corner
   *  of the window, in desktop coordinates. */
  Offset2D absolute;
  /** Relative mouse movement, in desktop coordinates */
  Offset2D relative;
  /** Mouse button state */
  WsiMouseButtons buttons;
};


/**
 * \brief Keyboard event data
 *
 * Stores data for events of the type
 * \c WsiEvent::eKeyPress.
 */
struct WsiKeyEvent {
  /** Key scancode */
  WsiScancode scancode;
  /** Modifier keys that were pressed at the time */
  WsiModifierKeys modifiers;
  /** Key state. Note that if \c repeat is \c true,
   *  the key may not have been released previously. */
  bool pressed;
  /** Whether the key press is repeated */
  bool repeat;
};


/**
 * \brief Text input event
 */
struct WsiTextEvent {
  /** Text being edited or committed. This memory is
   *  allocated internally and must not be freed. */
  const char* text;
  /** Start of editing section */
  int32_t editCursor;
  /** Length of editing section */
  int32_t editLength;
};


/**
 * \brief WSI event
 *
 * Stores the event type data.
 */
struct WsiEvent {
  WsiEventType type;
  WsiWindow window;
  union {
    WsiWindowResizeEvent  windowResize;
    WsiWindowFocusEvent   windowFocus;
    WsiWindowStateEvent   windowState;
    WsiMouseButtonEvent   mouseButton;
    WsiMouseWheelEvent    mouseWheel;
    WsiMouseMoveEvent     mouseMove;
    WsiTextEvent          text;
    WsiKeyEvent           key;
  } info;
};


/**
 * \brief Event processing callback
 */
using WsiEventProc = std::function<void (const WsiEvent&)>;

}
