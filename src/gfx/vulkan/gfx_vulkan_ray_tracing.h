#pragma once

#include <memory>
#include <vector>

#include "../gfx_ray_tracing.h"

#include "gfx_vulkan_descriptor_handle.h"
#include "gfx_vulkan_loader.h"
#include "gfx_vulkan_memory.h"

namespace as {

class GfxVulkanDevice;

/**
 * \brief Vulkan ray tracing acceleration structure info
 *
 * Helper class to create and contain the ray tracing
 * acceleration structure build info and geometry infos.
 */
struct GfxVulkanRayTracingBvhInfo {
  GfxVulkanRayTracingBvhInfo() = default;

  GfxVulkanRayTracingBvhInfo(
    const GfxVulkanDevice&              device,
    const GfxRayTracingGeometryDesc&    desc);

  GfxVulkanRayTracingBvhInfo(
    const GfxVulkanDevice&              device,
    const GfxRayTracingInstanceDesc&    desc);

  GfxVulkanRayTracingBvhInfo(GfxVulkanRayTracingBvhInfo&& other);
  GfxVulkanRayTracingBvhInfo(const GfxVulkanRayTracingBvhInfo& other);

  GfxVulkanRayTracingBvhInfo& operator = (GfxVulkanRayTracingBvhInfo&& other);
  GfxVulkanRayTracingBvhInfo& operator = (const GfxVulkanRayTracingBvhInfo&& other);

  VkAccelerationStructureBuildGeometryInfoKHR info = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
  std::vector<VkAccelerationStructureGeometryKHR> geometries;
  std::vector<VkAccelerationStructureBuildRangeInfoKHR> rangeInfos;

private:

  void fixupPointers();

};


/**
 * \brief BVH size info
 */
struct GfxVulkanRayTracingBvhSize {
  VkDeviceSize allocationSize;
  VkDeviceSize scratchSizeForUpdate;
  VkDeviceSize scratchSizeForBuild;
};


/**
 * \brief Vulkan ray tracing acceleration structure
 */
class GfxVulkanRayTracingBvh : public GfxRayTracingBvhIface {

public:

  GfxVulkanRayTracingBvh(
          GfxVulkanDevice&              device,
    const GfxRayTracingBvhDesc&         desc,
          GfxVulkanRayTracingBvhInfo&&  info,
    const GfxVulkanRayTracingBvhSize&   size,
          VkBuffer                      buffer,
          VkAccelerationStructureKHR    rtas,
          VkDeviceAddress               va,
          GfxVulkanMemorySlice&&        memory);

  ~GfxVulkanRayTracingBvh();

  /**
   * \brief Retrieves acceleration structure handle
   * \returns Acceleration structure handle
   */
  VkAccelerationStructureKHR getHandle() const {
    return m_rtas;
  }

  /**
   * \brief Queries scratch memory size
   *
   * \param [in] mode Build mode
   * \returns Required scratch size
   */
  uint64_t getScratchSize(
        GfxRayTracingBvhBuildMode     mode) const;

  /**
   * \brief Retrieves build info
   *
   * Copies the build info and populates
   * it with the given parameters.
   * \param [in] mode Build mode
   * \param [in] data Data sources
   * \param [in] scratch Scratch memory GPU address
   * \returns Build info
   */
  GfxVulkanRayTracingBvhInfo getBuildInfo(
        GfxRayTracingBvhBuildMode     mode,
  const GfxRayTracingBvhData*         data,
        uint64_t                      scratch) const;

  /**
   * \brief Retrieves BVH descriptor
   * \returns BVH descriptor
   */
  GfxDescriptor getDescriptor() const override;

private:

  GfxVulkanDevice&            m_device;
  GfxVulkanRayTracingBvhInfo  m_info;
  GfxVulkanRayTracingBvhSize  m_size;
  GfxVulkanMemorySlice        m_memory;
  VkBuffer                    m_buffer  = VK_NULL_HANDLE;
  VkAccelerationStructureKHR  m_rtas    = VK_NULL_HANDLE;

};

}
