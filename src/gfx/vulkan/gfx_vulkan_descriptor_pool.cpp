#include "../../util/util_small_vector.h"

#include "gfx_vulkan_descriptor_pool.h"
#include "gfx_vulkan_device.h"

namespace as {

GfxVulkanDescriptorPool::GfxVulkanDescriptorPool(
        GfxVulkanDevice&              device)
: m_device(device) {
  auto& vk = m_device.vk();

  constexpr uint32_t maxSets = 8192;

  small_vector<VkDescriptorPoolSize, 16> types;
  types.push_back({ VK_DESCRIPTOR_TYPE_SAMPLER,               maxSets * 1 });
  types.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,        maxSets * 2 });
  types.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,        maxSets * 2 });
  types.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,  maxSets / 8 });
  types.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,  maxSets / 8 });
  types.push_back({ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,         maxSets * 1 });
  types.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,         maxSets * 1 });

  if (device.getVkFeatures().khrAccelerationStructure.accelerationStructure)
    types.push_back({ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, maxSets / 256 });

  VkDescriptorPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
  poolInfo.maxSets = maxSets;
  poolInfo.poolSizeCount = types.size();
  poolInfo.pPoolSizes = types.data();

  VkResult vr = vk.vkCreateDescriptorPool(vk.device, &poolInfo, nullptr, &m_pool);

  if (vr)
    throw VulkanError("Vulkan: Failed to create descriptor pool", vr);
}


GfxVulkanDescriptorPool::~GfxVulkanDescriptorPool() {
  auto& vk = m_device.vk();

  vk.vkDestroyDescriptorPool(vk.device, m_pool, nullptr);
}


bool GfxVulkanDescriptorPool::allocateSets(
        uint32_t                      setCount,
  const VkDescriptorSetLayout*        setLayouts,
        VkDescriptorSet*              sets) {
  auto& vk = m_device.vk();

  VkDescriptorSetAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
  allocateInfo.descriptorPool = m_pool;
  allocateInfo.descriptorSetCount = setCount;
  allocateInfo.pSetLayouts = setLayouts;

  VkResult vr = vk.vkAllocateDescriptorSets(vk.device, &allocateInfo, sets);

  if (vr && vr != VK_ERROR_OUT_OF_POOL_MEMORY && vr != VK_ERROR_FRAGMENTED_POOL)
    throw VulkanError("Vulkan: Failed to allocate descriptor sets", vr);

  return vr == VK_SUCCESS;
}


void GfxVulkanDescriptorPool::reset() {
  auto& vk = m_device.vk();

  VkResult vr = vk.vkResetDescriptorPool(vk.device, m_pool, 0);

  if (vr)
    throw VulkanError("Vulkan: Failed to reset descriptor pool", vr);
}




GfxVulkanDescriptorPoolManager::GfxVulkanDescriptorPoolManager(
        GfxVulkanDevice&              device)
: m_device(device) {

}


GfxVulkanDescriptorPoolManager::~GfxVulkanDescriptorPoolManager() {

}


std::shared_ptr<GfxVulkanDescriptorPool> GfxVulkanDescriptorPoolManager::getPool() {
  std::unique_lock lock(m_mutex);

  if (!m_pools.empty()) {
    std::shared_ptr<GfxVulkanDescriptorPool> result = std::move(m_pools.back());
    m_pools.pop_back();
    return result;
  }

  lock.unlock();

  // Create new pool on demand, but outside of the
  // locked context since this operation may be slow.
  return std::make_shared<GfxVulkanDescriptorPool>(m_device);
}


void GfxVulkanDescriptorPoolManager::recyclePool(
        std::shared_ptr<GfxVulkanDescriptorPool> pool) {
  pool->reset();

  std::lock_guard lock(m_mutex);
  m_pools.push_back(std::move(pool));
}

}
