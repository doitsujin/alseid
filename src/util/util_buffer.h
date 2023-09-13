#pragma once

#include "util_ptr.h"

#include <cstdlib>

namespace as {

/**
 * \brief Aligned buffer
 *
 * Provides a convenience wrapper around a
 * fixed-size, aligned memory allocation.
 */
class AlignedBuffer {

public:

  AlignedBuffer()
  : m_data(nullptr)
  , m_size(0) { }

  explicit AlignedBuffer(size_t size, size_t alignment)
  : m_data(size ? std::aligned_alloc(alignment, size) : nullptr)
  , m_size(size) { }

  AlignedBuffer(AlignedBuffer&& other)
  : m_data(other.m_data)
  , m_size(other.m_size) {
    other.m_data = nullptr;
    other.m_size = 0;
  }

  AlignedBuffer& operator = (AlignedBuffer&& other) {
    if (m_data)
      std::free(m_data);

    m_data = other.m_data;
    m_size = other.m_size;

    other.m_data = nullptr;
    other.m_size = 0;
    return *this;
  }

  ~AlignedBuffer() {
    if (m_data)
      std::free(m_data);
  }

  void* getData() const {
    return m_data;
  }

  size_t getSize() const {
    return m_size;
  }

  void* getAt(size_t offset) const {
    return ptroffset(m_data, offset);
  }

  template<typename T>
  T* getAs(size_t offset) const {
    return reinterpret_cast<T*>(getAt(offset));
  }

private:

  void*   m_data;
  size_t  m_size;

};

}
