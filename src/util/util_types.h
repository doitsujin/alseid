#pragma once

#include <string>

#include "util_hash.h"
#include "util_matrix.h"
#include "util_vector.h"

namespace as {

using Offset2D = Vector<int32_t, 2>;
using Offset3D = Vector<int32_t, 3>;

using Extent2D = Vector<uint32_t, 2>;
using Extent3D = Vector<uint32_t, 3>;


/**
 * \brief Rectangle
 *
 * Stores a signed 2D offset
 * and an unsigned extent.
 */
struct Rect2D {
  Rect2D() { }
  Rect2D(Offset2D o, Extent2D e)
  : offset(o), extent(e) { }

  Offset2D offset;
  Extent2D extent;

  bool operator == (const Rect2D&) const = default;
  bool operator != (const Rect2D&) const = default;
};


/**
 * \brief Box
 *
 * Stores a signed 3D offset
 * and an unsigned extent.
 */
struct Box3D {
  Box3D() { }
  Box3D(Offset3D o, Extent3D e)
  : offset(o), extent(e) { }

  Offset3D offset;
  Extent3D extent;

  bool operator == (const Box3D&) const = default;
  bool operator != (const Box3D&) const = default;
};


/**
 * \brief FourCC
 *
 * Used in various binary formats.
 */
struct FourCC {
  FourCC() = default;

  FourCC(char c0, char c1, char c2, char c3) {
    c[0] = c0;
    c[1] = c1;
    c[2] = c2;
    c[3] = c3;
  }

  explicit FourCC(const std::string& str) {
    for (uint32_t i = 0; i < 4; i++)
      c[i] = i < str.size() ? str[i] : ' ';
  }

  char c[4];

  bool operator == (const FourCC&) const = default;
  bool operator != (const FourCC&) const = default;

  std::string toString() const {
    std::string result(4, '\0');
    for (size_t i = 0; i < 4; i++)
      result[i] = c[i];
    return result;
  }
};


/**
 * \brief 24-bit integer
 *
 * Provides 24-bit storage, but no arithmetic operations.
 * Explicitly cast to 32-bit integers to do math on this.
 */
struct uint24_t {
  uint24_t() = default;

  explicit uint24_t(uint32_t value)
  : data { uint8_t(value >>  0),
           uint8_t(value >>  8),
           uint8_t(value >> 16) } { }

  explicit operator uint32_t () const {
    return uint32_t(data[0])
         | uint32_t(data[1]) << 8
         | uint32_t(data[2]) << 16;
  }

  bool operator == (const uint24_t& other) const = default;
  bool operator != (const uint24_t& other) const = default;

  uint8_t data[3];
};

static_assert(sizeof(uint24_t) == 3 && alignof(uint24_t) == 1);


/**
 * \brief Short string
 *
 * Provides storage for null-terminated fixed-size strings.
 * \tparam N Maximum number of characters to store
 * \tparam Ch Character type
 */
template<size_t N>
class ShortString {

public:

  ShortString() = default;

  ShortString(const char* str) {
    size_t i = 0;

    while (i + 1 < N && str[i]) {
      m_data[i] = str[i];
      i += 1;
    }

    while (i < N) {
      m_data[i] = '\0';
      i += 1;
    }
  }

  ShortString(const std::string& str)
  : ShortString(str.c_str()) { }

  char operator [] (size_t idx) const {
    return m_data[idx];
  }

  size_t size() const {
    size_t n = 0;

    while (m_data[n])
      n += 1;

    return n;
  }

  const char* data() const {
    return m_data;
  }

  const char* c_str() const {
    return m_data;
  }

  bool operator == (const char* other) const {
    for (size_t i = 0; i < N; i++) {
      if (m_data[i] != other[i])
        return false;

      if (!m_data[i])
        break;
    }

    return true;
  }

  bool operator == (const std::string& other) const {
    return operator == (other.c_str());
  }

  bool operator == (const ShortString& other) const {
    return operator == (other.c_str());
  }

  template<typename T>
  bool operator != (const T& other) const {
    return !operator == (other);
  }

  bool empty() const {
    return m_data[0] == '\0';
  }

  size_t hash() const {
    HashState hash;

    for (size_t i = 0; i < N && m_data[i]; i++)
      hash.add(uint32_t(uint8_t(m_data[i])));

    return hash;
  }

private:

  char m_data[N];

};

}
