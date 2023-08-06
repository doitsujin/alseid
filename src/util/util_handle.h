#pragma once

#include <cstddef>
#include <cstdint>

namespace as {

/**
 * \brief Type-safe handle
 *
 * Can be used to identify objects of a given type,
 * without actually referencing that object.
 */
template<typename T>
class Handle {

public:

  Handle() { }

  explicit Handle(uint32_t raw)
  : m_raw(raw) { }

  /**
   * \brief Retrieves raw handle value
   * \returns Raw handle value
   */
  explicit operator uint32_t () const {
    return m_raw;
  }

  bool operator == (const Handle& other) const { return m_raw == other.m_raw; }
  bool operator != (const Handle& other) const { return m_raw != other.m_raw; }

  operator bool () const {
    return m_raw != 0u;
  }

private:

  uint32_t m_raw = 0u;

};

/** 
 * \brief Handle hasher
 */
struct HandleHash {
  template<typename T>
  size_t operator () (const Handle<T>& handle) const {
    return size_t(handle.getRaw());
  }
};

}
