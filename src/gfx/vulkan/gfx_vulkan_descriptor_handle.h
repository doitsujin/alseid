#pragma once

#include <cstring>

#include "../gfx_descriptor_handle.h"

#include "gfx_vulkan_include.h"

namespace as {

/**
 * \brief Vulkan descriptor info
 *
 * Contains all the information required to populate
 * a Vulkan descriptor with a given descriptor type.
 */
union GfxVulkanDescriptor {
  VkDescriptorBufferInfo      buffer;
  VkDescriptorImageInfo       image;
  VkBufferView                bufferView;
  VkAccelerationStructureKHR  bvh;
};

static_assert(GfxDescriptorSize >= sizeof(GfxVulkanDescriptor));


/**
 * \brief Extracts Vulkan descriptor from common descriptor
 *
 * \param [in] descriptor Common descriptor object
 * \returns Vulkan descriptor
 */
inline GfxVulkanDescriptor importVkDescriptor(
  const GfxDescriptor&                  descriptor) {
  GfxVulkanDescriptor result;
  std::memcpy(&result, descriptor.data, sizeof(result));
  return result;
}


/**
 * \brief Packs Vulkan descriptor into common descriptor
 *
 * \param [in] descriptor Common descriptor object
 * \returns Vulkan descriptor
 */
inline GfxDescriptor exportVkDescriptor(
  const GfxVulkanDescriptor&            descriptor) {
  GfxDescriptor result = { };
  std::memcpy(result.data, &descriptor, sizeof(descriptor));
  return result;
}

}
