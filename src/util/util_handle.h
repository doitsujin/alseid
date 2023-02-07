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

  explicit Handle(uint64_t raw)
  : m_raw(raw) { }

  /**
   * \brief Retrieves raw handle value
   * \returns Raw handle value
   */
  uint64_t getRaw() const {
    return m_raw;
  }

  bool operator == (const Handle& other) const { return m_raw == other.m_raw; }
  bool operator != (const Handle& other) const { return m_raw != other.m_raw; }

  operator bool () const {
    return m_raw != 0ull;
  }

private:

  uint64_t m_raw = 0ull;

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
