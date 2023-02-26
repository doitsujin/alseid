#pragma once

#include <string>

#include "util_vector.h"
#include "util_matrix.h"

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

}
