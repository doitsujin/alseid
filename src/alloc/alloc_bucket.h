#pragma once

#include <atomic>
#include <optional>

#include "../util/util_math.h"

namespace as {

/**
 * \brief Bucket allocator
 *
 * Allows allocating objects from a fixed-size bucket,
 * which is internally represented by a bit mask.
 * This allocator is fully thread-safe.
 */
template<typename T>
class BucketAllocator {

public:

  BucketAllocator()
  : m_freeMask(0)
  , m_capacity(0) { }

  /**
   * \brief Initializes bucket allocator
   * \param [in] capacity Number of objects
   */
  explicit BucketAllocator(uint32_t capacity)
  : m_freeMask(computeMask(0, capacity))
  , m_capacity(capacity) { }

  /**
   * \brief Tries to allocate objects
   *
   * If possible, allocates the given number of
   * consecutive objects.
   * \param [in] count Number of objects to allocate
   * \returns Index of the allocated objects
   */
  std::optional<uint32_t> alloc(uint32_t count) {
    T oldFreeMask = m_freeMask.load(std::memory_order_acquire);
    T newFreeMask;

    uint32_t index;

    do {
      index = 0;

      // Find a sequence of count consecutive free bits
      while (index + count <= m_capacity) {
        T freeMask = oldFreeMask >> index;
        uint32_t freeCount = tzcnt(~freeMask);

        if (freeCount >= count)
          break;

        uint32_t usedCount = tzcnt(freeMask >> freeCount);
        index += freeCount + usedCount;
      }

      // Not enough memory available in this buffer
      if (index + count > m_capacity)
        return std::nullopt;

      // Unset bits in the free mask
      newFreeMask = oldFreeMask & ~computeMask(index, count);
    } while (!m_freeMask.compare_exchange_strong(oldFreeMask, newFreeMask));

    return index;
  }

  /**
   * \brief Frees memory range
   *
   * \param [in] offset Offset of allocation
   * \param [in] size Size of allocation
   */
  void free(uint32_t index, uint32_t count) {
    m_freeMask.fetch_or(computeMask(index, count), std::memory_order_release);
  }

private:

  std::atomic<T>  m_freeMask;
  uint32_t        m_capacity;

  static T computeMask(uint32_t index, uint32_t count) {
    return ((T(2) << (count - 1)) - T(1)) << index;
  }

};

}
