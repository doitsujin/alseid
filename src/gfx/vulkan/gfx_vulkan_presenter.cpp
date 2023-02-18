#include <cstdint>

#include <cs_present_blit.h>
#include <fs_present_blit.h>
#include <vs_present_blit.h>

#include "gfx_vulkan_image.h"
#include "gfx_vulkan_presenter.h"
#include "gfx_vulkan_semaphore.h"
#include "gfx_vulkan_utils.h"

namespace as {

GfxVulkanPresenter::GfxVulkanPresenter(
        std::shared_ptr<GfxVulkanDevice> device,
        GfxVulkanWsi                  wsiBrdige,
  const GfxPresenterDesc&             desc)
: m_device        (std::move(device))
, m_wsi           (std::move(wsiBrdige))
, m_desc          (desc) {
  createFence();
  createSurface();
}


GfxVulkanPresenter::~GfxVulkanPresenter() {
  destroySwapchain();
  destroySurface();
  destroyFence();
}


bool GfxVulkanPresenter::supportsFormat(
        GfxFormat                     format,
        GfxColorSpace                 colorSpace) {
  auto& vk = m_device->vk();
  VkResult vr = VK_NOT_READY;

  uint32_t formatCount = 0;
  std::vector<VkSurfaceFormatKHR> formats;

  while (vr) {
    if (vr == VK_ERROR_SURFACE_LOST_KHR) {
      destroySwapchain();
      destroySurface();
      createSurface();
    }

    vr = vk.vkGetPhysicalDeviceSurfaceFormatsKHR(vk.adapter, m_surface, &formatCount, nullptr);

    if (!handleVkResult("Vulkan: Failed to query surface formats", vr))
      continue;

    formats.resize(formatCount);
    vr = vk.vkGetPhysicalDeviceSurfaceFormatsKHR(vk.adapter, m_surface, &formatCount, formats.data());

    if (!handleVkResult("Vulkan: Failed to query surface formats", vr))
      continue;
  }

  // Find first format with a matching color space, and
  // also match the format if an exact format is specified.
  auto e = std::find_if(formats.begin(), formats.end(), [
    cFormat     = m_device->getVkFormat(format),
    cColorSpace = getVkColorSpace(colorSpace)
  ] (const VkSurfaceFormatKHR& c) {
    if (c.colorSpace != cColorSpace)
      return false;

    if (c.format == cFormat)
      return true;

    return cFormat == VK_FORMAT_UNDEFINED;
  });

  return e != formats.end();
}


bool GfxVulkanPresenter::supportsPresentMode(
        GfxPresentMode                presentMode) {
  auto& vk = m_device->vk();
  VkResult vr = VK_NOT_READY;

  uint32_t modeCount = 0;
  std::vector<VkPresentModeKHR> modes;

  while (vr) {
    if (vr == VK_ERROR_SURFACE_LOST_KHR) {
      destroySwapchain();
      destroySurface();
      createSurface();
    }

    vr = vk.vkGetPhysicalDeviceSurfacePresentModesKHR(vk.adapter, m_surface, &modeCount, nullptr);

    if (!handleVkResult("Failed to query present modes", vr))
      continue;

    vr = vk.vkGetPhysicalDeviceSurfacePresentModesKHR(vk.adapter, m_surface, &modeCount, modes.data());

    if (!handleVkResult("Failed to query present modes", vr))
      continue;
  }

  auto e = std::find(modes.begin(), modes.end(), getVkPresentMode(presentMode));
  return e != modes.end();
}


void GfxVulkanPresenter::setFormat(
        GfxFormat                     format,
        GfxColorSpace                 colorSpace) {
  m_dirty |= m_format != format || m_colorSpace != colorSpace;

  m_format = format;
  m_colorSpace = colorSpace;
}


void GfxVulkanPresenter::setPresentMode(
        GfxPresentMode                presentMode) {
  m_dirty |= m_presentMode != presentMode;

  m_presentMode = presentMode;
}


GfxPresentStatus GfxVulkanPresenter::present(
  const GfxPresenterProc&             proc) {
  auto& vk = m_device->vk();

  // Try to acquire an image. If this fails for whatever reason,
  // recreate the swap chain and surface as necessary.
  VkResult vr = VK_ERROR_OUT_OF_DATE_KHR;
  uint32_t imageId = 0;

  if (m_swapchain && !m_dirty) {
    vr = vk.vkAcquireNextImageKHR(vk.device, m_swapchain,
      ~0ull, VK_NULL_HANDLE, m_fence, &imageId);
  }

  while (vr) {
    destroySwapchain();

    if (vr == VK_ERROR_SURFACE_LOST_KHR) {
      destroySurface();
      createSurface();
    }

    vr = createSwapchain();

    if (!handleVkResult("Vulkan: Failed to create swap chain", vr))
      continue;

    if (!m_swapchain)
      return GfxPresentStatus::eAcquireFailed;

    vr = vk.vkAcquireNextImageKHR(vk.device, m_swapchain,
      ~0ull, VK_NULL_HANDLE, m_fence, &imageId);
  }

  // Wait for image acquisition to complete. Most drivers will stall
  // here anyway, so there's no real advantage to using semaphores.
  vr = vk.vkWaitForFences(vk.device, 1, &m_fence, VK_TRUE, ~0ull);

  if (!vr)
    vr = vk.vkResetFences(vk.device, 1, &m_fence);

  if (vr)
    throw VulkanError("Vulkan: Failed to wait for presenter fence.", vr);

  // The acquisition fence itself doesn't quite guarantee
  // that it's actually safe to reset the command buffers,
  // so we need a per-image timeline as well.
  auto& objects = m_objects.at(imageId);

  objects.timeline->wait(objects.timelineValue);
  objects.context->reset();

  // Execute presenter callback and record blit commands as necessary
  bool needsBlit = m_blitMode != GfxVulkanPresenterBlitMode::eNone;

  GfxPresenterContext context(objects.context,
    needsBlit ? m_image : objects.image,
    m_submission);

  proc(context);

  if (needsBlit)
    recordBlit(context.getContext(), context.getImage(), objects.image);

  // Submit presenter command list
  m_submission.addCommandList(context.getContext()->endCommandList());
  m_submission.addSignalSemaphore(objects.semaphore, 0);
  m_submission.addSignalSemaphore(objects.timeline, ++objects.timelineValue);

  m_device->submit(m_desc.queue, std::move(m_submission));

  // Execute the actual present operation
  vr = m_device->present(m_presentQueue,
    static_cast<GfxVulkanSemaphore&>(*objects.semaphore).getHandle(),
    m_swapchain, imageId);

  if (vr >= 0)
    return GfxPresentStatus::eSuccess;

  if (vr == VK_ERROR_SURFACE_LOST_KHR) {
    destroySwapchain();
    destroySurface();
    createSurface();
  }

  return GfxPresentStatus::ePresentFailed;
}


uint32_t GfxVulkanPresenter::pickImageCount(
  const VkSurfaceCapabilitiesKHR&     caps) const {
  uint32_t count = caps.minImageCount + 1;

  if (caps.maxImageCount)
    count = std::max(count, caps.maxImageCount);

  return count;
}


VkExtent2D GfxVulkanPresenter::pickImageExtent(
  const VkSurfaceCapabilitiesKHR&     caps) const {
  if (caps.currentExtent.width == ~0u && caps.currentExtent.height == ~0u)
    return caps.currentExtent;

  Extent2D surfaceExtent = m_wsi->getSurfaceSize(m_desc.window);

  VkExtent2D extent;
  extent.width  = std::clamp(surfaceExtent.at<0>(), caps.minImageExtent.width,  caps.maxImageExtent.width);
  extent.height = std::clamp(surfaceExtent.at<1>(), caps.minImageExtent.height, caps.maxImageExtent.height);
  return extent;
}


VkResult GfxVulkanPresenter::pickSurfaceFormat(
  const VkSurfaceFormatKHR&           desired,
        VkSurfaceFormatKHR&           actual) const {
  auto& vk = m_device->vk();
  VkResult vr;

  uint32_t formatCount = 0;
  if ((vr = vk.vkGetPhysicalDeviceSurfaceFormatsKHR(vk.adapter, m_surface, &formatCount, nullptr)))
    return vr;

  std::vector<VkSurfaceFormatKHR> formats(formatCount);
  if ((vr = vk.vkGetPhysicalDeviceSurfaceFormatsKHR(vk.adapter, m_surface, &formatCount, formats.data())))
    return vr;

  if (!formatCount)
    throw VulkanError("Vulkan: No supported surface formats found.", VK_ERROR_UNKNOWN);

  // Try to find an entry that matches exactly
  if (desired.format) {
    auto e = std::find_if(formats.begin(), formats.end(), [
      cFormat         = desired.format,
      cColorSpace     = desired.colorSpace
    ] (const VkSurfaceFormatKHR& c) {
      return c.format == cFormat && c.colorSpace == cColorSpace;
    });

    if (e != formats.end()) {
      actual = *e;
      return VK_SUCCESS;
    }
  }

  // For sRGB, prioritize basic RGBA8 or BGRA8 formats. For other
  // color spaces, prioritize formats with higher bit depths.
  std::vector<VkFormat> formatPriority;

  if (desired.format == VK_FORMAT_R8G8B8A8_SRGB || desired.format == VK_FORMAT_B8G8R8A8_SRGB) {
    formatPriority.push_back(VK_FORMAT_R8G8B8A8_SRGB);
    formatPriority.push_back(VK_FORMAT_B8G8R8A8_SRGB);
  } else {
    if (desired.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      formatPriority.push_back(VK_FORMAT_R8G8B8A8_UNORM);
      formatPriority.push_back(VK_FORMAT_B8G8R8A8_UNORM);
    }

    formatPriority.push_back(VK_FORMAT_A2B10G10R10_UNORM_PACK32);
    formatPriority.push_back(VK_FORMAT_A2R10G10B10_UNORM_PACK32);
    formatPriority.push_back(VK_FORMAT_R16G16B16A16_SFLOAT);

    if (desired.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      formatPriority.push_back(VK_FORMAT_R8G8B8A8_UNORM);
      formatPriority.push_back(VK_FORMAT_B8G8R8A8_UNORM);
    }
  }

  for (auto f : formatPriority) {
    auto e = std::find_if(formats.begin(), formats.end(), [
      cFormat         = f,
      cColorSpace     = desired.colorSpace
    ] (const VkSurfaceFormatKHR& c) {
      return c.format == cFormat && c.colorSpace == cColorSpace;
    });

    if (e != formats.end()) {
      actual = *e;
      return VK_SUCCESS;
    }
  }

  // Otherwise, pick the first supported format in the requested color space
  auto e = std::find_if(formats.begin(), formats.end(), [
    cColorSpace     = desired.colorSpace
  ] (const VkSurfaceFormatKHR& c) {
    return c.colorSpace == cColorSpace;
  });

  if (e != formats.end()) {
    actual = *e;
    return VK_SUCCESS;
  }

  // If that still didn't work, just pick the first supported format
  actual = *formats.begin();
  return VK_SUCCESS;
}


VkResult GfxVulkanPresenter::pickPresentMode(
        VkPresentModeKHR              desired,
        VkPresentModeKHR&             actual) const {
  auto& vk = m_device->vk();
  VkResult vr;

  uint32_t modeCount = 0;
  if ((vr = vk.vkGetPhysicalDeviceSurfacePresentModesKHR(vk.adapter, m_surface, &modeCount, nullptr)))
    return vr;

  std::vector<VkPresentModeKHR> modes(modeCount);
  if ((vr = vk.vkGetPhysicalDeviceSurfacePresentModesKHR(vk.adapter, m_surface, &modeCount, modes.data())))
    return vr;

  if (!modeCount)
    throw VulkanError("Vulkan: No supported present modes found.", VK_ERROR_UNKNOWN);

  // Find supported mode that best matches the desired mode
  const std::array<VkPresentModeKHR, 3> modePriority = {{
    VK_PRESENT_MODE_IMMEDIATE_KHR,
    VK_PRESENT_MODE_MAILBOX_KHR,
    VK_PRESENT_MODE_FIFO_KHR,
  }};

  auto e = std::find(modePriority.begin(), modePriority.end(), desired);

  while (e != modePriority.end()) {
    auto mode = *e;
    auto f = std::find(modes.begin(), modes.end(), mode);

    if (f != modes.end()) {
      actual = mode;
      return VK_SUCCESS;
    }
  }

  // This shouldn't happen since FIFO is required
  actual = *modes.begin();
  return VK_SUCCESS;
}


void GfxVulkanPresenter::createFence() {
  auto& vk = m_device->vk();

  VkFenceCreateInfo fenceInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
  VkResult vr = vk.vkCreateFence(vk.device, &fenceInfo, nullptr, &m_fence);

  if (vr)
    throw VulkanError("Vulkan: Failed to create fence", vr);
}


void GfxVulkanPresenter::createSurface() {
  auto& vk = m_device->vk();

  VkResult vr = m_wsi->createSurface(m_desc.window, &m_surface);

  if (vr)
    throw VulkanError("Vulkan: Failed to create surface", vr);

  // Check which queue we can present on
  if (m_wsi->checkSurfaceSupport(vk.adapter, m_device->getQueueFamilyIndex(m_desc.queue)))
    m_presentQueue = m_desc.queue;
  else
    m_presentQueue = GfxQueue::ePresent;
}


VkResult GfxVulkanPresenter::createSwapchain() {
  auto& vk = m_device->vk();

  if (!m_surface)
    return VK_ERROR_SURFACE_LOST_KHR;

  VkSurfaceCapabilitiesKHR caps = { };
  VkResult vr = vk.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk.adapter, m_surface, &caps);

  if (!handleVkResult("Vulkan: Failed to query surface capabilities", vr))
    return vr;

  // If we can't create a swap chain, exit immediately
  if (!caps.maxImageExtent.width || !caps.maxImageExtent.height)
    return VK_SUCCESS;

  // Find surface format that best matches the swap chain
  VkSurfaceFormatKHR desiredFormat;
  desiredFormat.format = m_device->getVkFormat(m_format);
  desiredFormat.colorSpace = getVkColorSpace(m_colorSpace);

  VkSurfaceFormatKHR actualFormat = { };
  vr = pickSurfaceFormat(desiredFormat, actualFormat);

  if (!handleVkResult("Vulkan: Failed to pick surface format", vr))
    return vr;

  // Pick present mode that best matches the desired mode
  VkPresentModeKHR desiredPresentMode = getVkPresentMode(m_presentMode);
  VkPresentModeKHR actualPresentMode = VK_PRESENT_MODE_FIFO_KHR;

  vr = pickPresentMode(desiredPresentMode, actualPresentMode);

  if (!handleVkResult("Vulkan: Failed to pick present mode", vr))
    return vr;

  // Pick image count and extent based on surface and window properties
  VkExtent2D imageExtent = pickImageExtent(caps);
  uint32_t imageCount = pickImageCount(caps);

  // Check whether a blit is going to be required for presentation. This is
  // true if either the required image usage flags are not supported or if
  // the desired format is not supported for the desired color space.
  GfxUsageFlags actualUsage = m_desc.imageUsage;

  VkImageUsageFlags requiredUsage = getVkImageUsage(m_format, actualUsage);
  bool needsBlit = (caps.supportedUsageFlags & requiredUsage) != requiredUsage;

  if (m_format != GfxFormat::eUnknown)
    needsBlit |= desiredFormat.format != actualFormat.format;

  if (needsBlit) {
    // Create back buffer image as necessary and set blit mode
    if (m_desc.queue == GfxQueue::eGraphics) {
      m_blitMode = GfxVulkanPresenterBlitMode::eGraphics;
      actualUsage = GfxUsage::eRenderTarget;
    } else {
      m_blitMode = GfxVulkanPresenterBlitMode::eCompute;
      actualUsage = GfxUsage::eShaderStorage;
    }

    GfxImageDesc imageDesc;
    imageDesc.debugName = "Virtual Swapchain Image";
    imageDesc.type = GfxImageType::e2D;
    imageDesc.format = m_format;
    imageDesc.extent = Extent3D(imageExtent.width, imageExtent.height, 1u);
    imageDesc.usage = m_desc.imageUsage | GfxUsage::eShaderResource;
    imageDesc.flags = GfxImageFlag::eDedicatedAllocation;

    if (imageDesc.format == GfxFormat::eUnknown)
      imageDesc.format = m_device->getGfxFormat(actualFormat.format);

    m_image = m_device->createImage(imageDesc, GfxMemoryType::eAny);
  } else {
    m_blitMode = GfxVulkanPresenterBlitMode::eNone;
  }

  // Create the actual Vulkan swap chain
  const std::array<uint32_t, 2> queueFamilies = {
    m_device->getQueueFamilyIndex(m_desc.queue),
    m_device->getQueueFamilyIndex(m_presentQueue),
  };

  VkSwapchainCreateInfoKHR swapchainInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
  swapchainInfo.surface = m_surface;
  swapchainInfo.minImageCount = imageCount;
  swapchainInfo.imageFormat = actualFormat.format;
  swapchainInfo.imageColorSpace = actualFormat.colorSpace;
  swapchainInfo.imageExtent = imageExtent;
  swapchainInfo.imageArrayLayers = 1;
  swapchainInfo.imageUsage = getVkImageUsage(m_format, actualUsage);

  swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (queueFamilies[0] != queueFamilies[1]) {
    swapchainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    swapchainInfo.queueFamilyIndexCount = uint32_t(queueFamilies.size());
    swapchainInfo.pQueueFamilyIndices = queueFamilies.data();
  }

  swapchainInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapchainInfo.presentMode = actualPresentMode;
  swapchainInfo.clipped = VK_TRUE;

  vr = vk.vkCreateSwapchainKHR(vk.device, &swapchainInfo, nullptr, &m_swapchain);

  if (!handleVkResult("Vulkan: Failed to create swap chain.", vr)) {
    destroySwapchain();
    return vr;
  }

  // Query image handles from the swap chain
  vr = vk.vkGetSwapchainImagesKHR(vk.device, m_swapchain, &imageCount, nullptr);

  if (!handleVkResult("Failed to query swap chain images.", vr)) {
    destroySwapchain();
    return vr;
  }

  std::vector<VkImage> imageHandles(imageCount);
  vr = vk.vkGetSwapchainImagesKHR(vk.device, m_swapchain, &imageCount, imageHandles.data());

  if (!handleVkResult("Failed to query swap chain images.", vr)) {
    destroySwapchain();
    return vr;
  }

  // Create additional per-image objects
  GfxImageDesc imageDesc;
  imageDesc.debugName = "Swapchain Image";
  imageDesc.type = GfxImageType::e2D;
  imageDesc.format = m_device->getGfxFormat(actualFormat.format);
  imageDesc.usage = actualUsage;
  imageDesc.extent = Extent3D(imageExtent.width, imageExtent.height, 1u);

  GfxSemaphoreDesc semaphoreDesc;
  semaphoreDesc.debugName = "Swapchain WSI Semaphore";

  GfxSemaphoreDesc timelineDesc;
  timelineDesc.debugName = "Swapchain timeline Semaphore";
  timelineDesc.initialValue = 0;

  m_objects.resize(imageCount);

  for (uint32_t i = 0; i < imageCount; i++) {
    auto& objects = m_objects.at(i);

    objects.image = GfxImage(std::make_shared<GfxVulkanImage>(m_device, imageDesc,
      imageHandles[i], swapchainInfo.imageSharingMode == VK_SHARING_MODE_CONCURRENT));
    objects.context = m_device->createContext(m_desc.queue);
    objects.semaphore = GfxSemaphore(std::make_shared<GfxVulkanSemaphore>(
      m_device, semaphoreDesc, VK_SEMAPHORE_TYPE_BINARY));
    objects.timeline = GfxSemaphore(std::make_shared<GfxVulkanSemaphore>(
      m_device, semaphoreDesc, VK_SEMAPHORE_TYPE_TIMELINE));
    objects.timelineValue = 0;
  }

  m_dirty = VK_FALSE;
  return VK_SUCCESS;
}


void GfxVulkanPresenter::destroyFence() {
  auto& vk = m_device->vk();

  vk.vkDestroyFence(vk.device, m_fence, nullptr);
}


void GfxVulkanPresenter::destroySurface() {
  auto& vk = m_device->vk();

  if (!m_surface)
    return;

  vk.vkDestroySurfaceKHR(vk.instance, m_surface, nullptr);

  m_surface = VK_NULL_HANDLE;
}


void GfxVulkanPresenter::destroySwapchain() {
  auto& vk = m_device->vk();

  if (!m_swapchain)
    return;

  // Wait until all queues involved in presentation are idle. This
  // is necessary to synchronize swap image and semaphore access.
  m_device->waitQueueIdle(m_desc.queue);

  if (m_presentQueue != m_desc.queue)
    m_device->waitQueueIdle(m_presentQueue);

  vk.vkDestroySwapchainKHR(vk.device, m_swapchain, nullptr);

  // Destroy other per-image objects involved in presentation
  m_objects.clear();

  m_image = GfxImage();
  m_swapchain = VK_NULL_HANDLE;
}


void GfxVulkanPresenter::recordBlit(
        GfxContext                    context,
        GfxImage                      srcImage,
        GfxImage                      dstImage) {
  context->beginDebugLabel("Vulkan swap chain blit", { 0.8f, 0.8f, 0.8f, 1.0f });

  GfxImageDesc srcImageDesc = srcImage->getDesc();
  GfxImageDesc dstImageDesc = dstImage->getDesc();

  GfxImageViewDesc srcViewDesc;
  srcViewDesc.type = GfxImageViewType::e2D;
  srcViewDesc.format = srcImageDesc.format;
  srcViewDesc.subresource = srcImage->getAvailableSubresources();
  srcViewDesc.usage = GfxUsage::eShaderResource;

  GfxImageViewDesc dstViewDesc;
  dstViewDesc.type = GfxImageViewType::e2D;
  dstViewDesc.format = dstImageDesc.format;
  dstViewDesc.subresource = dstImage->getAvailableSubresources();
  dstViewDesc.usage = m_blitMode == GfxVulkanPresenterBlitMode::eGraphics
    ? GfxUsage::eRenderTarget : GfxUsage::eShaderStorage;

  GfxImageView srcView = srcImage->createView(srcViewDesc);
  GfxImageView dstView = dstImage->createView(dstViewDesc);

  Extent2D imageExtent = Extent2D(dstImageDesc.extent);

  switch (m_blitMode) {
    case GfxVulkanPresenterBlitMode::eGraphics: {
      if (!m_blitPipelineGraphics)
        m_blitPipelineGraphics = createGraphicsBlitPipeline();

      context->imageBarrier(dstImage, dstImage->getAvailableSubresources(),
        0, 0, GfxUsage::eRenderTarget, 0, GfxBarrierFlag::eDiscard);

      GfxRenderingInfo renderInfo;
      renderInfo.color[0].op = GfxRenderTargetOp::eDiscard;
      renderInfo.color[0].view = dstView;

      GfxViewport viewport;
      viewport.extent = Vector2D(imageExtent);
      viewport.scissor.extent = imageExtent;

      context->beginRendering(renderInfo, 0);
      context->bindPipeline(m_blitPipelineGraphics);
      context->bindDescriptor(0, 0, srcView->getDescriptor());
      context->setVertexInputState(nullptr);
      context->setRasterizerState(nullptr);
      context->setDepthStencilState(nullptr);
      context->setColorBlendState(nullptr);
      context->setMultisampleState(nullptr);
      context->setViewports(1, &viewport);
      context->draw(3, 1, 0, 0);
      context->endRendering();

      context->imageBarrier(dstImage, dstImage->getAvailableSubresources(),
        GfxUsage::eRenderTarget, 0,
        GfxUsage::ePresent, 0, 0);
    } break;

    case GfxVulkanPresenterBlitMode::eCompute: {
      if (!m_blitPipelineCompute)
        m_blitPipelineCompute = createComputeBlitPipeline();

      Extent3D workgroupCount = gfxComputeWorkgroupCount(dstImageDesc.extent,
        m_blitPipelineCompute->getWorkgroupSize());

      context->imageBarrier(dstImage, dstImage->getAvailableSubresources(),
        0, 0, GfxUsage::eShaderStorage, GfxShaderStage::eCompute, GfxBarrierFlag::eDiscard);

      context->bindPipeline(m_blitPipelineCompute);
      context->bindDescriptor(0, 0, srcView->getDescriptor());
      context->bindDescriptor(0, 1, dstView->getDescriptor());
      context->setShaderConstants(0, sizeof(imageExtent), &imageExtent);
      context->dispatch(workgroupCount);

      context->imageBarrier(dstImage, dstImage->getAvailableSubresources(),
        GfxUsage::eShaderStorage, GfxShaderStage::eCompute,
        GfxUsage::ePresent, 0, 0);
    } break;

    case GfxVulkanPresenterBlitMode::eNone:
      break;
  }

  context->endDebugLabel();
}


GfxComputePipeline GfxVulkanPresenter::createComputeBlitPipeline() {
  GfxComputePipelineDesc pipelineDesc;
  pipelineDesc.debugName = "Presentation blit";
  pipelineDesc.compute = createVkBuiltInShader(cs_present_blit);
  return m_device->createComputePipeline(pipelineDesc);
}


GfxGraphicsPipeline GfxVulkanPresenter::createGraphicsBlitPipeline() {
  GfxGraphicsPipelineDesc pipelineDesc;
  pipelineDesc.debugName = "Presentation blit";
  pipelineDesc.vertex = createVkBuiltInShader(vs_present_blit);
  pipelineDesc.fragment = createVkBuiltInShader(fs_present_blit);
  return m_device->createGraphicsPipeline(pipelineDesc);
}


VkColorSpaceKHR GfxVulkanPresenter::getVkColorSpace(
        GfxColorSpace                 colorSpace) {
  switch (colorSpace) {
    case GfxColorSpace::eSrgb:
      return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    case GfxColorSpace::eHdr10:
      return VK_COLOR_SPACE_HDR10_ST2084_EXT;
  }

  throw Error("Invalid color space");
}


VkPresentModeKHR GfxVulkanPresenter::getVkPresentMode(
        GfxPresentMode                presentMode) {
  switch (presentMode) {
    case GfxPresentMode::eFifo:
      return VK_PRESENT_MODE_FIFO_KHR;
    case GfxPresentMode::eMailbox:
      return VK_PRESENT_MODE_MAILBOX_KHR;
    case GfxPresentMode::eImmediate:
      return VK_PRESENT_MODE_IMMEDIATE_KHR;
  }

  throw Error("Invalid present mode");
}


bool GfxVulkanPresenter::handleVkResult(
  const char*                         pMessage,
        VkResult                      vr) {
  if (vr >= 0)
    return true;

  if (vr == VK_ERROR_SURFACE_LOST_KHR)
    return false;

  throw VulkanError(pMessage, vr);
}

}
