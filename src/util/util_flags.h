#pragma once

#include <cstdint>
#include <iterator>
#include <type_traits>

namespace as {

template<typename T>
class Flags {
  static_assert(std::is_enum_v<T>);
public:

  using IntType = std::underlying_type_t<T>;
  static_assert(!IntType(T::eFlagEnum));

  class iterator {

  public:

    using iterator_category = std::input_iterator_tag;
    using difference_type = uint32_t;
    using value_type = T;
    using reference = T;
    using pointer = const T*;

    explicit iterator(IntType flags)
    : m_flags(flags) { }

    iterator& operator ++ () {
      m_flags &= m_flags - 1;
      return *this;
    }

    iterator operator ++ (int) {
      iterator retval = *this;
      m_flags &= m_flags - 1;
      return retval;
    }

    T operator * () const {
      return T(m_flags & -m_flags);
    }

    bool operator == (iterator other) const { return m_flags == other.m_flags; }
    bool operator != (iterator other) const { return m_flags != other.m_flags; }

  private:

    IntType m_flags;

  };

  Flags() = default;
  Flags(IntType raw)
  : m_raw(raw) { }
  Flags(T flag)
  : m_raw(IntType(flag)) { }

  iterator begin() const { return iterator(m_raw); }
  iterator end() const { return iterator(0); }

  Flags operator | (Flags f) const { return Flags(m_raw | f.m_raw); }
  Flags operator & (Flags f) const { return Flags(m_raw & f.m_raw); }
  Flags operator ^ (Flags f) const { return Flags(m_raw ^ f.m_raw); }
  Flags operator - (Flags f) const { return Flags(m_raw &~f.m_raw); }

  Flags& operator |= (Flags f) { m_raw |= f.m_raw; return *this; }
  Flags& operator &= (Flags f) { m_raw &= f.m_raw; return *this; }
  Flags& operator ^= (Flags f) { m_raw ^= f.m_raw; return *this; }
  Flags& operator -= (Flags f) { m_raw &=~f.m_raw; return *this; }

  bool all(Flags f) {
    return (m_raw & f.m_raw) == f.m_raw;
  }

  T first() const {
    return T(m_raw & -m_raw);
  }

  bool operator == (Flags f) const { return m_raw == f.m_raw; }
  bool operator != (Flags f) const { return m_raw != f.m_raw; }

  operator bool () const {
    return m_raw != 0;
  }

  explicit operator T () const {
    return T(m_raw);
  }

  explicit operator IntType () const {
    return m_raw;
  }

private:

  IntType m_raw;

};

}

template<typename T, T v = T::eFlagEnum>
auto operator | (T a, T b) { return as::Flags<T>(a) | b; }
template<typename T, T v = T::eFlagEnum>
auto operator & (T a, T b) { return as::Flags<T>(a) & b; }
template<typename T, T v = T::eFlagEnum>
auto operator ^ (T a, T b) { return as::Flags<T>(a) ^ b; }
template<typename T, T v = T::eFlagEnum>
auto operator - (T a, T b) { return as::Flags<T>(a) - b; }
template<typename T, T v = T::eFlagEnum>
auto operator == (T a, T b) { return as::Flags<T>(a) == b; }
template<typename T, T v = T::eFlagEnum>
auto operator != (T a, T b) { return as::Flags<T>(a) != b; }

template<typename T, T v = T::eFlagEnum>
auto operator | (T a, as::Flags<T> b) { return as::Flags<T>(a) | b; }
template<typename T, T v = T::eFlagEnum>
auto operator & (T a, as::Flags<T> b) { return as::Flags<T>(a) & b; }
template<typename T, T v = T::eFlagEnum>
auto operator ^ (T a, as::Flags<T> b) { return as::Flags<T>(a) ^ b; }
template<typename T, T v = T::eFlagEnum>
auto operator - (T a, as::Flags<T> b) { return as::Flags<T>(a) - b; }
template<typename T, T v = T::eFlagEnum>
auto operator == (T a, as::Flags<T> b) { return as::Flags<T>(a) == b; }
template<typename T, T v = T::eFlagEnum>
auto operator != (T a, as::Flags<T> b) { return as::Flags<T>(a) != b; }
