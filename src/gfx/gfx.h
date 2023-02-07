#pragma once

#include "../util/util_flags.h"
#include "../util/util_iface.h"

#include "../wsi/wsi.h"

#include "gfx_adapter.h"
#include "gfx_backend.h"
#include "gfx_context.h"
#include "gfx_device.h"
#include "gfx_format.h"
#include "gfx_types.h"

namespace as {

/**
 * \brief Graphics instance flags
 */
enum class GfxInstanceFlag : uint32_t {
  /** Enables internal debug validation and logging. Helps
   *  ensure that API usage is correct across backends. */
  eDebugValidation  = (1u << 0),
  /** Enables debug markers for graphics debuggers */
  eDebugMarkers     = (1u << 1),
  /** Enables backend API validation layers, if available */
  eApiValidation    = (1u << 2),

  eFlagEnum         = 0
};

using GfxInstanceFlags = Flags<GfxInstanceFlag>;


/**
 * \brief Graphics system interface
 */
class GfxIface {

public:

  virtual ~GfxIface() { }

  /**
   * \brief Queries backend type
   * \returns Backend type
   */
  virtual GfxBackend getBackendType() const = 0;

  /**
   * \brief Enumerates graphics adapters
   *
   * Note that this method may return new adapter objects
   * every time it gets invoked, even for the same index.
   * \param [in] index Adapter index
   * \returns Adapter object, or \c null if the
   *    given adapter index is out of bounds
   */
  virtual GfxAdapter enumAdapters(
          uint32_t                      index) = 0;

  /**
   * \brief Creates a logical device
   *
   * \param [in] adapter Adapter to create device for
   * \returns Device object
   */
  virtual GfxDevice createDevice(
    const GfxAdapter&                   adapter) = 0;

};


/**
 * \brief Graphics system
 */
class Gfx : public IfaceRef<GfxIface> {

public:

  Gfx() { }
  Gfx(std::nullptr_t) { }

  /**
   * \brief Initializes graphics system with the given backend
   *
   * \param [in] wsi Optionally specifies the WSI instance. If no WSI
   *    is specified, the graphics system will not be able to present
   *    to the screen and effectively run in headless mode.
   * \param [in] backend The preferred graphics backend. If this fails
   *    to initialize then the system will try to fall back to another
   *    backend that is compatible with the given WSI instance.
   * \param [in] flags Instance creation flags
   */
  Gfx(
          GfxBackend                    backend,
    const Wsi&                          wsi,
          GfxInstanceFlags              flags);

  /**
   * \brief Looks up information about a specific format
   *
   * \param [in] format Format to look up
   * \returns Format info
   */
  static const GfxFormatInfo& getFormatInfo(
          GfxFormat                     format);

private:

  static std::shared_ptr<GfxIface> initBackend(
          GfxBackend                    backend,
    const Wsi&                          wsi,
          GfxInstanceFlags              flags);

};

}
