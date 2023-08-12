#pragma once

#include "../gfx_semaphore.h"

#include "gfx_vulkan_device.h"

namespace as {

/**
 * \brief Vulkan semaphore
 */
class GfxVulkanSemaphore : public GfxSemaphoreIface {

public:

  GfxVulkanSemaphore(
          GfxVulkanDevice&              device,
    const GfxSemaphoreDesc&             desc,
          VkSemaphoreType               type);

  ~GfxVulkanSemaphore();

  /**
   * \brief Retrieves Vulkan handle of the semaphore
   * \returns Vulkan semaphore handle
   */
  VkSemaphore getHandle() const {
    return m_semaphore;
  }

  /**
   * \brief Queries current semaphore value
   * \returns Current semaphore value
   */
  uint64_t getCurrentValue() override;

  /**
   * \brief Waits for semaphore to reach the given value
   *
   * \param [in] value Desired semaphore value
   * \param [in] timeout Timeout, in nanoseconds
   * \returns \c true if the semaphore reached \c value
   */
  bool wait(
          uint64_t                      value,
          std::chrono::nanoseconds      timeout) override;

  /**
   * \brief Signals semaphore to given value
   * \param [in] value Desired semaphore value
   */
  void signal(
          uint64_t                      value) override;

private:

  GfxVulkanDevice&                  m_device;
  VkSemaphore                       m_semaphore = VK_NULL_HANDLE;

};

}
