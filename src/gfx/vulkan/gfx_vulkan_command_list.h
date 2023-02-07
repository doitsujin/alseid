#pragma once

#include "../gfx_command_list.h"

#include "gfx_vulkan_loader.h"

namespace as {

/**
 * \brief Vulkan command list
 *
 * Basically just a thin wrapper around a Vulkan
 * command buffer that doesn't do anything on its
 * own. Apps must make sure to not misuse this.
 */
class GfxVulkanCommandList : public GfxCommandListIface {

public:

  /**
   * \brief Initializes command list
   * \param [in] handle Vulkan commadn buffer
   */
  GfxVulkanCommandList(
          VkCommandBuffer               handle);

  ~GfxVulkanCommandList();

  /**
   * \brief Queries Vulkan command buffer
   * \returns Command buffer handle
   */
  VkCommandBuffer getHandle() const {
    return m_handle;
  }

private:

  VkCommandBuffer m_handle;

};

}
