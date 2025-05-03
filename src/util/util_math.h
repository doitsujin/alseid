#pragma once

#include <iostream>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cmath>

#include "util_likely.h"

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

constexpr double pi = 3.141592653589793;

/**
 * \brief Aligns integer offset
 *
 * Increases \c value to the next multiple of \c alignment.
 * \param [in] value Original value. Must not be negative.
 * \param [in] alignment Desired alignment. Must be a power of two.
 * \returns Aligned value
 */
template<typename T>
constexpr T align(T value, T alignment) {
  return (value + alignment - 1) & ~(alignment - 1);
}


template<typename T>
inline T popcntStep(T n, T  mask, uint32_t shift) {
  return (n & mask) + ((n & ~mask) >> shift);
}


/**
 * \brief Population count
 *
 * \param [in] n Number
 * \returns Number of bits set to 1
 */
inline uint32_t popcnt(uint32_t n) {
#ifdef __POPCNT__
  return _mm_popcnt_u32(n);
#else
  n = popcntStep(n, 0x55555555u, 1u);
  n = popcntStep(n, 0x33333333u, 2u);
  n = popcntStep(n, 0x0F0F0F0Fu, 4u);
  n = popcntStep(n, 0x00FF00FFu, 8u);
  n = popcntStep(n, 0x0000FFFFu, 16u);
  return n;
#endif
}


inline uint32_t popcnt(uint64_t n) {
#ifdef __POPCNT__
  return _mm_popcnt_u64(n);
#else
  n = popcntStep(n, uint64_t(0x5555555555555555ull), 1u);
  n = popcntStep(n, uint64_t(0x3333333333333333ull), 2u);
  n = popcntStep(n, uint64_t(0x0F0F0F0F0F0F0F0Full), 4u);
  n = popcntStep(n, uint64_t(0x00FF00FF00FF00FFull), 8u);
  n = popcntStep(n, uint64_t(0x0000FFFF0000FFFFull), 16u);
  n = popcntStep(n, uint64_t(0x00000000FFFFFFFFull), 32u);
  return n;
#endif
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


/**
 * \brief Inserts given set of bits
 *
 * \param [in] op Operand to insert to
 * \param [in] v Value to insert
 * \param [in] first Bit index to insert at
 * \param [in] count Number of bits to insert
 */
template<typename T>
T binsert(T op, T v, uint32_t first, uint32_t count) {
  if (!count)
    return op;

  T mask = ((T(2) << (count - 1)) - T(1)) << first;
  return (op & ~mask) | ((v << first) & mask);
}


/**
 * \brief Byte swap helpers
 */
inline uint16_t bswap16(uint16_t a) {
  return (a >> 8) | (a << 8);
}

inline uint32_t bswap32(uint32_t a) {
#ifdef AS_HAS_X86_INTRINSICS
  return uint32_t(_bswap(a));
#else
  a = (a >> 16) | (a << 16);
  a = ((a & 0xff00ff00u) >> 8)
    | ((a & 0x00ff00ffu) << 8);
  return a;
#endif
}

inline uint64_t bswap64(uint64_t a) {
#ifdef AS_HAS_X86_INTRINSICS
  return uint64_t(_bswap64(a));
#else
  a = (a >> 32) | (a << 32);
  a = ((a & 0xffff0000ffff0000ull) >> 16)
    | ((a & 0x0000ffff0000ffffull) << 16);
  a = ((a & 0xff00ff00ff00ff00ull) >> 8)
    | ((a & 0x00ff00ff00ff00ffull) << 8);
  return a;
#endif
}


/**
 * \brief Swaps byte order
 *
 * Useful to convert between big endian and little endian.
 * \param [in] a Input value
 * \returns Input value with reversed byte order
 */
template<typename T, std::enable_if_t<std::is_integral_v<T>, bool> = true>
inline T bswap(T a) {
  if constexpr (sizeof(T) == 8)
    return T(bswap64(uint64_t(a)));

  if constexpr (sizeof(T) == 4)
    return T(bswap32(uint32_t(a)));

  if constexpr (sizeof(T) == 2)
    return T(bswap16(uint16_t(a)));

  return a;
}


/**
 * \brief Reverses bits
 *
 * \param [in] a Input
 * \returns Input with reversed bits
 */
template<typename T>
T breverse(T a) {
  a = ((a & T(0xf0f0f0f0f0f0f0f0ull)) >> 4)
    | ((a & T(0x0f0f0f0f0f0f0f0full)) << 4);
  a = ((a & T(0xccccccccccccccccull)) >> 2)
    | ((a & T(0x3333333333333333ull)) << 2);
  a = ((a & T(0xaaaaaaaaaaaaaaaaull)) >> 1)
    | ((a & T(0x5555555555555555ull)) << 1);
  return bswap(a);
}


/**
 * \brief Computes multiply-add
 * \returns a * b + c
 */
template<typename T>
T fmadd(T a, T b, T c) {
  return a * b + c;
}


/**
 * \brief Computes negative multiply-add
 * \returns c - a * b
 */
template<typename T>
T fnmadd(T a, T b, T c) {
  return c - a * b;
}


/**
 * \brief Computes multiply-subtract
 * \returns a * b - c
 */
template<typename T>
T fmsub(T a, T b, T c) {
  return a * b - c;
}


/**
 * \brief Computes negative multiply-subtract
 * \returns -(a * b) - c
 */
template<typename T>
T fnmsub(T a, T b, T c) {
  return -(a * b) - c;
}


/**
 * \brief Computes absolute value
 * \returns |a|
 */
template<typename T>
T abs(T a) {
  return std::abs(a);
}


/**
 * \brief Clamps value
 * \returns min(max(a, lo), hi)
 */
template<typename T>
T clamp(T a, T lo, T hi) {
  return std::min(std::max(a, lo), hi);
}


#ifdef AS_HAS_X86_INTRINSICS

/**
 * \brief Computes packed absolute value
 * \returns |a|
 */
force_inline __m128 abs_packed(__m128 a) {
  return _mm_and_ps(a, _mm_castsi128_ps(_mm_set1_epi32(0x7fffffff)));
}


/**
 * \brief Computes packed negation
 * \returns -a
 */
force_inline __m128 neg_packed(__m128 a) {
  return _mm_xor_ps(a, _mm_castsi128_ps(_mm_set1_epi32(0x80000000)));
}


/**
 * \brief Computes packed multiply-add
 * \returns a + b * c
 */
force_inline __m128 fmadd_packed(__m128 a, __m128 b, __m128 c) {
  #ifdef __FMA__
  return _mm_fmadd_ps(a, b, c);
  #else
  return _mm_add_ps(_mm_mul_ps(a, b), c);
  #endif
}


/**
 * \brief Computes packed negative multiply-add
 * \returns c - a * b
 */
force_inline __m128 fnmadd_packed(__m128 a, __m128 b, __m128 c) {
  #ifdef __FMA__
  return _mm_fnmadd_ps(a, b, c);
  #else
  return _mm_sub_ps(c, _mm_mul_ps(a, b));
  #endif
}


/**
 * \brief Computes packed multiply-subtract
 * \returns a * b - c
 */
force_inline __m128 fmsub_packed(__m128 a, __m128 b, __m128 c) {
  #ifdef __FMA__
  return _mm_fmsub_ps(a, b, c);
  #else
  return _mm_sub_ps(_mm_mul_ps(a, b), c);
  #endif
}


/**
 * \brief Computes packed negative multiply-subtract
 * \returns -(a * b) - c
 */
force_inline __m128 fnmsub_packed(__m128 a, __m128 b, __m128 c) {
  #ifdef __FMA__
  return _mm_fnmsub_ps(a, b, c);
  #else
  return _mm_sub_ps(_mm_sub_ps(_mm_setzero_ps(), _mm_mul_ps(a, b)), c);
  #endif
}


/**
 * \brief Computes packed addsub reesult
 * \returns a ± b
 */
force_inline __m128 addsub_packed(__m128 a, __m128 b) {
  #ifdef __SSE3__
  return _mm_addsub_ps(a, b);
  #else
  __m128 x = _mm_sub_ps(a, b);
  __m128 y = _mm_add_ps(a, b);
  __m128 r = _mm_shuffle_ps(x, y, _MM_SHUFFLE(3, 1, 2, 0));
  return _mm_shuffle_ps(r, r, _MM_SHUFFLE(3, 1, 2, 0));
  #endif
}


/**
 * \brief Computes packed multiply-addsub
 * \returns a * b ± c
 */
force_inline __m128 fmaddsub_packed(__m128 a, __m128 b, __m128 c) {
  #ifdef __FMA__
  return _mm_fmaddsub_ps(a, b, c);
  #else
  return addsub_packed(_mm_mul_ps(a, b), c);
  #endif
}


// GCC generates garbage code for these even when used
// in longer functions, so just don't use these
#if defined(__FMA__) && defined(__clang__)
force_inline float fmadd(float a, float b, float c) { return _mm_cvtss_f32(_mm_fmadd_ss(_mm_set_ss(a), _mm_set_ss(b), _mm_set_ss(c))); }
force_inline float fmsub(float a, float b, float c) { return _mm_cvtss_f32(_mm_fmsub_ss(_mm_set_ss(a), _mm_set_ss(b), _mm_set_ss(c))); }
force_inline float fnmadd(float a, float b, float c) { return _mm_cvtss_f32(_mm_fnmadd_ss(_mm_set_ss(a), _mm_set_ss(b), _mm_set_ss(c))); }
force_inline float fnmsub(float a, float b, float c) { return _mm_cvtss_f32(_mm_fnmsub_ss(_mm_set_ss(a), _mm_set_ss(b), _mm_set_ss(c))); }
#endif

/**
 * \brief Computes packed approximate reciprocal
 * \returns 1 / a (approx)
 */
force_inline __m128 approx_rcp_packed(__m128 a) {
  __m128 two = _mm_set1_ps(2.0f);
  __m128 x = _mm_rcp_ps(a);

  return _mm_mul_ps(x, fnmadd_packed(a, x, two));
}


/**
 * \brief Computes packed approximate division
 * \returns a / b (approx)
 */
force_inline __m128 approx_div_packed(__m128 a, __m128 b) {
  __m128 two = _mm_set1_ps(2.0f);
  __m128 x = _mm_rcp_ps(b);

  return _mm_mul_ps(_mm_mul_ps(a, x), fnmadd_packed(b, x, two));
}


/**
 * \brief Computes packed approximate inverse square root
 * \returns 1 / sqrt(a) (approx)
 */
force_inline __m128 approx_rsqrt_packed(__m128 a) {
  __m128 half = _mm_set1_ps(0.5f);
  __m128 three = _mm_set1_ps(3.0f);
  __m128 x = _mm_rsqrt_ps(a);
  __m128 ax = _mm_mul_ps(a, x);

  return _mm_mul_ps(_mm_mul_ps(half, x),
    fnmadd_packed(x, ax, three));
}


/**
 * \brief Computes packed approximate square root
 * \returns sqrt(a) (approx)
 */
force_inline __m128 approx_sqrt_packed(__m128 a) {
  __m128 zero = _mm_setzero_ps();
  __m128 mask = _mm_cmpeq_ps(a, zero);
  __m128 half = _mm_set1_ps(0.5f);
  __m128 three = _mm_set1_ps(3.0f);
  __m128 x = _mm_rsqrt_ps(a);
         x = _mm_andnot_ps(mask, x);
  __m128 ax = _mm_mul_ps(a, x);

  return _mm_mul_ps(_mm_mul_ps(half, ax),
    fnmadd_packed(x, ax, three));
}


/**
 * \brief Computes vector dot product
 *
 * Only the first component will be valid.
 * \returns dot(a, b)
 */
force_inline __m128 dot_packed_one(__m128 a, __m128 b) {
  __m128 r = _mm_mul_ps(a, b);
  __m128 s = _mm_shuffle_ps(r, r, _MM_SHUFFLE(2, 3, 0, 1));
  r = _mm_add_ps(r, s);
  s = _mm_movehl_ps(r, r);
  return _mm_add_ss(r, s);
}


/**
 * \brief Computes full vector dot product
 *
 * All components will contain the correct value.
 * \returns dot(a, b)
 */
force_inline __m128 dot_packed(__m128 a, __m128 b) {
  __m128 r = _mm_mul_ps(a, b);
  __m128 s = _mm_shuffle_ps(r, r, _MM_SHUFFLE(2, 3, 0, 1));
  r = _mm_add_ps(r, s);
  s = _mm_shuffle_ps(r, r, _MM_SHUFFLE(1, 0, 3, 2));
  return _mm_add_ps(r, s);
}


/**
 * \brief Blends vectors with constant mask
 * \returns blendps result
 */
template<uint8_t Imm>
force_inline __m128 blend_packed(__m128 a, __m128 b) {
  #ifdef __SSE4_1__
  return _mm_blend_ps(a, b, Imm);
  #else
  __m128 mask = _mm_castsi128_ps(_mm_set_epi32(
    Imm & 0x8 ? ~0u : 0, Imm & 0x4 ? ~0u : 0,
    Imm & 0x2 ? ~0u : 0, Imm & 0x1 ? ~0u : 0));

  return _mm_or_ps(
    _mm_andnot_ps(mask, a),
    _mm_and_ps(mask, b));
  #endif
}


/**
 * \brief Blends vectors with variable mask
 * \returns blendvps result
 */
force_inline __m128 blendv_packed(__m128 a, __m128 b, __m128 mask) {
  #ifdef __SSE4_1__
  return _mm_blendv_ps(a, b, mask);
  #else
  mask = _mm_castsi128_ps(_mm_cmplt_epi32(_mm_castps_si128(mask), _mm_setzero_si128()));
  return _mm_or_ps(_mm_andnot_ps(mask, a), _mm_and_ps(mask, b));
  #endif
}


/**
 * \brief Decomposes float into fractional and integer part
 *
 * \param [in] x Float
 * \param [out] i Integer part
 * \returns Fractional part
 */
force_inline __m128 ftrunc_packed(__m128 x, __m128i& i) {
  i = _mm_cvttps_epi32(x);

  __m128 f;
  #ifdef __SSE4_1__
  f = _mm_round_ps(x, _MM_FROUND_TRUNC);
  #else
  f = _mm_cvtepi32_ps(i);
  #endif

  return _mm_sub_ps(x, f);
}


/**
 * \brief Computes cross product
 *
 * The last component will be the product of the
 * last components of both vectors.
 * \returns a × b
 */
force_inline __m128 cross_packed(__m128 a, __m128 b) {
  constexpr uint8_t s = _MM_SHUFFLE(3, 0, 2, 1);

  __m128 as = _mm_shuffle_ps(a, a, s);
  __m128 bs = _mm_shuffle_ps(b, b, s);
  __m128 p = _mm_mul_ps(a, bs);
  __m128 r = fnmadd(as, b, p);

  #ifdef __SSE4_1__
  r = _mm_shuffle_ps(r, r, s);
  r = _mm_blend_ps(p, r, 0x7);
  #else
  r = _mm_shuffle_ps(r, r, _MM_SHUFFLE(0, 3, 2, 1));
  p = _mm_unpackhi_ps(r, p);
  r = _mm_shuffle_ps(r, p, _MM_SHUFFLE(3, 2, 1, 0));
  #endif

  return r;
}


#endif


/**
 * \brief Computes approximate reciprocal
 * \returns 1 / a (approx.)
 */
force_inline float approx_rcp(float a) {
#ifdef AS_HAS_X86_INTRINSICS
  return _mm_cvtss_f32(approx_rcp_packed(_mm_set_ss(a)));
#else
  return 1.0f / a;
#endif
}


/**
 * \brief Computes approximate quotient
 * \returns a / b (approx.)
 */
force_inline float approx_div(float a, float b) {
#ifdef AS_HAS_X86_INTRINSICS
  return _mm_cvtss_f32(approx_div_packed(_mm_set_ss(a), _mm_set_ss(b)));
#else
  return a / b;
#endif
}



/**
 * \brief Computes approximate square root
 * \returns sqrt(n) (approx.)
 */
force_inline float approx_sqrt(float n) {
#ifdef AS_HAS_X86_INTRINSICS
  return _mm_cvtss_f32(approx_sqrt_packed(_mm_set_ss(n)));
#else
  return std::sqrt(n);
#endif
}


/**
 * \brief Computes approximate inverse square root
 * \returns sqrt(n) (approx.)
 */
force_inline float approx_rsqrt(float n) {
#ifdef AS_HAS_X86_INTRINSICS
  return _mm_cvtss_f32(approx_rsqrt_packed(_mm_set_ss(n)));
#else
  return 1.0f / std::sqrt(n);
#endif
}


/**
 * \brief Coefficients for taylor series
 *
 * \param [in] n Power
 * \returns Coefficient
 */
constexpr float sincos_coeff(uint32_t n) {
  double result = 1.0;

  for (uint32_t i = 1; i <= n; i++)
    result *= pi * 0.25f / double(i);

  return float(result);
}


/**
 * \brief Computes packed approximate sine and cosine
 *
 * Can process up to two inputs at a time.
 * \returns Vector of (sin(x), cos(x), sin(y), cos(y))
 */
#if defined(AS_HAS_X86_INTRINSICS)
inline __m128 approx_sincos_vectorized(__m128 x) {
  constexpr float c7 = -sincos_coeff(7);
  constexpr float c5 =  sincos_coeff(5);
  constexpr float c3 = -sincos_coeff(3);
  constexpr float c1 =  sincos_coeff(1);

  __m128 sgnMask = _mm_castsi128_ps(_mm_set1_epi32(0x80000000));
  __m128 one = _mm_set1_ps(1.0f);

  // Duplicate input components
  __m128 xDup = _mm_unpacklo_ps(x, x);

  // Normalize input to range of [0..pi/4)
  __m128 xNorm = _mm_mul_ps(_mm_andnot_ps(sgnMask, xDup), _mm_set1_ps(4.0 / pi));
  __m128i xInt;
  xNorm = ftrunc_packed(xNorm, xInt);

  // Mirror input as necessary
  __m128i xMirrorMask = _mm_slli_epi32(xInt, 31);
  xNorm = blendv_packed(xNorm, _mm_sub_ps(one, xNorm), _mm_castsi128_ps(xMirrorMask));

  // Compute taylor series with minimal dependency chains
  __m128 xPow2 = _mm_mul_ps(xNorm, xNorm);
  __m128 xPow4 = _mm_mul_ps(xPow2, xPow2);

  __m128 prodHi = fmadd_packed(xPow2, _mm_set1_ps(c7), _mm_set1_ps(c5));
  __m128 prodLo = fmadd_packed(xPow2, _mm_set1_ps(c3), _mm_set1_ps(c1));
  __m128 taylor = _mm_mul_ps(fmadd_packed(xPow4, prodHi, prodLo), xNorm);

  // Compute co-function. Input to sqrt is always greater
  // than 0, so the result will always be a valid number.
  __m128 cofactor = fnmadd_packed(taylor, taylor, one);

  __m128 rsqrt = _mm_rsqrt_ps(cofactor);
  __m128 asqrt = _mm_mul_ps(cofactor, rsqrt);

  // One Newton-Raphson round to improve precision
  __m128 co = _mm_mul_ps(_mm_mul_ps(asqrt, _mm_set1_ps(0.5f)),
    fnmadd_packed(rsqrt, asqrt, _mm_set1_ps(3.0f)));

  // Determine sign of the respective results
  __m128 sgn = _mm_castsi128_ps(_mm_slli_epi32(_mm_add_epi32(xInt, _mm_set_epi32(2, 0, 2, 0)), 29));
         sgn = _mm_xor_ps(sgn, _mm_unpacklo_ps(x, _mm_setzero_ps()));
         sgn = _mm_and_ps(sgn, sgnMask);

  // Determine whether the taylor series is sine or cosine and
  // replace result with the co-function as necessary. This is
  // the reason why inputs are duplicated early in the function.
  __m128i swapMask = _mm_slli_epi32(_mm_add_epi32(xInt, _mm_set_epi32(-1, 1, -1, 1)), 30);
  __m128 sincos = blendv_packed(taylor, co, _mm_castsi128_ps(swapMask));

  return _mm_xor_ps(sincos, sgn);
}
#endif


/**
 * \brief Sine and cosine
 */
struct SinCos {
  float sin;
  float cos;
};


/**
 * \brief Computes approximate sine and cosine
 *
 * If possible, this leverages vectorization so that
 * computing the cosine comes at no additional cost.
 * \returns sin(x) and cos(x) (approx)
 */
inline SinCos approx_sincos(float x) {
#if defined(AS_HAS_X86_INTRINSICS)
  __m128 sincos = approx_sincos_vectorized(_mm_set1_ps(x));

  SinCos result;
  uint64_t tmp = _mm_cvtsi128_si64(_mm_castps_si128(sincos));
  std::memcpy(&result, &tmp, sizeof(tmp));
  return result;
#else
  SinCos result;
  result.sin = std::sinf(x);
  result.cos = std::cosf(x);
  return result;
#endif
}


inline float approx_sin(float x) {
  return approx_sincos(x).sin;
}


inline float approx_cos(float x) {
  return approx_sincos(x).cos;
}


/**
 * \brief Computes approximate tangent
 */
inline float approx_tan(float x) {
  SinCos sincos = approx_sincos(x);
  return approx_div(sincos.sin, sincos.cos);
}


/**
 * \brief Computes approximate cotangent
 */
inline float approx_cot(float x) {
  SinCos sincos = approx_sincos(x);
  return approx_div(sincos.cos, sincos.sin);
}


/**
 * \brief Tangent and cotangent
 */
struct TanCot {
  float tan;
  float cot;
};


/**
 * \brief Computes tangent and cotangent
 */
inline TanCot approx_tancot(float x) {
#if defined(AS_HAS_X86_INTRINSICS)
  __m128 sincos = approx_sincos_vectorized(_mm_set1_ps(x));
  __m128 cossin = _mm_shuffle_ps(sincos, sincos, 0xb1);

  __m128 tancot = approx_div_packed(sincos, cossin);

  TanCot result;
  uint64_t tmp = _mm_cvtsi128_si64(_mm_castps_si128(tancot));
  std::memcpy(&result, &tmp, sizeof(tmp));
  return result;
#else
  SinCos sincos = approx_sincos(x);

  TanCot result;
  result.tan = approx_div(sincos.sin, sincos.cos);
  result.cot = approx_div(sincos.cos, sincos.sin);
  return result;
#endif
}


/**
 * \brief Converts 32-bit float to 16-bit float
 *
 * Uses round-to-zero semantics.
 * \param [in] f32 32-bit float
 * \returns Encoded 16-bit float
 */
inline uint16_t f32tof16(float f32) {
  #ifdef __F16C__
  __m128i result = _mm_cvtps_ph(_mm_set_ss(f32), _MM_FROUND_TO_ZERO);
  return _mm_cvtsi128_si32(result);
  #else
  uint32_t u32;
  std::memcpy(&u32, &f32, 4);

  uint32_t exp32 = (u32 & 0x7F800000) >> 23;
  uint32_t frc32 = (u32 & 0x007FFFFF);

  uint32_t sgn16 = (u32 & 0x80000000) >> 16;
  uint32_t exp16, frc16;

  if (exp32 > 142) {
    if (exp32 == 0xFF) {
      // Infinity or NaN, preserve.
      exp16 = 0x1F;
      frc16 = frc32 >> 13;

      if (frc32)
        frc16 |= 0x200;
    } else {
      // Regular number that is larger what we can represent
      // with f16, return maximum representable number.
      exp16 = 0x1E;
      frc16 = 0x3FF;
    }
  } else if (exp32 < 113) {
    if (exp32 >= 103) {
      // Number can be represented as denorm
      exp16 = 0;
      frc16 = (0x0400 | (frc32 >> 13)) >> (113 - exp32);
    } else {
      // Number too small to be represented
      exp16 = 0;
      frc16 = 0;
    }
  } else {
    // Regular number
    exp16 = exp32 - 112;
    frc16 = frc32 >> 13;
  }

  return uint16_t(sgn16 | (exp16 << 10) | frc16);
  #endif
}


/**
 * \brief Converts 16-bit float to 32-bit float
 *
 * \param [in] f16 Encoded 16-bit float
 * \returns 32-bit float
 */
inline float f16tof32(uint16_t f16) {
  #ifdef __F16C__
  __m128 result = _mm_cvtph_ps(_mm_cvtsi32_si128(uint32_t(f16)));
  return _mm_cvtss_f32(result);
  #else
  uint32_t exp16 = uint32_t(f16 & 0x7C00) >> 10;
  uint32_t frc16 = uint32_t(f16 & 0x03FF);

  uint32_t sgn32 = uint32_t(f16 & 0x8000) << 16;
  uint32_t exp32, frc32;

  if (!exp16) {
    if (!frc16) {
      exp32 = 0;
      frc32 = 0;
    } else {
      // Denorm in 16-bit, but we can represent these
      // natively in 32-bit by adjusting the exponent.
      int32_t bit = findmsb(frc16);
      exp32 = 127 - 24 + bit;
      frc32 = (frc16 << (23 - bit)) & 0x007FFFFF;
    }
  } else if (exp16 == 0x1F) {
    // Infinity or NaN, preserve semantic meaning. 
    exp32 = 0xFF;
    frc32 = frc16 << 13;

    if (frc16)
      frc32 |= 0x400000;
  } else {
    // Regular finite number, adjust the exponent as
    // necessary and shift the fractional part.
    exp32 = exp16 + 112;
    frc32 = frc16 << 13;
  }

  float f32;
  uint32_t u32 = sgn32 | (exp32 << 23) | frc32;
  std::memcpy(&f32, &u32, sizeof(f32));
  return f32;
  #endif
}

}
