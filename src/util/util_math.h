#pragma once

#include <cstddef>
#include <cstdint>
#include <cmath>

#ifndef _MSC_VER
  #define AS_HAS_X86_INTRINSICS 1
  #if defined(__WINE__) && defined(__clang__)
    #pragma push_macro("_WIN32")
    #undef _WIN32
  #endif
  #include <x86intrin.h>
  #if defined(__WINE__) && defined(__clang__)
    #pragma pop_macro("_WIN32")
  #endif
#else
  #define AS_HAS_X86_INTRINSICS 1
  #include <intrin.h>
#endif

namespace as {

/**
 * \brief Aligns integer offset
 *
 * Increases \c value to the next multiple of \c alignment.
 * \param [in] value Original value. Must not be negative.
 * \param [in] alignment Desired alignment. Must be a power of two.
 * \returns Aligned value
 */
template<typename T>
T align(T value, T alignment) {
  return (value + alignment - 1) & ~(alignment - 1);
}


inline uint32_t popcntStep(uint32_t n, uint32_t mask, uint32_t shift) {
  return (n & mask) + ((n & ~mask) >> shift);
}


/**
 * \brief Population count
 *
 * \param [in] n Number
 * \returns Number of bits set to 1
 */
inline uint32_t popcnt(uint32_t n) {
  n = popcntStep(n, 0x55555555, 1);
  n = popcntStep(n, 0x33333333, 2);
  n = popcntStep(n, 0x0F0F0F0F, 4);
  n = popcntStep(n, 0x00FF00FF, 8);
  n = popcntStep(n, 0x0000FFFF, 16);
  return n;
}


/**
 * \brief Trailing zero count (32-bit)
 *
 * Returns operand size in bits if the
 * input operand is zero.
 * \param [in] n Number
 * \returns Number of trailing zeroes
 */
inline uint32_t tzcnt(uint32_t n) {
  #if defined(_MSC_VER) && !defined(__clang__)
  return _tzcnt_u32(n);
  #elif defined(__BMI__)
  return __tzcnt_u32(n);
  #elif defined(__GNUC__) || defined(__clang__)
  // tzcnt is encoded as rep bsf, so we can use it on all
  // processors, but the behaviour of zero inputs differs:
  // - bsf:   zf = 1, cf = ?, result = ?
  // - tzcnt: zf = 0, cf = 1, result = 32
  // We'll have to handle this case manually.
  uint32_t res;
  uint32_t tmp;
  asm (
    "tzcnt %2, %0;"
    "mov  $32, %1;"
    "test  %2, %2;"
    "cmovz %1, %0;"
    : "=&r" (res), "=&r" (tmp)
    : "r" (n)
    : "cc");
  return res;
  #else
  uint32_t r = 31;
  n &= -n;
  r -= (n & 0x0000FFFF) ? 16 : 0;
  r -= (n & 0x00FF00FF) ?  8 : 0;
  r -= (n & 0x0F0F0F0F) ?  4 : 0;
  r -= (n & 0x33333333) ?  2 : 0;
  r -= (n & 0x55555555) ?  1 : 0;
  return n != 0 ? r : 32;
  #endif
}


/**
 * \brief Trailing zero count (64-bit)
 *
 * Returns operand size in bits if the
 * input operand is zero.
 * \param [in] n Number
 * \returns Number of trailing zeroes
 */
inline uint32_t tzcnt(uint64_t n) {
  #if defined(_MSC_VER) && !defined(__clang__)
  return (uint32_t)_tzcnt_u64(n);
  #elif defined(__BMI__)
  return __tzcnt_u64(n);
  #elif (defined(__GNUC__) || defined(__clang__))
  uint64_t res;
  uint64_t tmp;
  asm (
    "tzcnt %2, %0;"
    "mov  $64, %1;"
    "test  %2, %2;"
    "cmovz %1, %0;"
    : "=&r" (res), "=&r" (tmp)
    : "r" (n)
    : "cc");
  return res;
  #else
  uint32_t lo = uint32_t(n);
  if (lo) {
    return tzcnt(lo);
  } else {
    uint32_t hi = uint32_t(n >> 32);
    return tzcnt(hi) + 32;
  }
  #endif
}


/**
 * \brief Leading zero count
 *
 * Returns operand size in bits if the
 * input operand is zero.
 */
inline uint32_t lzcnt(uint32_t n) {
  #if defined(_MSC_VER) && !defined(__clang__)
  return __lzcnt(n);
  #elif defined (__BMI__)
  return _lzcnt_u32(n);
  #elif (defined(__GNUC__) || defined(__clang__))
  return n ? __builtin_clz(n) : 32;
  #else
  uint32_t r = 0;
  r += ((n << r) & 0xFFFF0000) ? 0 : 16;
  r += ((n << r) & 0xFF000000) ? 0 : 8;
  r += ((n << r) & 0xF0000000) ? 0 : 4;
  r += ((n << r) & 0xC0000000) ? 0 : 2;
  r += ((n << r) & 0x80000000) ? 0 : 1;
  return n != 0 ? r : 32;
  #endif
}


/**
 * \brief Leading zero count (64-bit)
 *
 * Returns operand size in bits if the
 * input operand is zero.
 */
inline uint32_t lzcnt(uint64_t n) {
  #if defined(_MSC_VER) && !defined(__clang__)
  return __lzcnt64(n);
  #elif defined (__BMI__)
  return _lzcnt_u64(n);
  #elif (defined(__GNUC__) || defined(__clang__))
  return n ? __builtin_clzll(n) : 64;
  #else
  uint32_t hi = uint32_t(n >> 32);
  if (hi) {
    return lzcnt(hi);
  } else {
    uint32_t lo = uint32_t(n);
    return lzcnt(lo) + 32;
  }
  #endif
}


/**
 * \brief Reverse bit scan
 *
 * \param [in] number Number to scan
 * \returns Index of the most significant 1 bit,
 *    or -1 if the number is zero.
 */
inline int32_t findmsb(uint32_t number) {
  return 31 - int32_t(lzcnt(number));
}


/**
 * \brief Reverse bit scan (64-bit)
 *
 * \param [in] number Number to scan
 * \returns Index of the most significant 1 bit,
 *    or -1 if the number is zero.
 */
inline int32_t findmsb(uint64_t number) {
  return 63 - int32_t(lzcnt(number));
}


/**
 * \brief Extracts given set of bits
 *
 * \param [in] op Operand to extract from
 * \param [in] first First bit to extract
 * \param [in] count Number of bits to extract
 * \returns Extracted bit pattern
 */
template<typename T>
T bextract(T op, uint32_t first, uint32_t count) {
  if (!count)
    return 0;

  T mask = (T(2) << (count - 1)) - T(1);
  return (op >> first) & mask;
}


}