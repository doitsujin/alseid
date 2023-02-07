#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include "gfx_vulkan_loader.h"

namespace as {

class GfxVulkanDevice;

/**
 * \brief Vulkan descriptor pool
 */
class GfxVulkanDescriptorPool {

public:

  GfxVulkanDescriptorPool(
          GfxVulkanDevice&              device);

  ~GfxVulkanDescriptorPool();

  /**
   * \brief Allocates sets with given layouts
   *
   * \param [in] setCount 
   * \returns \c true on success
   */
  bool allocateSets(
          uint32_t                      setCount,
    const VkDescriptorSetLayout*        setLayouts,
          VkDescriptorSet*              sets);

  /**
   * \brief Resets descriptor pool
   */
  void reset();

private:

  GfxVulkanDevice&  m_device;
  VkDescriptorPool  m_pool    = VK_NULL_HANDLE;

};


/**
 * \brief Vulkan descriptor pool manager
 */
class GfxVulkanDescriptorPoolManager {

public:

  GfxVulkanDescriptorPoolManager(
          GfxVulkanDevice&              device);

  ~GfxVulkanDescriptorPoolManager();

  /**
   * \brief Gets or creates a descriptor pool
   *
   * The pool will be in a reset state.
   * \returns Descriptor pool
   */
  std::shared_ptr<GfxVulkanDescriptorPool> getPool();

  /**
   * \brief Recycles a descriptor pool
   * \param [in] pool Descriptor pool
   */
  void recyclePool(
          std::shared_ptr<GfxVulkanDescriptorPool> pool);

private:

  GfxVulkanDevice&  m_device;

  std::mutex        m_mutex;
  std::vector<std::shared_ptr<GfxVulkanDescriptorPool>> m_pools;

};

}
