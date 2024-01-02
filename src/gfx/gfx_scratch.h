#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include "../alloc/alloc_bucket.h"
#include "../alloc/alloc_linear.h"

#include "../util/util_likely.h"
#include "../util/util_lock_free.h"

#include "gfx_buffer.h"

namespace as {

constexpr uint64_t GfxScratchPageSize = 1ull << 20;
constexpr uint64_t GfxScratchPageCount = 64ull;
constexpr uint64_t GfxScratchBufferSize = GfxScratchPageCount * GfxScratchPageSize;

class GfxDeviceIface;
class GfxScratchAllocator;

/**
 * \brief Scratch buffer slice allocated from a context
 */
struct GfxScratchBuffer {
  /** Buffer object that contains the allocation. The buffer
   *  is guaranteed to support the desied usage flags. */
  GfxBuffer buffer;
  /** Offset of the allocated region, in bytes */
  uint64_t offset = 0;
  /** Size of the allocated region, in bytes */
  uint64_t size = 0;

  /**
   * \brief Retrieves descriptor for the mapped slice
   *
   * Convenience method to retrieve a descriptor for the slice.
   * \param [in] usage Descriptor usage. Must be one of the bits
   *    that the slice has been allocated for alignment reasons.
   * \returns Descriptor handle for the given slice.
   */
  GfxDescriptor getDescriptor(GfxUsage usage) const {
    return buffer->getDescriptor(usage, offset, size);
  }

  /**
   * \brief Retrieves GPU address at given location
   *
   * Only useful if the buffer itself has a GPU address.
   * \returns GPU address of allocated slice
   */
  uint64_t getGpuAddress() const {
    return buffer->getGpuAddress() + offset;
  }

  /**
   * \brief Returns pointer to mapped memory region
   *
   * See \c GfxBufferIface::map.
   * \param [in] access CPU access flags
   * \param [in] offset Offset into the mapped resource
   * \returns Pointer to mapped buffer
   */
  void* map(GfxUsageFlags access, size_t offset) {
    return buffer->map(access, this->offset + offset);
  }

  /**
   * \brief Flushes mapped memory region
   *
   * See \c GfxBufferIface::unmap.
   * \param [in] access CPU access flags
   */
  void unmap(GfxUsageFlags access) {
    buffer->unmap(access);
  }

};


/**
 * \brief Scratch buffer page
 *
 * Provides a linear allocator for a small memory page
 * suballocated from a scratch buffer. Automatically
 * frees the page when the object runs out of scope.
 */
class GfxScratchBufferPage {

public:

  GfxScratchBufferPage(
          std::shared_ptr<GfxScratchAllocator> buffer,
          uint32_t                      pageIndex,
          uint32_t                      pageCount,
          GfxMemoryType                 memoryType);

  ~GfxScratchBufferPage();

  GfxScratchBufferPage             (GfxScratchBufferPage&& other);
  GfxScratchBufferPage& operator = (GfxScratchBufferPage&& other);

  /**
   * \brief Retrieves memory type
   * \returns Memory type
   */
  GfxMemoryType getMemoryType() const {
    return m_memoryType;
  }

  /**
   * \brief Allocates memory from the page
   *
   * \param [in] size Number of bytes to allocate
   * \param [in] alignment Required alignment
   * \returns Allocated memory range
   */
  std::optional<GfxScratchBuffer> alloc(
          uint64_t                      size,
          uint64_t                      alignment);

private:

  std::shared_ptr<GfxScratchAllocator> m_parent;

  uint32_t m_pageIndex = 0;
  uint32_t m_pageCount = 0;

  GfxMemoryType m_memoryType = GfxMemoryType::eFlagEnum;

  LinearAllocator<uint64_t> m_allocator;

};


/**
 * \brief Scratch buffer allocator
 *
 * Manages a single buffer allocation and a bucket
 * allocator that can be used to suballocate memory.
 */
class GfxScratchAllocator
: public std::enable_shared_from_this<GfxScratchAllocator> {
public:

  GfxScratchAllocator(
          GfxDeviceIface&               device,
          GfxMemoryType                 memoryType);

  ~GfxScratchAllocator();

  /**
   * \brief Retrieves buffer object
   * \returns Buffer object
   */
  GfxBuffer getBuffer() const {
    return m_buffer;
  }

  /**
   * \brief Retrieves memory type
   * \returns Memory type
   */
  GfxMemoryType getMemoryType() const {
    return m_memoryType;
  }

  /**
   * \brief Allocates pages
   *
   * \param [in] pageCount Number of pages to allocate
   * \returns Allocated page, or \c nullopt if the given
   *    number of pages could not be allocated.
   */
  std::optional<GfxScratchBufferPage> allocPages(
          uint32_t                      pageCount);

  /**
   * \brief Frees previously allocated pages
   *
   * \param [in] pageIndex Page to free
   * \param [in] pageCount Number of pages to free
   */
  void freePages(
          uint32_t                      pageIndex,
          uint32_t                      pageCount);

private:

  GfxBuffer                 m_buffer;
  GfxMemoryType             m_memoryType;
  BucketAllocator<uint64_t> m_allocator;

};


/**
 * \brief Scratch buffer pool
 *
 * Generic allocator for scratch buffers
 * that backends may use internally.
 */
class GfxScratchBufferPool {

public:

  GfxScratchBufferPool(
          GfxDeviceIface&               device);

  ~GfxScratchBufferPool();

  /**
   * \brief Allocates scratch memory pages
   *
   * \param [in] memoryType Memory type
   * \param [in] pageCount Number of pages to allocate
   * \returns Allocated scratch buffer pages
   */
  GfxScratchBufferPage allocPages(
          GfxMemoryType                 memoryType,
          uint32_t                      pageCount);

private:

  GfxDeviceIface& m_device;

  std::mutex                                         m_mutex;
  LockFreeList<std::shared_ptr<GfxScratchAllocator>> m_buffers;

  std::optional<GfxScratchBufferPage> tryAllocPages(
          GfxMemoryType                 memoryType,
          uint32_t                      pageCount);

};

}
