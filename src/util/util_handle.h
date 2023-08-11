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

  Handle() = default;

  explicit Handle(int32_t raw)
  : m_raw(raw) { }

  /**
   * \brief Retrieves raw handle value
   * \returns Raw handle value
   */
  explicit operator int32_t () const {
    return m_raw;
  }

  bool operator == (const Handle&) const = default;
  bool operator != (const Handle&) const = default;

  operator bool () const {
    return m_raw >= 0;
  }

private:

  int32_t m_raw = -1;

};

/** 
 * \brief Handle hasher
 */
struct HandleHash {
  template<typename T>
  size_t operator () (const Handle<T>& handle) const {
    return size_t(int32_t(handle));
  }
};

}
