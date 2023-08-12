#pragma once

#include <mutex>
#include <shared_mutex>
#include <unordered_map>

#include "../gfx_image.h"

#include "gfx_vulkan_descriptor_handle.h"
#include "gfx_vulkan_device.h"
#include "gfx_vulkan_memory.h"

namespace as {

class GfxVulkanImage;

/**
 * \brief Vulkan image view
 */
class GfxVulkanImageView : public GfxImageViewIface {

public:

  GfxVulkanImageView(
          GfxVulkanDevice&              device,
    const GfxVulkanImage&               image,
    const GfxImageViewDesc&             desc);

  ~GfxVulkanImageView();

  GfxVulkanImageView             (const GfxVulkanImageView&) = delete;
  GfxVulkanImageView& operator = (const GfxVulkanImageView&) = delete;

  /**
   * \brief Queries Vulkan image view handle
   * \returns Vulkan image view handle
   */
  VkImageView getHandle() const {
    return m_view;
  }

  /**
   * \brief Queries Vulkan image layout
   * \returns Vulkan image layout
   */
  VkImageLayout getLayout() const {
    return m_layout;
  }

  /**
   * \brief Retrieves image view descriptor
   * \returns View descriptor
   */
  GfxDescriptor getDescriptor() const override;

private:

  GfxVulkanDevice& m_device;

  VkImageView   m_view    = VK_NULL_HANDLE;
  VkImageLayout m_layout  = VK_IMAGE_LAYOUT_UNDEFINED;

};


/**
 * \brief Vulkan image
 */
class GfxVulkanImage : public GfxImageIface {

public:

  GfxVulkanImage(
          GfxVulkanDevice&              device,
    const GfxImageDesc&                 desc,
          VkImage                       image,
          GfxVulkanMemorySlice&&        memory);

  GfxVulkanImage(
          GfxVulkanDevice&              device,
    const GfxImageDesc&                 desc,
          VkImage                       image,
          VkBool32                      isConcurrent);

  ~GfxVulkanImage();

  /**
   * \brief Retrieves Vulkan image handle
   * \returns Vulkan image handle
   */
  VkImage getHandle() const {
    return m_image;
  }

  /**
   * \brief Queries stage and access mask for the image
   *
   * This may mask out some bits that the image can under no
   * circumstances be used with, e.g. depth-stencil bits for
   * color images and vice versa.
   * \returns Stage and access mask for the image
   */
  std::pair<VkPipelineStageFlags2, VkAccessFlags2> getStageAccessMasks() const {
    return std::make_pair(m_stageFlags, m_accessFlags);
  }

  /**
   * \brief Checks whether the image comes from a Vulkan swap chain
   *
   * Relevant when dealing with abstractions around presentation.
   * \returns \c true if the image is a swap chain image.
   */
  bool isSwapChainImage() const {
    // Currently, swap chain images are
    // the only source of external images
    return m_isExternal;
  }

  /**
   * \brief Picks image layout based on properties
   *
   * \param [in] layout Desired image layout
   * \returns Required image layout
   */
  VkImageLayout pickLayout(
          VkImageLayout                 layout) const {
    return m_isConcurrent ? VK_IMAGE_LAYOUT_GENERAL : layout;
  }

  /**
   * \brief Retrieves view with the given properties
   *
   * \param [in] desc View description
   * \returns View object
   */
  GfxImageView createView(
    const GfxImageViewDesc&             desc) override;

  /**
   * \brief Queries memory info for the resource
   * \returns Image memory allocation info
   */
  GfxMemoryInfo getMemoryInfo() const override;

private:

  GfxVulkanDevice&      m_device;
  GfxVulkanMemorySlice  m_memory;

  VkImage               m_image         = VK_NULL_HANDLE;
  VkBool32              m_isExternal    = VK_FALSE;
  VkBool32              m_isConcurrent  = VK_FALSE;

  VkPipelineStageFlags2 m_stageFlags    = 0;
  VkAccessFlags2        m_accessFlags   = 0;

  std::shared_mutex     m_viewLock;
  std::unordered_map<
    GfxImageViewDesc,
    GfxVulkanImageView,
    HashMemberProc>     m_viewMap;

  void setupStageAccessFlags();

};

}
