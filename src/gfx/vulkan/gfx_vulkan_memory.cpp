#include <algorithm>

#include "../../util/util_log.h"
#include "../../util/util_math.h"

#include "gfx_vulkan_device.h"
#include "gfx_vulkan_memory.h"

namespace as {

GfxVulkanMemorySlice::GfxVulkanMemorySlice() {

}


GfxVulkanMemorySlice::GfxVulkanMemorySlice(
        std::shared_ptr<GfxVulkanDevice> device,
        std::shared_ptr<GfxVulkanMemoryChunk> chunk,
        VkDeviceSize                  offset,
        VkDeviceSize                  size)
: m_device      (std::move(device))
, m_chunk       (std::move(chunk))
, m_memory      (m_chunk->getHandle())
, m_offset      (offset)
, m_size        (size)
, m_mapPtr      (m_chunk->getMapPtr(m_offset))
, m_typeId      (m_chunk->getMemoryTypeId())
, m_type        (m_chunk->getMemoryType()) {

}


GfxVulkanMemorySlice::GfxVulkanMemorySlice(
        std::shared_ptr<GfxVulkanDevice> device,
        VkDeviceMemory                memory,
        VkDeviceSize                  size,
        void*                         mapPtr,
        uint32_t                      typeId,
        GfxMemoryType                 type)
: m_device      (std::move(device))
, m_chunk       (nullptr)
, m_memory      (memory)
, m_offset      (0)
, m_size        (size)
, m_mapPtr      (mapPtr)
, m_typeId      (typeId)
, m_type        (type) {

}


GfxVulkanMemorySlice::GfxVulkanMemorySlice(GfxVulkanMemorySlice&& other)
: m_device      (std::move(other.m_device))
, m_chunk       (std::move(other.m_chunk))
, m_memory      (std::exchange(other.m_memory, VK_NULL_HANDLE))
, m_offset      (std::exchange(other.m_offset, 0))
, m_size        (std::exchange(other.m_size, 0))
, m_mapPtr      (std::exchange(other.m_mapPtr, nullptr))
, m_typeId      (std::exchange(other.m_typeId, 0u))
, m_type        (std::exchange(other.m_type, GfxMemoryType::eVideoMemory)) {

}


GfxVulkanMemorySlice& GfxVulkanMemorySlice::operator = (GfxVulkanMemorySlice&& other) {
  if (m_device)
    freeMemory();

  m_device    = std::move(other.m_device);
  m_chunk     = std::move(other.m_chunk);
  m_memory    = std::exchange(other.m_memory, VK_NULL_HANDLE);
  m_offset    = std::exchange(other.m_offset, 0);
  m_size      = std::exchange(other.m_size, 0);
  m_mapPtr    = std::exchange(other.m_mapPtr, nullptr);
  m_typeId    = std::exchange(other.m_typeId, 0u);
  m_type      = std::exchange(other.m_type, GfxMemoryType::eVideoMemory);
  return *this;
}


GfxVulkanMemorySlice::~GfxVulkanMemorySlice() {
  if (m_device)
    freeMemory();
}


void GfxVulkanMemorySlice::freeMemory() {
  m_device->getMemoryAllocator().freeMemory(m_chunk, *this);
}




GfxVulkanMemoryChunk::GfxVulkanMemoryChunk(
        GfxVulkanDevice&              device,
        VkDeviceMemory                memory,
        VkDeviceSize                  size,
        void*                         mapPtr,
        uint32_t                      typeId,
        GfxMemoryType                 type)
: m_device    (device)
, m_allocator (size)
, m_memory    (memory)
, m_mapPtr    (mapPtr)
, m_typeId    (typeId)
, m_type      (type) {

}


GfxVulkanMemoryChunk::~GfxVulkanMemoryChunk() {
  auto& vk = m_device.vk();

  vk.vkFreeMemory(vk.device, m_memory, nullptr);
}


bool GfxVulkanMemoryChunk::isEmpty() const {
  return m_allocator.isEmpty();
}


std::optional<VkDeviceSize> GfxVulkanMemoryChunk::allocRange(
      VkDeviceSize                    size,
      VkDeviceSize                    alignment) {
  return m_allocator.alloc(size, alignment);
}


void GfxVulkanMemoryChunk::freeRange(
      VkDeviceSize                    offset,
      VkDeviceSize                    size) {
  m_allocator.free(offset, size);
}


bool GfxVulkanMemoryChunk::checkCompatibility(
        uint32_t                      memoryTypeId,
        GfxMemoryType                 memoryType,
        GfxUsageFlags                 cpuAccess) const {
  if (m_typeId != memoryTypeId || m_type != memoryType)
    return false;

  // Don't put fallback sysmem allocations into mapped
  // chunks to reduce the amount of mapped memory
  bool hasCpuAccess = m_mapPtr != nullptr;
  bool needsCpuAccess = cpuAccess;

  return hasCpuAccess == needsCpuAccess;
}




GfxVulkanMemoryAllocator::GfxVulkanMemoryAllocator(
        GfxVulkanDevice&              device)
: m_device(device) {
  const auto& memoryProperties = m_device.getVkProperties().memory.memoryProperties;

  m_memoryHeapCount = memoryProperties.memoryHeapCount;
  m_memoryTypeCount = memoryProperties.memoryTypeCount;

  for (uint32_t i = 0; i < m_memoryHeapCount; i++)
    m_memoryHeaps[i].heap = memoryProperties.memoryHeaps[i];

  for (uint32_t i = 0; i < m_memoryTypeCount; i++) {
    m_memoryTypes[i].type = memoryProperties.memoryTypes[i];

    // Compute memory chunk size for each memory type
    VkDeviceSize heapSize = m_memoryHeaps[m_memoryTypes[i].type.heapIndex].heap.size;

    if (m_memoryTypes[i].type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
      m_memoryTypes[i].chunkSize = std::min(heapSize / 32, VkDeviceSize(64 << 20));
    else
      m_memoryTypes[i].chunkSize = std::min(heapSize / 16, VkDeviceSize(256 << 20));
  }
}


GfxVulkanMemoryAllocator::~GfxVulkanMemoryAllocator() {

}


GfxVulkanMemorySlice GfxVulkanMemoryAllocator::allocateMemory(
  const GfxVulkanMemoryRequirements&  requirements,
  const GfxVulkanMemoryAllocationInfo& properties) {
  std::lock_guard lock(m_mutex);

  for (auto memoryType : properties.memoryTypes) {
    uint32_t memoryTypeMask = requirements.core.memoryRequirements.memoryTypeBits
      & getMemoryTypeMask(memoryType, properties.cpuAccess);

    for (uint32_t i = memoryTypeMask; i; i &= i - 1) {
      uint32_t memoryTypeId = tzcnt(i);

      GfxVulkanMemorySlice memorySlice;

      if (requirements.dedicated.prefersDedicatedAllocation) {
        memorySlice = tryAllocateDedicatedMemoryFromType(
          memoryTypeId, memoryType, requirements, properties);
      }

      if (!memorySlice && !requirements.dedicated.requiresDedicatedAllocation) {
        memorySlice = tryAllocateChunkMemoryFromType(
          memoryTypeId, memoryType, requirements, properties);
      }

      if (memorySlice)
        return memorySlice;
    }
  }

  return GfxVulkanMemorySlice();
}


void GfxVulkanMemoryAllocator::freeMemory(
  const std::shared_ptr<GfxVulkanMemoryChunk>& chunk,
  const GfxVulkanMemorySlice&         slice) {
  auto& vk = m_device.vk();
  std::lock_guard lock(m_mutex);

  // Adjust memory allocation stats for the relevant memory type
  uint32_t heapIndex = m_memoryTypes[slice.getMemoryTypeId()].type.heapIndex;
  m_memoryHeaps[heapIndex].used -= slice.getSize();

  if (chunk) {
    chunk->freeRange(slice.getOffset(), slice.getSize());

    // Keep at most one empty device memory chunk of each kind
    // alive, or four system memory chunks.
    if (chunk->isEmpty()) {
      uint32_t maxEmptyChunks = m_memoryTypes[chunk->getMemoryTypeId()].type.propertyFlags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ? 1 : 4;
      uint32_t numEmptyChunks = 0;

      // Do not keep chunk alive at all if we're under memory pressure
      if (isHeapUnderPressure(heapIndex, 0))
        maxEmptyChunks = 0;

      auto entry = std::find(m_chunks.begin(), m_chunks.end(), chunk);

      if (entry != m_chunks.end())
        m_chunks.erase(entry);

      GfxUsageFlags cpuAccess = chunk->getMapPtr(0)
        ? GfxUsage::eCpuWrite | GfxUsage::eCpuRead
        : GfxUsage::eFlagEnum;

      if (maxEmptyChunks) {
        for (const auto& c : m_chunks) {
          if (c->checkCompatibility(chunk->getMemoryTypeId(), chunk->getMemoryType(), cpuAccess)
          && c->isEmpty())
            numEmptyChunks += 1;
        }
      }

      if (numEmptyChunks < maxEmptyChunks) {
        // Add empty chunks to the end so that they only get used if
        // necessary. This can reduce fragmentation and allows us to
        // destroy more chunks if needed.
        m_chunks.push_back(chunk);
      } else {
        // If the chunk gets destroyed, adjust stats
        m_memoryHeaps[heapIndex].allocated -= chunk->getSize();
      }
    }
  } else {
    vk.vkFreeMemory(vk.device, slice.getHandle(), nullptr);
    m_memoryHeaps[heapIndex].allocated -= slice.getSize();
  }
}


GfxVulkanMemorySlice GfxVulkanMemoryAllocator::tryAllocateDedicatedMemoryFromType(
        uint32_t                      memoryTypeId,
        GfxMemoryType                 memoryType,
  const GfxVulkanMemoryRequirements&  requirements,
  const GfxVulkanMemoryAllocationInfo& properties) {
  auto& vk = m_device.vk();

  uint32_t heapIndex = m_memoryTypes[memoryTypeId].type.heapIndex;
  this->freeEmptyChunks(heapIndex, requirements.core.memoryRequirements.size);

  VkMemoryAllocateFlagsInfo allocateFlags = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO };
  allocateFlags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

  VkMemoryAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, &allocateFlags };
  allocateInfo.allocationSize = requirements.core.memoryRequirements.size;
  allocateInfo.memoryTypeIndex = memoryTypeId;

  if (properties.dedicated.buffer || properties.dedicated.image)
    allocateFlags.pNext = &properties.dedicated;

  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkResult vr = vk.vkAllocateMemory(vk.device, &allocateInfo, nullptr, &memory);

  if (vr == VK_ERROR_OUT_OF_DEVICE_MEMORY || vr == VK_ERROR_OUT_OF_HOST_MEMORY)
    return GfxVulkanMemorySlice();

  if (vr != VK_SUCCESS)
    throw VulkanError("Vulkan: Failed to allocate memory", vr);

  void* mapPtr = nullptr;

  if (properties.cpuAccess) {
    vr = vk.vkMapMemory(vk.device, memory, 0, VK_WHOLE_SIZE, 0, &mapPtr);

    if (vr) {
      vk.vkFreeMemory(vk.device, memory, nullptr);
      throw VulkanError("Vulkan: Failed to map memory", vr);
    }
  }

  m_memoryHeaps[heapIndex].allocated += allocateInfo.allocationSize;
  m_memoryHeaps[heapIndex].used += allocateInfo.allocationSize;

  return GfxVulkanMemorySlice(m_device.shared_from_this(), memory,
    allocateInfo.allocationSize, mapPtr, memoryTypeId, memoryType);
}


GfxVulkanMemorySlice GfxVulkanMemoryAllocator::tryAllocateChunkMemoryFromType(
        uint32_t                      memoryTypeId,
        GfxMemoryType                 memoryType,
  const GfxVulkanMemoryRequirements&  requirements,
  const GfxVulkanMemoryAllocationInfo& properties) {
  // If the resource is almost as large as a chunk, use a dedicated allocation
  VkDeviceSize size = requirements.core.memoryRequirements.size;

  if (5 * size > 4 * m_memoryTypes[memoryTypeId].chunkSize) {
    GfxVulkanMemorySlice result = tryAllocateDedicatedMemoryFromType(memoryTypeId, memoryType, requirements, properties);

    if (result || size > m_memoryTypes[memoryTypeId].chunkSize)
      return result;
  }

  // Align all image resources to the buffer-image granularity. In practice,
  // this is hardly ever relevant since most current GPUs don't have a large
  // granularity value.
  VkDeviceSize alignment = requirements.core.memoryRequirements.alignment;

  if (properties.tiling == VK_IMAGE_TILING_OPTIMAL) {
    VkDeviceSize granularity = m_device.getVkProperties().core.properties.limits.bufferImageGranularity;

    alignment = align(alignment, granularity);
    size = align(size, alignment);
  }

  // Iterate over existing chunks and see if one can fit the allocation
  GfxVulkanMemorySlice result;

  for (const auto& chunk : m_chunks) {
    if (!chunk->checkCompatibility(memoryTypeId, memoryType, properties.cpuAccess))
      continue;

    auto offset = chunk->allocRange(size, alignment);

    if (offset) {
      result = GfxVulkanMemorySlice(m_device.shared_from_this(), chunk, *offset, size);
      break;
    }
  }

  // Try to allocate a new chunk on the given memory type
  if (!result) {
    auto chunk = tryCreateChunk(memoryTypeId, memoryType, properties.cpuAccess);

    if (!chunk)
      return GfxVulkanMemorySlice();

    // Allocate resource from newly created chunk. This is guaranteed
    // to succeed on empty chunks since the resource has to be smaller.
    m_chunks.push_back(chunk);

    auto offset = chunk->allocRange(size, alignment);
    result = GfxVulkanMemorySlice(m_device.shared_from_this(), std::move(chunk), *offset, size);
  }

  uint32_t heapIndex = m_memoryTypes[memoryTypeId].type.heapIndex;
  m_memoryHeaps[heapIndex].used += result.getSize();
  return result;
}


std::shared_ptr<GfxVulkanMemoryChunk> GfxVulkanMemoryAllocator::tryCreateChunk(
        uint32_t                      memoryTypeId,
        GfxMemoryType                 memoryType,
        GfxUsageFlags                 cpuAccess) {
  auto& vk = m_device.vk();

  uint32_t heapIndex = m_memoryTypes[memoryTypeId].type.heapIndex;
  this->freeEmptyChunks(heapIndex, m_memoryTypes[memoryTypeId].chunkSize);

  VkMemoryAllocateFlagsInfo allocateFlags = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO };
  allocateFlags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

  VkMemoryAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, &allocateFlags };
  allocateInfo.allocationSize = m_memoryTypes[memoryTypeId].chunkSize;
  allocateInfo.memoryTypeIndex = memoryTypeId;

  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkResult vr = vk.vkAllocateMemory(vk.device, &allocateInfo, nullptr, &memory);

  if (vr == VK_ERROR_OUT_OF_HOST_MEMORY || vr == VK_ERROR_OUT_OF_DEVICE_MEMORY)
    return nullptr;

  if (vr)
    throw VulkanError("Vulkan: Failed to allocate chunk memory", vr);

  // Map chunk as necessary
  void* mapPtr = nullptr;

  if (cpuAccess) {
    vr = vk.vkMapMemory(vk.device, memory, 0, VK_WHOLE_SIZE, 0, &mapPtr);

    if (vr) {
      vk.vkFreeMemory(vk.device, memory, nullptr);
      throw VulkanError("Vulkan: Failed to map memory", vr);
    }
  }

  m_memoryHeaps[heapIndex].allocated += allocateInfo.allocationSize;

  return std::make_shared<GfxVulkanMemoryChunk>(m_device, memory,
    allocateInfo.allocationSize, mapPtr, memoryTypeId, memoryType);
}


void GfxVulkanMemoryAllocator::freeEmptyChunks(
        uint32_t                      heapIndex,
        VkDeviceSize                  allocationSize) {
  // Remove as many chunks as necessary to get memory usage below 80%
  // of the heap size, already accounting for a new allocation.
  for (auto i = m_chunks.begin(); i != m_chunks.end(); ) {
    if (!isHeapUnderPressure(heapIndex, allocationSize))
      return;

    bool doRemove = false;

    if ((*i)->isEmpty()) {
      uint32_t typeIndex = (*i)->getMemoryTypeId();
      doRemove = heapIndex == m_memoryTypes[typeIndex].type.heapIndex;
    }    

    if (doRemove) {
      m_memoryHeaps[heapIndex].allocated -= (*i)->getSize();
      i = m_chunks.erase(i);
    } else {
      i++;
    }
  }
}


bool GfxVulkanMemoryAllocator::isHeapUnderPressure(
        uint32_t                      heapIndex,
        VkDeviceSize                  allocationSize) const {
  return 5 * (m_memoryHeaps[heapIndex].allocated + allocationSize) > 4 * m_memoryHeaps[heapIndex].heap.size;
}


uint32_t GfxVulkanMemoryAllocator::getMemoryTypeMask(
        GfxMemoryTypes                typeFlags,
        GfxUsageFlags                 cpuAccess) const {
  const auto memoryTypeMasks = m_device.getMemoryTypeInfo();

  // Work out required memory properties
  VkMemoryPropertyFlags requiredProperties = 0;

  if (!(typeFlags & GfxMemoryType::eSystemMemory))
    requiredProperties |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

  if (cpuAccess & GfxUsage::eCpuRead) {
    requiredProperties |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                       |  VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
  } else if (cpuAccess & GfxUsage::eCpuWrite) {
    requiredProperties |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                       |  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  }

  // Compute memory type mask
  uint32_t compatibleMask = 0u;
  uint32_t allowedMask = 0u;

  for (uint32_t i = 0; i < m_memoryTypeCount; i++) {
    if ((m_memoryTypes[i].type.propertyFlags & requiredProperties) == requiredProperties)
      compatibleMask |= 1u << i;
  }

  if ((typeFlags & GfxMemoryType::eVideoMemory) && !cpuAccess)
    allowedMask |= memoryTypeMasks.vidMem;

  if ((typeFlags & GfxMemoryType::eBarMemory) && cpuAccess)
    allowedMask |= memoryTypeMasks.barMem;

  if (typeFlags & GfxMemoryType::eSystemMemory)
    allowedMask |= memoryTypeMasks.sysMem;

  return compatibleMask & allowedMask;
}

}
