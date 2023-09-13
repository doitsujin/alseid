#include "gfx_buffer_pool.h"

namespace as {

GfxBufferPool::GfxBufferPool(
        GfxDevice                     device,
  const GfxBufferDesc&                desc,
        GfxMemoryTypes                memoryTypes)
: m_device      (std::move(device))
, m_desc        (desc)
, m_memoryTypes (memoryTypes) {
  if (m_desc.debugName) {
    m_name = m_desc.debugName;
    m_desc.debugName = m_name.c_str();
  }
}


GfxBufferPool::~GfxBufferPool() {

}


GfxBufferPoolStats GfxBufferPool::getStats() {
  std::lock_guard lock(m_mutex);
  return m_stats;
}


GfxBufferSlice GfxBufferPool::alloc(
        uint64_t                      size,
        uint64_t                      alignment) {
  if (size <= m_desc.size) {
    std::lock_guard lock(m_mutex);

    for (auto& chunk : m_chunks) {
      auto offset = chunk.allocator.alloc(size, alignment);

      if (offset) {
        GfxBufferSlice result;
        result.buffer = chunk.buffer;
        result.offset = *offset;
        result.size = size;

        m_stats.memoryUsed += size;
        return result;
      }
    }

    auto& chunk = m_chunks.emplace_back();

    if (m_freed.empty()) {
      chunk.buffer = m_device->createBuffer(m_desc, m_memoryTypes);
      chunk.allocator = ChunkAllocator<uint64_t>(m_desc.size);

      m_stats.memoryAllocated += m_desc.size;
    } else {
      chunk = std::move(m_freed.back());
      m_freed.pop_back();
    }

    GfxBufferSlice result;
    result.buffer = chunk.buffer;
    result.offset = *chunk.allocator.alloc(size, alignment);
    result.size = size;

    m_stats.memoryUsed += size;
    return result;
  } else {
    GfxBufferDesc desc = m_desc;
    desc.size = size;

    GfxBufferSlice result;
    result.buffer = m_device->createBuffer(desc, m_memoryTypes);
    result.offset = 0;
    result.size = size;

    std::lock_guard lock(m_mutex);
    m_stats.memoryAllocated += size;
    m_stats.memoryUsed += size;
    return result;
  }
}


void GfxBufferPool::free(
  const GfxBufferSlice&               slice) {
  // Slices that were not suballocated will skip the iteration
  // and just be destroyed right away, which is fine.
  std::lock_guard lock(m_mutex);

  if (slice.size <= m_desc.size) {
    // Find the chunk that contains the slice, and if empty after
    // the operation, move it to the list of free chunks.
    for (auto i = m_chunks.begin(); i != m_chunks.end(); ) {
      if (i->buffer == slice.buffer) {
        i->allocator.free(slice.offset, slice.size);

        if (i->allocator.isEmpty()) {
          m_freed.push_back(std::move(*i));
          m_chunks.erase(i);
          break;
        }
      }
    }
  }

  m_stats.memoryUsed -= slice.size;
}


void GfxBufferPool::trim(
        float                         loadFactor) {
  std::lock_guard lock(m_mutex);

  float memoryUsed = float(m_stats.memoryUsed);
  float memoryAllocated = float(m_stats.memoryAllocated);

  while (!m_freed.empty() && memoryUsed < memoryAllocated * loadFactor) {
    m_stats.memoryAllocated -= m_freed.back().buffer->getDesc().size;
    memoryAllocated = float(m_stats.memoryAllocated);

    m_freed.pop_back();
  }
}

}
