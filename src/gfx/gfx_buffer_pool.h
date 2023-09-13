#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "gfx_buffer.h"
#include "gfx_device.h"

#include "../alloc/alloc_chunk.h"

namespace as {

/**
 * \brief Buffer slice
 *
 * Stores a buffer range allocated from a buffer pool.
 */
struct GfxBufferSlice {
  /** Buffer object */
  GfxBuffer buffer = nullptr;
  /** Offset within the buffer */
  uint64_t offset = 0;
  /** Size of the buffer slice, in bytes */
  uint64_t size = 0;
};


/**
 * \brief Buffer pool chunk
 *
 * Stores a buffer as well as a chunk allocator
 * to allocate memory from that buffer.
 */
struct GfxBufferPoolChunk {
  /** Buffer object */
  GfxBuffer buffer;
  /** Chunk allocator object */
  ChunkAllocator<uint64_t> allocator;
};


/**
 * \brief Buffer pool allocation stats
 */
struct GfxBufferPoolStats {
  /** Amount of memory allocated */
  uint64_t memoryAllocated = 0;
  /** Amount of memory actually in use */
  uint64_t memoryUsed = 0;
};


/**
 * \brief Buffer pool
 *
 * Provides a convenient way to suballocate memory from larger
 * buffers with desired properties. Buffer objects are created
 * and destroyed on demand.
 */
class GfxBufferPool {

public:

  /**
   * \brief Initializes buffer pool
   *
   * \param [in] device Device object
   * \param [in] desc Buffer description for each individual data
   *    buffer. The buffer size is the minimum size of each buffer
   *    to create for suballocation purposes.
   * \param [in] memoryTypes Desired buffer memory types
   */
  GfxBufferPool(
          GfxDevice                     device,
    const GfxBufferDesc&                desc,
          GfxMemoryTypes                memoryTypes);

  ~GfxBufferPool();

  GfxBufferPool             (const GfxBufferPool&) = delete;
  GfxBufferPool& operator = (const GfxBufferPool&) = delete;

  /**
   * \brief Queries allocation statistics
   * \returns Allocation statistics
   */
  GfxBufferPoolStats getStats();

  /**
   * \brief Allocates memory from the pool
   *
   * This is designed to always succeed unless it is impossible to
   * allocate any more device memory with the desired properties.
   * \param [in] size Requested buffer size, in bytes
   * \param [in] alignment Required alignment, in bytes
   * \returns Allocated buffer slice
   */
  GfxBufferSlice alloc(
          uint64_t                      size,
          uint64_t                      alignment);

  /**
   * \brief Frees a previously allocated buffer slice
   *
   * Must only be called for buffer slices that are no longer in
   * use by the GPU.
   * \param [in] slice Buffer slice to free
   */
  void free(
    const GfxBufferSlice&               slice);

  /**
   * \brief Frees unused buffers
   *
   * \param [in] loadFactor Desired load factor. A value of 0.5
   *    indicates that at the end of the operation, at least 50%
   *    of allocated memory should be in active use, if possible.
   */
  void trim(
          float                         loadFactor);

private:

  GfxDevice                       m_device;

  std::string                     m_name;
  GfxBufferDesc                   m_desc;
  GfxMemoryTypes                  m_memoryTypes;

  std::mutex                      m_mutex;
  std::vector<GfxBufferPoolChunk> m_chunks;
  std::vector<GfxBufferPoolChunk> m_freed;
  GfxBufferPoolStats              m_stats;

};

}
