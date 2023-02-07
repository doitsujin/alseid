#pragma once

#include <array>
#include <memory>
#include <mutex>
#include <vector>

#include "../../alloc/alloc_chunk.h"

#include "../gfx_memory.h"
#include "../gfx_types.h"

#include "gfx_vulkan_loader.h"

namespace as {

class GfxVulkanDevice;
class GfxVulkanMemoryChunk;
class GfxVulkanMemoryAllocator;

/**
 * \brief Vulkan memory requirement info
 */
struct GfxVulkanMemoryRequirements {
  VkMemoryDedicatedRequirements dedicated;
  VkMemoryRequirements2 core;
};


/**
 * \brief Vulkan allocation properties
 */
struct GfxVulkanMemoryAllocationInfo {
  VkMemoryDedicatedAllocateInfo dedicated;
  VkImageTiling tiling;
  GfxMemoryTypes memoryTypes;
  GfxUsageFlags cpuAccess;
};


/**
 * \brief Vulkan memory type masks
 */
struct GfxVulkanMemoryTypeMasks {
  uint32_t vidMem = 0;
  uint32_t barMem = 0;
  uint32_t sysMem = 0;
};


/**
 * \brief Vulkan memory heap info
 */
struct GfxVulkanMemoryHeap {
  VkMemoryHeap heap;
  VkDeviceSize used;
  VkDeviceSize allocated;
};


/**
 * \brief Vulkan memory type info
 */
struct GfxVulkanMemoryType {
  VkMemoryType type;
  VkDeviceSize chunkSize;
};


/**
 * \brief Memory slice
 *
 * A slice of memory returned from a memory allocator.
 * Automatically returns the slice to the allocator
 * when the object runs out of scope.
 */
class GfxVulkanMemorySlice {

public:

  /**
   * \brief Initializes empty slice
   */
  GfxVulkanMemorySlice();

  /**
   * \brief Initializes slice
   *
   * \param [in] allocator Memory allocator
   * \param [in] chunk Memory chunk
   * \param [in] offset Offset into the given chunk
   * \param [in] size Size of the memory slice
   */
  GfxVulkanMemorySlice(
          std::shared_ptr<GfxVulkanDevice> device,
          std::shared_ptr<GfxVulkanMemoryChunk> chunk,
          VkDeviceSize                  offset,
          VkDeviceSize                  size);

  /**
   * \brief Initializes dedicated slice
   *
   * \param [in] allocator Memory allocator
   * \param [in] memory Vulkan device memory
   * \param [in] size Size of the memory slice
   * \param [in] mapPtr Map pointer
   * \param [in] typeId Vulkan memory type ID
   * \param [in] type Memory type
   */
  GfxVulkanMemorySlice(
          std::shared_ptr<GfxVulkanDevice> device,
          VkDeviceMemory                memory,
          VkDeviceSize                  size,
          void*                         mapPtr,
          uint32_t                      typeId,
          GfxMemoryType                 type);

  ~GfxVulkanMemorySlice();

  GfxVulkanMemorySlice              (GfxVulkanMemorySlice&& other);
  GfxVulkanMemorySlice& operator =  (GfxVulkanMemorySlice&& other);

  /**
   * \brief Queries device memory handle
   * \returns Vulkan memory handle
   */
  VkDeviceMemory getHandle() const {
    return m_memory;
  }

  /**
   * \brief Queries memory offset
   * \returns Offset into the Vulkan allocation
   */
  VkDeviceSize getOffset() const {
    return m_offset;
  }

  /**
   * \brief Queries memory size
   * \returns Memory slice size
   */
  VkDeviceSize getSize() const {
    return m_size;
  }

  /**
   * \brief Queries memory type
   * \returns Memory type
   */
  GfxMemoryType getMemoryType() const {
    return m_type;
  }

  /**
   * \brief Queries memory type index
   * \returns Vulkan memory type index
   */
  uint32_t getMemoryTypeId() const {
    return m_typeId;
  }

  /**
   * \brief Queries CPU pointer
   *
   * Will return \c nullptr if the allocation is not mapped.
   * \returns CPU pointer
   */
  void* getMapPtr() const {
    return m_mapPtr;
  }

  /**
   * \brief Checks whether the slice is valid
   * \returns \c true if the slice is backed by memory
   */
  operator bool () const {
    return m_memory != VK_NULL_HANDLE;
  }

private:

  std::shared_ptr<GfxVulkanDevice>      m_device;
  std::shared_ptr<GfxVulkanMemoryChunk> m_chunk;

  VkDeviceMemory  m_memory  = VK_NULL_HANDLE;
  VkDeviceSize    m_offset  = 0;
  VkDeviceSize    m_size    = 0;
  void*           m_mapPtr  = nullptr;

  uint32_t        m_typeId  = 0;
  GfxMemoryType   m_type    = GfxMemoryType::eVideoMemory;

  void freeMemory();

};


/**
 * \brief Memory chunk
 */
class GfxVulkanMemoryChunk {

public:

  GfxVulkanMemoryChunk(
          GfxVulkanDevice&              device,
          VkDeviceMemory                memory,
          VkDeviceSize                  size,
          void*                         mapPtr,
          uint32_t                      typeId,
          GfxMemoryType                 type);

  ~GfxVulkanMemoryChunk();

  /**
   * \brief Queries device memory handle
   * \returns Vulkan memory handle
   */
  VkDeviceMemory getHandle() const {
    return m_memory;
  }

  /**
   * \brief Queries chunk size
   * \returns Memory chunk size
   */
  VkDeviceSize getSize() const {
    return m_allocator.capacity();
  }

  /**
   * \brief Queries Vulkan memory type ID
   * \returns Vulkan memory type
   */
  uint32_t getMemoryTypeId() const {
    return m_typeId;
  }

  /**
   * \brief Queries memory type
   * \returns Memory type
   */
  GfxMemoryType getMemoryType() const {
    return m_type;
  }

  /**
   * \brief Queries CPU pointer
   *
   * \param [in] offset Offset to add
   * \returns CPU pointer
   */
  void* getMapPtr(size_t offset) const {
    if (!m_mapPtr)
      return nullptr;

    return reinterpret_cast<char*>(m_mapPtr) + offset;
  }

  /**
   * \brief Checks whether the chunk is empty
   * \returns \c true if the chunk is empty
   */
  bool isEmpty() const;

  /**
   * \brief Allocates memory range
   *
   * \param [in] size Allocation size
   * \param [in] alignment Alignment
   * \returns Offset of allocated range
   */
  std::optional<VkDeviceSize> allocRange(
        VkDeviceSize                    size,
        VkDeviceSize                    alignment);

  /**
   * \brief Frees memory slice
   *
   * \param [in] offset Offset of the slice to free
   * \param [in] size Number of bytes to free
   */
  void freeRange(
        VkDeviceSize                    offset,
        VkDeviceSize                    size);

  /**
   * \brief Checks compatibility with an allocation
   *
   * \param [in] memoryTypeId Memory type index
   * \param [in] memoryType Memory type
   * \param [in] cpuAccess CPU access flags
   */
  bool checkCompatibility(
          uint32_t                      memoryTypeId,
          GfxMemoryType                 memoryType,
          GfxUsageFlags                 cpuAccess) const;

private:

  GfxVulkanDevice&              m_device;

  std::mutex                    m_mutex;
  ChunkAllocator<VkDeviceSize>  m_allocator;

  VkDeviceMemory  m_memory  = VK_NULL_HANDLE;
  void*           m_mapPtr  = nullptr;

  uint32_t        m_typeId  = 0;
  GfxMemoryType   m_type    = GfxMemoryType(0);

};


/**
 * \brief Vulkan memory allocator
 */
class GfxVulkanMemoryAllocator {

public:

  GfxVulkanMemoryAllocator(
          GfxVulkanDevice&              device);

  ~GfxVulkanMemoryAllocator();

  /**
   * \brief Allocates memory
   *
   * \param [in] requirements Memory requirements
   * \param [in] properties Memory properties
   * \returns Allocated memory slice
   */
  GfxVulkanMemorySlice allocateMemory(
    const GfxVulkanMemoryRequirements&  requirements,
    const GfxVulkanMemoryAllocationInfo& properties);

  /**
   * \brief Frees memory slice
   *
   * \param [in] chunk Memory chunk that owns the slice
   * \param [in] slice Memory slice object
   */
  void freeMemory(
    const std::shared_ptr<GfxVulkanMemoryChunk>& chunk,
    const GfxVulkanMemorySlice&         slice);

private:

  GfxVulkanDevice&  m_device;

  std::array<GfxVulkanMemoryHeap, VK_MAX_MEMORY_HEAPS> m_memoryHeaps = { };
  std::array<GfxVulkanMemoryType, VK_MAX_MEMORY_TYPES> m_memoryTypes = { };

  uint32_t m_memoryHeapCount = 0;
  uint32_t m_memoryTypeCount = 0;

  std::mutex                                          m_mutex;
  std::vector<std::shared_ptr<GfxVulkanMemoryChunk>>  m_chunks;

  GfxVulkanMemorySlice tryAllocateDedicatedMemoryFromType(
          uint32_t                      memoryTypeId,
          GfxMemoryType                 memoryType,
    const GfxVulkanMemoryRequirements&  requirements,
    const GfxVulkanMemoryAllocationInfo& properties);

  GfxVulkanMemorySlice tryAllocateChunkMemoryFromType(
          uint32_t                      memoryTypeId,
          GfxMemoryType                 memoryType,
    const GfxVulkanMemoryRequirements&  requirements,
    const GfxVulkanMemoryAllocationInfo& properties);

  std::shared_ptr<GfxVulkanMemoryChunk> tryCreateChunk(
          uint32_t                      memoryTypeId,
          GfxMemoryType                 memoryType,
          GfxUsageFlags                 cpuAccess);

  void freeEmptyChunks(
          uint32_t                      heapIndex,
          VkDeviceSize                  allocationSize);

  bool isHeapUnderPressure(
          uint32_t                      heapIndex,
          VkDeviceSize                  allocationSize) const;

  uint32_t getMemoryTypeMask(
          GfxMemoryTypes                typeFlags,
          GfxUsageFlags                 cpuAccess) const;

};

}
