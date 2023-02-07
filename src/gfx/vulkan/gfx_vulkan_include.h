#pragma once

#define VK_NO_PROTOTYPES 1
#include <vulkan/vulkan.h>

#include "../../util/util_error.h"

namespace as {

class VulkanError : public Error {

public:

  VulkanError(const char* msg, VkResult vr) noexcept {
    std::snprintf(m_message, sizeof(m_message), "%s: %d", msg, uint32_t(vr));
  }

};

}
