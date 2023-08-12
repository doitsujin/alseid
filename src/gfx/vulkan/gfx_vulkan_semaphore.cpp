#include "gfx_vulkan_semaphore.h"

namespace as {

GfxVulkanSemaphore::GfxVulkanSemaphore(
        GfxVulkanDevice&              device,
  const GfxSemaphoreDesc&             desc,
        VkSemaphoreType               type)
: m_device(device) {
  auto& vk = m_device.vk();

  VkSemaphoreTypeCreateInfo timelineInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
  timelineInfo.semaphoreType = type;
  timelineInfo.initialValue = desc.initialValue;

  VkSemaphoreCreateInfo semaphoreInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };  

  if (type != VK_SEMAPHORE_TYPE_BINARY)
    semaphoreInfo.pNext = &timelineInfo;

  VkResult vr = vk.vkCreateSemaphore(vk.device,
    &semaphoreInfo, nullptr, &m_semaphore);

  if (vr)
    throw VulkanError("Vulkan: Failed to create semaphore", vr);

  m_device.setDebugName(m_semaphore, desc.debugName);
}


GfxVulkanSemaphore::~GfxVulkanSemaphore() {
  auto& vk = m_device.vk();

  vk.vkDestroySemaphore(vk.device, m_semaphore, nullptr);
}


uint64_t GfxVulkanSemaphore::getCurrentValue() {
  auto& vk = m_device.vk();

  uint64_t value = 0;
  VkResult vr = vk.vkGetSemaphoreCounterValue(vk.device, m_semaphore, &value);

  if (vr)
    throw VulkanError("Vulkan: Failed to query semaphore value", vr);

  return value;
}


bool GfxVulkanSemaphore::wait(
        uint64_t                      value,
        std::chrono::nanoseconds      timeout) {
  auto& vk = m_device.vk();

  // Pick optimized path for a timeout of zero
  if (timeout <= std::chrono::nanoseconds(0))
    return getCurrentValue() >= value;

  // Adjust timeout to what Vulkan will expect
  uint64_t timeoutNs = uint64_t(timeout.count());

  if (timeout == std::chrono::nanoseconds::max())
    timeoutNs = ~0ull;

  VkSemaphoreWaitInfo waitInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
  waitInfo.semaphoreCount = 1;
  waitInfo.pSemaphores = &m_semaphore;
  waitInfo.pValues = &value;

  VkResult vr = vk.vkWaitSemaphores(vk.device, &waitInfo, timeoutNs);

  if (vr < 0)
    throw VulkanError("Vulkan: Failed to wait for semaphore", vr);

  return vr == VK_SUCCESS;
}


void GfxVulkanSemaphore::signal(
        uint64_t                      value) {
  auto& vk = m_device.vk();

  VkSemaphoreSignalInfo signalInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO };
  signalInfo.semaphore = m_semaphore;
  signalInfo.value = value;

  VkResult vr = vk.vkSignalSemaphore(vk.device, &signalInfo);

  if (vr)
    throw VulkanError("Vulkan: Failed to wait for semaphore", vr);
}

}
