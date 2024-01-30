#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "util_likely.h"
#include "util_math.h"

namespace as {

/**
 * \brief Bit array class
 *
 * Helper class to work with bit masks consisting of more than
 * 64 entries. Note that individual bit accesses are not bound
 * checked and therefore must be valid.
 * \tparam N Number of bits
 */
template<uint32_t N>
class BitArray {
  constexpr static size_t QwordCount = (N + 63u) / 64u;
public:

  BitArray() = default;


  /**
   * \brief Tests given bit
   *
   * \param [in] bit Bit index
   * \returns \c true if the given bit is set
   */
  bool test(uint32_t bit) const {
    auto location = computeBitLocation(bit);
    return m_qwords[location.first] & (1ull << location.second);
  }


  /**
   * \brief Sets given bit
   * \param [in] bit Bit index
   */
  void set(uint32_t bit) {
    auto location = computeBitLocation(bit);
    m_qwords[location.first] |= (1ull << location.second);
  }


  /**
   * \brief Clears given bit
   * \param [in] bit Bit index
   */
  void clear(uint32_t bit) {
    auto location = computeBitLocation(bit);
    m_qwords[location.first] &= ~(1ull << location.second);
  }


  /**
   * \brief Sets the first n bits
   *
   * Does not touch bits outside the given range.
   * \param [in] n Number of bits to set
   */
  void setLo(uint32_t n) {
    if (unlikely(!n))
      return;

    auto hiLocation = computeBitLocation(n - 1u);

    for (size_t i = 0; i < hiLocation.first; i++)
      m_qwords[i] = ~0ull;

    m_qwords[hiLocation.first] |= (2ull << hiLocation.second) - 1ull;
  }


  /**
   * \brief Computes number of set bits
   * \returns Number of bits set to 1
   */
  uint32_t popcnt() const {
    uint32_t result = as::popcnt(m_qwords[0]);

    for (size_t i = 1; i < m_qwords.size(); i++)
      result += as::popcnt(m_qwords[i]);

    return result;
  }


  /**
   * \brief Computes number of set bits
   *
   * \param [in] n Index of highest bit to consider
   * \returns Number of bits set to 1, up to, but excluding n
   */
  uint32_t popcntLo(uint32_t n) const {
    uint32_t result = 0u;

    if (unlikely(!n))
      return 0u;

    auto location = computeBitLocation(n - 1u);

    for (size_t i = 0; i < location.first; i++)
      result += as::popcnt(m_qwords[i]);

    result += as::popcnt(m_qwords[location.first] & ((2ull << location.second) - 1ull));
    return result;
  }


  /**
   * \brief Finds least significant set bit
   * \returns Index of least significant bit, or -1
   *    if the mask is empty.
   */
  int32_t findlsb() const {
    int32_t result = 0;

    for (size_t i = 0; i < m_qwords.size(); i++) {
      result += as::tzcnt(m_qwords[i]);

      if (m_qwords[i])
        return result;
    }

    return -1;
  }


  /**
   * \brief Finds most significant set bit
   * \returns Index of most significant bit, or -1
   *    if the mask is empty.
   */
  int32_t findmsb() const {
    int32_t result = N - 1;

    for (size_t i = m_qwords.size(); i; i--) {
      result -= as::lzcnt(m_qwords[i - 1]);

      if (m_qwords[i - 1])
        break;
    }

    return result;
  }


  /**
   * \brief Bit-wise or
   *
   * \param [in] src Input mask
   * \returns or'ed bit masks
   */
  template<uint32_t M>
  BitArray& operator |= (const BitArray<M>& src) {
    for (size_t i = 0; i < std::min(QwordCount, src.QwordCount); i++)
      m_qwords[i] |= src.m_qwords[i];
    if (M > N)
      maskLastQword();
    return *this;
  }

  template<uint32_t M>
  BitArray operator | (const BitArray<M>& src) const {
    return BitArray(*this) -= src;
  }


  /**
   * \brief Bit-wise and
   *
   * \param [in] src Input mask
   * \returns and'ed bit masks
   */
  template<uint32_t M>
  BitArray& operator &= (const BitArray<M>& src) {
    for (size_t i = 0; i < std::min(QwordCount, src.QwordCount); i++)
      m_qwords[i] &= src.m_qwords[i];
    maskLastQword();
    return *this;
  }

  template<uint32_t M>
  BitArray operator & (const BitArray<M>& src) const {
    return BitArray(*this) -= src;
  }


  /**
   * \brief Bit-wise xor
   *
   * \param [in] src Input mask
   * \returns xor'd bit masks
   */
  template<uint32_t M>
  BitArray& operator ^= (const BitArray<M>& src) {
    for (size_t i = 0; i < std::min(QwordCount, src.QwordCount); i++)
      m_qwords[i] ^= src.m_qwords[i];
    if (M > N)
      maskLastQword();
    return *this;
  }

  template<uint32_t M>
  BitArray operator ^ (const BitArray<M>& src) const {
    return BitArray(*this) -= src;
  }


  /**
   * \brief Clears a given set of bits
   *
   * \param [in] src Mask with bits to clear
   * \returns Bit mask with all bits set in \c src cleared.
   */
  template<uint32_t M>
  BitArray& operator -= (const BitArray<M>& src) {
    for (size_t i = 0; i < std::min(QwordCount, src.QwordCount); i++)
      m_qwords[i] &= ~src.m_qwords[i];
    return *this;
  }

  template<uint32_t M>
  BitArray operator - (const BitArray<M>& src) const {
    return BitArray(*this) -= src;
  }


  /**
   * \brief Inverts bit mask
   *
   * Exact with regards to the upper bits of the last word.
   * \returns Inverted bit mask
   */
  template<uint32_t M>
  BitArray operator ~ () const {
    BitArray result;

    for (size_t i = 0; i < m_qwords.size(); i++)
      result.m_qwords[i] = ~m_qwords[i];

    result.maskLastQword();
    return result;
  }


  /**
   * \brief Checks whether the bit mask is not empty
   * \returns \c true if any bit is set in the mask.
   */
  explicit operator bool () const {
    for (size_t i = 0; i < m_qwords.size(); i++) {
      if (m_qwords[i])
        return true;
    }

    return false;
  }


  bool operator == (const BitArray&) const = default;
  bool operator != (const BitArray&) const = default;

private:

  std::array<uint64_t, QwordCount> m_qwords;

  void maskLastQword() {
    if constexpr (N % 64u)
      m_qwords[N / 64u] &= (1ull << (N % 64u)) - 1ull;
  }

  static std::pair<size_t, size_t> computeBitLocation(size_t index) {
    return std::make_pair(index >> 6u, index & 63u);
  }

};

}
