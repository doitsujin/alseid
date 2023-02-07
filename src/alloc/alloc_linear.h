#pragma once

#include <optional>

#include "../util/util_math.h"

namespace as {

/**
 * \brief Linear allocator
 *
 * Very simple linear allocator that supports
 * aligned allocations on a fixed memory capacity.
 */
template<typename T>
class LinearAllocator {

public:

  LinearAllocator()
  : m_capacity(0), m_offset(0) { }

  /**
   * \brief Initializes linear allocator
   * \param [in] capacity Allocator capacity
   */
  explicit LinearAllocator(T capacity)
  : m_capacity(capacity), m_offset(0) { }

  /**
   * \brief Returns capacity
   * \returns Capacity
   */
  T capacity() const {
    return m_capacity;
  }

  /**
   * \brief Tries to allocate memory
   *
   * \param [in] size Amount to allocate
   * \param [in] alignment Offset alignment
   * \returns Allocation offset if successful
   */
  std::optional<T> alloc(T size, T alignment) {
    T offset = align(m_offset, alignment);

    if (offset + size > m_capacity)
      return std::nullopt;

    m_offset = offset + size;
    return offset;
  }

  /**
   * \brief Resets allocator
   */
  void reset() {
    m_offset = 0;
  }

private:

  T m_capacity;
  T m_offset;

};

}
