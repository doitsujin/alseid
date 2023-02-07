#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace as {

constexpr size_t CacheLineSize = 64;

/**
 * \brief Offsets a pointer by the given amount
 *
 * \param [in] base Base pointer
 * \param [in] offset Offset
 */
template<typename T>
T* ptroffset(T* base, size_t offset) {
  using c_ptr_t = std::conditional_t<std::is_const_v<T>, const char*, char*>;
  auto c_data = reinterpret_cast<c_ptr_t>(base);
  return reinterpret_cast<T*>(c_data + offset);
}

}