#include "../../util/util_math.h"

#include "../gfx.h"

#include "gfx_vulkan_buffer.h"
#include "gfx_vulkan_utils.h"

namespace as {

GfxVulkanBufferView::GfxVulkanBufferView(
        std::shared_ptr<GfxVulkanDevice> device,
  const GfxVulkanBuffer&              buffer,
  const GfxBufferViewDesc&            desc)
: GfxBufferViewIface(desc)
, m_device(std::move(device)) {

}


GfxVulkanBufferView::~GfxVulkanBufferView() {
  auto& vk = m_device->vk();

  vk.vkDestroyBufferView(vk.device, m_bufferView, nullptr);
}


GfxDescriptor GfxVulkanBufferView::getDescriptor() const {
  GfxVulkanDescriptor descriptor = { };
  descriptor.bufferView = m_bufferView;

  return exportVkDescriptor(descriptor);
}




GfxVulkanBuffer::GfxVulkanBuffer(
        std::shared_ptr<GfxVulkanDevice> device,
  const GfxBufferDesc&                desc,
        VkBuffer                      buffer,
        VkDeviceAddress               va,
        GfxVulkanMemorySlice&&        memory)
: GfxBufferIface(desc, va, memory.getMapPtr())
, m_device(std::move(device))
, m_memory(std::move(memory))
, m_buffer(buffer) {
  m_device->setDebugName(m_buffer, desc.debugName);
}


GfxVulkanBuffer::~GfxVulkanBuffer() {
  auto& vk = m_device->vk();

  vk.vkDestroyBuffer(vk.device, m_buffer, nullptr);
}


GfxBufferView GfxVulkanBuffer::createView(
  const GfxBufferViewDesc&            desc) {
  { std::shared_lock lock(m_viewLock);

    auto entry = m_viewMap.find(desc);
    if (entry != m_viewMap.end())
      return GfxBufferView(entry->second);
  }

  std::unique_lock lock(m_viewLock);

  auto result = m_viewMap.emplace(std::piecewise_construct,
    std::forward_as_tuple(desc),
    std::forward_as_tuple(m_device, *this, desc));

  return GfxBufferView(result.first->second);
}


GfxDescriptor GfxVulkanBuffer::getDescriptor(
        GfxUsage                      usage,
        uint64_t                      offset,
        uint64_t                      size) const {
  GfxVulkanDescriptor descriptor = { };
  descriptor.buffer.buffer = m_buffer;
  descriptor.buffer.offset = offset;
  descriptor.buffer.range = size;

  return exportVkDescriptor(descriptor);
}


GfxMemoryInfo GfxVulkanBuffer::getMemoryInfo() const {
  GfxMemoryInfo result;
  result.type = m_memory.getMemoryType();
  result.size = m_memory.getSize();
  return result;
}


void GfxVulkanBuffer::invalidateMappedRegion() {
  auto& vk = m_device->vk();

  VkMappedMemoryRange range = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
  range.memory = m_memory.getHandle();
  range.offset = m_memory.getOffset();
  range.size = m_desc.size;

  vk.vkInvalidateMappedMemoryRanges(vk.device, 1, &range);
}


void GfxVulkanBuffer::flushMappedRegion() {
  auto& vk = m_device->vk();

  VkMappedMemoryRange range = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
  range.memory = m_memory.getHandle();
  range.offset = m_memory.getOffset();
  range.size = m_desc.size;

  vk.vkFlushMappedMemoryRanges(vk.device, 1, &range);
}

}
