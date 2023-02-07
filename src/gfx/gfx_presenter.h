#pragma once

#include <functional>

#include "../util/util_iface.h"

#include "../wsi/wsi_window.h"

#include "gfx_context.h"
#include "gfx_format.h"
#include "gfx_image.h"
#include "gfx_submission.h"
#include "gfx_types.h"

namespace as {

/**
 * \brief Color space
 */
enum class GfxColorSpace : uint32_t {
  /** Standard sRGB SDR color space, typically used with RGBA8 or
   *  BGRA8 images but may return a different image format. */
  eSrgb,
  /** HDR10 color space used with RGB10A2 or BGR10A2 formats.
   *  Image data must use the PQ encoding to display correctly. */
  eHdr10,
};


/**
 * \brief Present mode
 */
enum class GfxPresentMode : uint32_t {
  /** Vertical synchronization enabled and frame rate limited to
   *  display refresh rate. Supported on all platforms. */
  eFifo,
  /** Tear-free but not synchronized to display refresh. */
  eMailbox,
  /** Tearing enabled with no frame rate limitation. */
  eImmediate,
};


/**
 * \brief Presenter context interface
 *
 * Exposes objects and functionality needed for
 * applications to effectively perform presentation.
 */
class GfxPresenterContext {

public:

  GfxPresenterContext();

  GfxPresenterContext(
          GfxContext                    context,
          GfxImage                      image,
          GfxCommandSubmission&         submission);

  ~GfxPresenterContext();

  GfxPresenterContext             (const GfxPresenterContext&) = delete;
  GfxPresenterContext& operator = (const GfxPresenterContext&) = delete;

  /**
   * \brief Retrieves the context object for presentation
   *
   * Presentation commands \e must be recorded into this context.
   * The context is guaranteed to be in a default state.
   * \returns Context object
   */
  GfxContext getContext() const {
    return m_context;
  }

  /**
   * \brief Retrieves image to present
   *
   * This image \e must be initialized before use, and \e must
   * be transitioned using \c GfxUsage::ePresent at the end of
   * the command list.
   *
   * Note that this image \e must \e not be used in any context
   * outside of presentation, even in subsequent frames.
   * \returns Image to present 
   */
  GfxImage getImage() const {
    return m_image;
  }

  /**
   * \brief Adds a semaphore to wait on before the submission
   *
   * It may be necessary for applications to synchronize access
   * to resources used within the presentation command list.
   * \param [in] semaphore Semaphore to wait on
   * \param [in] value Value to wait for
   */
  void addWaitSemaphore(
          GfxSemaphore                  semaphore,
          uint64_t                      value) const {
    return m_submission->addWaitSemaphore(std::move(semaphore), value);
  }

  /**
   * \brief Adds a semaphore to signal after the submission
   *
   * All resources accessed during presentation will be safe to
   * access again once this semaphore reaches the given value,
   * except for the swap chain image itself.
   * \param [in] semaphore Semaphore to signal
   * \param [in] value Value to signal semaphore to
   */
  void addSignalSemaphore(
          GfxSemaphore                  semaphore,
          uint64_t                      value) const {
    return m_submission->addSignalSemaphore(std::move(semaphore), value);
  }

private:

  GfxContext              m_context;
  GfxImage                m_image;
  GfxCommandSubmission*   m_submission = nullptr;

};


/**
 * \brief Presenter callback
 */
using GfxPresenterProc = std::function<void (const GfxPresenterContext&)>;


/**
 * \brief Presenter description
 *
 * Properties that the presenter is created with.
 */
struct GfxPresenterDesc {
  /** Window to create the presenter for. */
  WsiWindow window;
  /** Queue that presentation will be performed on. The command submission
   *  that records presentation commands will always be submitted to this
   *  queue, and presentation itself will be performed on this queue if the
   *  device supports it. presentation will transparently be performed on
   *  a different hardware queue if necessary. */
  GfxQueue queue = GfxQueue::eGraphics;
  /** Desired image usage. This must \e only consist of write usage,
   *  such as render target, shader storage, or transfer dst. */
  GfxUsageFlags imageUsage = GfxUsage::eRenderTarget;
};


/**
 * \brief Presenter interface
 */
class GfxPresenterIface {

public:

  virtual ~GfxPresenterIface() { }

  /**
   * \brief Checks whether the given color space and format are supported
   *
   * If the given color space is not supported by the implementation with
   * any format, this method will return \c false.
   *
   * As for the format parameter, if the format is \ref GfxFormat::eUnknown,
   * this funtion will check whether the given color space is supported for
   * any format. Otherwise, it will only return \c true if the combination
   * of format and color space are natively supported without conversion.
   *
   * Note that any format can be used with any supported color space, but
   * if the combination of format and color space is not natively supported,
   * a blit will take place at present time, which may incur a performance hit.
   * \param [in] format Format to query. May be \ref GfxFormat::eUnknown.
   * \param [in] colorSpace Color space to query
   * \returns \c true if the given combination of format and color space
   *    are natively supported by the device, as described above.
   */
  virtual bool supportsFormat(
          GfxFormat                     format,
          GfxColorSpace                 colorSpace) = 0;

  /**
   * \brief Checks whether the given present mode is supported
   *
   * Note that if an unsupported present mode is used for the presenter,
   * a supported one will be picked based on a priority system.
   * \param [in] presentMode Present mode to query
   * \returns \c true if the given present mode is supported.
   */
  virtual bool supportsPresentMode(
          GfxPresentMode                presentMode) = 0;

  /**
   * \brief Sets swap chain format and color space
   *
   * \param [in] format Image format. If set to \ref GfxFormat::eUnknown,
   *    a format that is natively supported for the given color space
   *    will be selected for optimal performance.
   * \param [in] colorSpace Color space. If the given color space is
   *    unsupported, the swap chain will fall back to sRGB.
   */
  virtual void setFormat(
          GfxFormat                     format,
          GfxColorSpace                 colorSpace) = 0;

  /**
   * \brief Sets swap chain present mode
   *
   * \param [in] presentMode Present mode. If the given present mode
   *    is unsupported, another will be picked based on a priority system.
   */
  virtual void setPresentMode(
          GfxPresentMode                presentMode) = 0;

  /**
   * \brief Presents an image
   */
  virtual bool present(
    const GfxPresenterProc&             proc) = 0;

};

using GfxPresenter = IfaceRef<GfxPresenterIface>;

}
