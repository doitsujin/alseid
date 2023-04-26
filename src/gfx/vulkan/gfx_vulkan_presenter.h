#pragma once

#include "../gfx_context.h"
#include "../gfx_presenter.h"
#include "../gfx_semaphore.h"
#include "../gfx_submission.h"

#include "gfx_vulkan_device.h"
#include "gfx_vulkan_semaphore.h"

#include "wsi/gfx_vulkan_wsi.h"

namespace as {

/**
 * \brief Vulkan presenter blit mode
 *
 * Depending on swap chain and surface support, we may
 * have to manually blit a user image to the swap image.
 */
enum class GfxVulkanPresenterBlitMode : uint32_t {
  /** No blit required */
  eNone,
  /** Graphics pipeline blit */
  eGraphics,
  /** Compute pipeline blit */
  eCompute,
};


/**
 * \brief Per-frame semaphores
 */
struct GfxVulkanPresenterSemaphores {
  GfxSemaphore acquire;
  GfxSemaphore present;

  VkSemaphore getAcquireHandle() const {
    return static_cast<GfxVulkanSemaphore&>(*acquire).getHandle();
  }

  VkSemaphore getPresentHandle() const {
    return static_cast<GfxVulkanSemaphore&>(*present).getHandle();
  }
};


/**
 * \brief Per-frame objects
 */
struct GfxVulkanPresenterObjects {
  GfxImage image;
  GfxContext context;
  GfxSemaphore timeline;
  uint64_t timelineValue = 0;
};


/**
 * \brief Vulkan presenter
 */
class GfxVulkanPresenter : public GfxPresenterIface {

public:

  GfxVulkanPresenter(
          std::shared_ptr<GfxVulkanDevice> device,
          GfxVulkanWsi                  wsiBrdige,
    const GfxPresenterDesc&             desc);

  ~GfxVulkanPresenter();

  /**
   * \brief Checks whether the given color space and format are supported
   *
   * \param [in] format Format to query. May be \c GfxFormat::eUnknown.
   * \param [in] colorSpace Color space to query
   * \returns \c true if the given combination of format and color space
   *    are natively supported by the device, as described above.
   */
  bool supportsFormat(
          GfxFormat                     format,
          GfxColorSpace                 colorSpace) override;

  /**
   * \brief Checks whether the given present mode is supported
   *
   * \param [in] presentMode Present mode to query
   * \returns \c true if the given present mode is supported.
   */
  bool supportsPresentMode(
          GfxPresentMode                presentMode) override;

  /**
   * \brief Sets swap chain format and color space
   *
   * \param [in] format Image format
   * \param [in] colorSpace Color space
   */
  void setFormat(
          GfxFormat                     format,
          GfxColorSpace                 colorSpace) override;

  /**
   * \brief Sets swap chain present mode
   * \param [in] presentMode Present mode
   */
  void setPresentMode(
          GfxPresentMode                presentMode) override;

  /**
   * \brief Presents next swap chain image
   *
   * Recreates the underlying Vulkan swap chain and
   * surface as necessary.
   * \param [in] proc Presenter callback
   * \returns Presentation status
   */
  GfxPresentStatus present(
    const GfxPresenterProc&             proc) override;

private:

  std::shared_ptr<GfxVulkanDevice>  m_device;

  GfxVulkanWsi              m_wsi;
  GfxPresenterDesc          m_desc;

  GfxCommandSubmission      m_submission;
  GfxQueue                  m_presentQueue = GfxQueue::ePresent;

  GfxComputePipeline        m_blitPipelineCompute;
  GfxGraphicsPipeline       m_blitPipelineGraphics;

  GfxFormat                 m_format      = GfxFormat::eUnknown;
  GfxColorSpace             m_colorSpace  = GfxColorSpace::eSrgb;
  GfxPresentMode            m_presentMode = GfxPresentMode::eFifo;
  GfxVulkanPresenterBlitMode m_blitMode   = GfxVulkanPresenterBlitMode::eNone;

  VkSurfaceKHR              m_surface     = VK_NULL_HANDLE;
  VkSwapchainKHR            m_swapchain   = VK_NULL_HANDLE;

  VkBool32                  m_dirty       = VK_FALSE;

  GfxImage                  m_image;

  std::vector<GfxVulkanPresenterSemaphores> m_semaphores;
  size_t                                    m_semaphoreIndex = 0;

  std::vector<GfxVulkanPresenterObjects>    m_objects;

  void waitForDevice();

  uint32_t pickImageCount(
    const VkSurfaceCapabilitiesKHR&     caps) const;

  VkExtent2D pickImageExtent(
    const VkSurfaceCapabilitiesKHR&     caps) const;

  VkResult pickSurfaceFormat(
    const VkSurfaceFormatKHR&           desired,
          VkSurfaceFormatKHR&           actual) const;

  VkResult pickPresentMode(
          VkPresentModeKHR              desired,
          VkPresentModeKHR&             actual) const;

  void createSurface();

  VkResult createSwapchain();

  void destroySurface();

  void destroySwapchain();

  void recordBlit(
          GfxContext                    context,
          GfxImage                      srcImage,
          GfxImage                      dstImage);

  GfxComputePipeline createComputeBlitPipeline();

  GfxGraphicsPipeline createGraphicsBlitPipeline();

  static VkColorSpaceKHR getVkColorSpace(
          GfxColorSpace                 colorSpace);

  static VkPresentModeKHR getVkPresentMode(
          GfxPresentMode                presentMode);

  static bool handleVkResult(
    const char*                         pMessage,
          VkResult                      vr);

};

}
