#pragma once

#include <unordered_map>

#include "../gfx_format.h"

#include "gfx_vulkan_include.h"

namespace as {

/**
 * \brief Vulkan format map
 *
 * Maps each common format to a Vulkan format.
 */
class GfxVulkanFormatMap {

public:

  GfxVulkanFormatMap();

  /**
   * \brief Queries Vulkan format
   *
   * \param [in] format Common format
   * \returns Corresponding Vulkan format
   */
  VkFormat getVkFormat(GfxFormat format) const {
    return m_gfxToVkFormats[uint32_t(format)];
  }

  /**
   * \brief Queries common format
   *
   * \param [in] format Vulkan format
   * \returns Corresponding common format
   */
  GfxFormat getGfxFormat(VkFormat format) const;

private:

  std::array<VkFormat, uint32_t(GfxFormat::eCount)> m_gfxToVkFormats;

  std::unordered_map<VkFormat, GfxFormat> m_vkToGfxFormats;

  void map(
          GfxFormat                     gfxFormat,
          VkFormat                      vkFormat);

};

}
