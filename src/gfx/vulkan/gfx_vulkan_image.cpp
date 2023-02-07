#include "../../util/util_math.h"

#include "../gfx.h"

#include "gfx_vulkan_image.h"
#include "gfx_vulkan_utils.h"

namespace as {

GfxVulkanImageView::GfxVulkanImageView(
        std::shared_ptr<GfxVulkanDevice> device,
  const GfxVulkanImage&               image,
  const GfxImageViewDesc&             desc)
: GfxImageViewIface (image, desc)
, m_device          (std::move(device))
, m_layout          (getVkImageLayoutFromUsage(image, desc.usage)) {
  auto& vk = m_device->vk();

  VkImageViewUsageCreateInfo usageInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO };
  usageInfo.usage = getVkImageUsage(desc.format, desc.usage);

  VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, &usageInfo };
  viewInfo.image = image.getHandle();
  viewInfo.viewType = getVkImageViewType(desc.type);
  viewInfo.format = m_device->getVkFormat(desc.format);
  viewInfo.subresourceRange = getVkImageSubresourceRange(desc.subresource);

  VkResult vr = vk.vkCreateImageView(vk.device, &viewInfo, nullptr, &m_view);

  if (vr)
    throw VulkanError("Vulkan: Failed to create image view", vr);
}


GfxVulkanImageView::~GfxVulkanImageView() {
  auto& vk = m_device->vk();

  vk.vkDestroyImageView(vk.device, m_view, nullptr);
}


GfxDescriptor GfxVulkanImageView::getDescriptor() const {
  GfxVulkanDescriptor descriptor = { };
  descriptor.image.imageView = m_view;
  descriptor.image.imageLayout = m_layout;

  return exportVkDescriptor(descriptor);
}


GfxVulkanImage::GfxVulkanImage(
        std::shared_ptr<GfxVulkanDevice> device,
  const GfxImageDesc&                 desc,
        VkImage                       image,
        GfxVulkanMemorySlice&&        memory)
: GfxImageIface   (desc)
, m_device        (std::move(device))
, m_memory        (std::move(memory))
, m_image         (image)
, m_isConcurrent  (desc.flags & GfxImageFlag::eSimultaneousAccess) {
  m_device->setDebugName(m_image, desc.debugName);

  setupStageAccessFlags();
}


GfxVulkanImage::GfxVulkanImage(
        std::shared_ptr<GfxVulkanDevice> device,
  const GfxImageDesc&                 desc,
        VkImage                       image,
        VkBool32                      isConcurrent)
: GfxImageIface   (desc)
, m_device        (std::move(device))
, m_image         (image)
, m_isExternal    (VK_TRUE)
, m_isConcurrent  (isConcurrent) {
  m_device->setDebugName(m_image, desc.debugName);

  setupStageAccessFlags();
}


GfxVulkanImage::~GfxVulkanImage() {
  auto& vk = m_device->vk();

  if (!m_isExternal)
    vk.vkDestroyImage(vk.device, m_image, nullptr);
}


GfxImageView GfxVulkanImage::createView(
  const GfxImageViewDesc&             desc) {
  { std::shared_lock lock(m_viewLock);

    auto entry = m_viewMap.find(desc);
    if (entry != m_viewMap.end())
      return GfxImageView(entry->second);
  }

  std::unique_lock lock(m_viewLock);

  auto result = m_viewMap.emplace(std::piecewise_construct,
    std::forward_as_tuple(desc),
    std::forward_as_tuple(m_device, *this, desc));

  return GfxImageView(result.first->second);
}


GfxMemoryInfo GfxVulkanImage::getMemoryInfo() const {
  GfxMemoryInfo result;
  result.type = m_memory.getMemoryType();
  result.size = m_memory.getSize();
  return result;
}


void GfxVulkanImage::setupStageAccessFlags() {
  // Since render target usage is one bit in the frontend but
  // different bits in Vulkan, mask out the invalid ones based
  // on the format. All other bits are allowed.
  auto& formatInfo = Gfx::getFormatInfo(m_desc.format);

  if (formatInfo.aspects & (GfxImageAspect::eDepth | GfxImageAspect::eStencil)) {
    m_stageFlags  = ~(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
    m_accessFlags = ~(VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
  } else {
    m_stageFlags  = ~(VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT);
    m_accessFlags = ~(VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
  }
}

}
