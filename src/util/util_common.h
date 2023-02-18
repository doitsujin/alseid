#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

namespace as {

/**
 * \brief Object wrapper for temporary lvalue references
 *
 * Useful to wrap mutable objects like streams
 * in case some side effects can be discarded.
 */
template<typename T>
class Lwrap {

public:

  template<typename... Args>
  Lwrap(Args&&... args)
  : m_object(std::forward<Args>(args)...) { }

  operator T& () {
    return m_object;
  }

private:

  T m_object;

};

/**
 * \brief Wraps object
 * \returns Object
 */
template<typename T>
auto lwrap(T&& object) {
  return Lwrap<T>(std::move(object));
}

}
