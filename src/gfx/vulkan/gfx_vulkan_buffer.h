#pragma once

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

#include "../gfx_buffer.h"

#include "gfx_vulkan_descriptor_handle.h"
#include "gfx_vulkan_device.h"
#include "gfx_vulkan_loader.h"
#include "gfx_vulkan_memory.h"

namespace as {

class GfxVulkanBuffer;

/**
 * \brief Vulkan buffer view
 */
class GfxVulkanBufferView : public GfxBufferViewIface {

public:

  GfxVulkanBufferView(
          GfxVulkanDevice&              device,
    const GfxVulkanBuffer&              buffer,
    const GfxBufferViewDesc&            desc);

  ~GfxVulkanBufferView();

  /**
   * \brief Retrieves buffer view
   * \returns Buffer view
   */
  VkBufferView getHandle() const {
    return m_bufferView;
  }

  /**
   * \brief Retrieves buffer view descriptor
   * \returns View descriptor
   */
  GfxDescriptor getDescriptor() const override;

protected:

  GfxVulkanDevice& m_device;

  VkBufferView m_bufferView = VK_NULL_HANDLE;

};


/**
 * \brief Buffer resource interface
 */
class GfxVulkanBuffer : public GfxBufferIface {

public:

  GfxVulkanBuffer(
          GfxVulkanDevice&              device,
    const GfxBufferDesc&                desc,
          VkBuffer                      buffer,
          VkDeviceAddress               va,
          GfxVulkanMemorySlice&&        memory);

  ~GfxVulkanBuffer();

  /**
   * \brief Retrieves buffer handle
   * \returns Buffer handle
   */
  VkBuffer getHandle() const {
    return m_buffer;
  }

  /**
   * \brief Retrieves view with the given properties
   *
   * \param [in] desc View description
   * \returns View object
   */
  GfxBufferView createView(
    const GfxBufferViewDesc&            desc) override;

  /**
   * \brief Retrieves buffer descriptor
   *
   * \param [in] usage Buffer descriptor usage
   * \param [in] offset Buffer usage
   * \param [in] size Buffer size
   * \returns Buffer descriptor
   */
  GfxDescriptor getDescriptor(
          GfxUsage                      usage,
          uint64_t                      offset,
          uint64_t                      size) const override;

  /**
   * \brief Queries memory info for the resource
   * \returns Buffer memory allocation info
   */
  GfxMemoryInfo getMemoryInfo() const override;

protected:

  GfxVulkanDevice&      m_device;

  GfxVulkanMemorySlice  m_memory;
  VkBuffer              m_buffer = VK_NULL_HANDLE;

  std::shared_mutex     m_viewLock;
  std::unordered_map<
    GfxBufferViewDesc,
    GfxVulkanBufferView,
    HashMemberProc>     m_viewMap;

  void invalidateMappedRegion() override;

  void flushMappedRegion() override;

};

}
