#include "gfx_vulkan_command_list.h"

namespace as {

GfxVulkanCommandList::GfxVulkanCommandList(
        VkCommandBuffer               handle)
: m_handle(handle) {

}


GfxVulkanCommandList::~GfxVulkanCommandList() {

}

}
