#pragma once

#include <cstddef>
#include <cstdint>

namespace as {

/**
 * \brief Graphics backend
 */
enum GfxBackend : uint32_t {
  /** Platform-specific default */
  eDefault          = 0,
  /** Vulkan */
  eVulkan           = 1,
};

}
