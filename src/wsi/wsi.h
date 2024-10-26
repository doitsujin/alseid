#pragma once

#include <memory>

#include "../gfx/gfx_backend.h"

#include "../util/util_iface.h"
#include "../util/util_log.h"

#include "wsi_display.h"
#include "wsi_event.h"
#include "wsi_window.h"

namespace as {

/**
 * \brief WSI back-end type
 */
enum class WsiBackend : uint32_t {
  /** Chooses an available back-end
   *  based on the current platform. */
  eDefault      = 0,
  /** SDL 2 back-end */
  eSdl2         = 1,
  /** SDL 3 back-end */
  eSdl3         = 2,
};


/**
 * \brief WSI interface
 *
 * Provides the public interface for WSI backends.
 */
class WsiIface {

public:

  virtual ~WsiIface() { }

  /**
   * \brief Queries backend type
   * \returns Backend type
   */
  virtual WsiBackend getBackendType() const = 0;

  /**
   * \brief Checks graphics backend compatibility
   *
   * \param [in] backend Graphics backend
   * \returns \c true if the given graphics backend can be used
   */
  virtual bool checkGfxBackendCompatibility(
          GfxBackend                    backend) = 0;

  /**
   * \brief Enumerates available displays
   *
   * Note that this function may always return new objects, so
   * different objects may still represent the same display.
   * \param [in] index Display index
   * \returns Display object. Will be null if
   *    no display with the given index exists.
   */
  virtual WsiDisplay enumDisplays(
          uint32_t                      index) = 0;

  /**
   * \brief Creates a window
   *
   * \param [in] desc Window description
   * \returns Newly created window
   */
  virtual WsiWindow createWindow(
    const WsiWindowDesc&                desc) = 0;

  /**
   * \brief Queries name of a given key
   * 
   * \param [in] scancode Key scancode
   * \returns Human-readable name of the key
   */
  virtual std::string getKeyName(
          WsiScancode                   scancode) = 0;

  /**
   * \brief Queries name of a given mouse button
   * 
   * \param [in] button Mouse button
   * \returns Human-readable name of the button
   */
  virtual std::string getMouseButtonName(
          WsiMouseButton                button) = 0;

  /**
   * \brief Processes queued events
   *
   * Invokes an application-defined callback
   * for each queued event.
   * \param [in] proc Event callback
   */
  virtual void processEvents(
    const WsiEventProc&                 proc) = 0;

  /**
   * \brief Shows info message
   *
   * Can be used to display errors. Will block calling
   * thread until message is confirmed by the user.
   * \param [in] severity Message severity
   * \param [in] title Message box title
   * \param [in] message Message
   */
  virtual void showMessage(
          LogSeverity                   severity,
    const std::string&                  title,
    const std::string&                  message) = 0;

};


/**
 * \brief WSI instance
 *
 * See WsiIface.
 */
class Wsi : public IfaceRef<WsiIface> {

public:

  Wsi() { }
  Wsi(std::nullptr_t) { }

  /**
   * \brief Initializes WSI object with given backend
   *
   * \param [in] backend Backend to initialize
   * \throws Error if no backend could be initialized
   */
  explicit Wsi(WsiBackend backend);

private:

  static std::shared_ptr<WsiIface> initBackend(
          WsiBackend                    backend);

};

}