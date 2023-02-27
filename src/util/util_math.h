#pragma once

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
#ifdef __POPCNT__
  return _mm_popcnt_u32(n);
#else
  n = popcntStep(n, 0x55555555, 1);
  n = popcntStep(n, 0x33333333, 2);
  n = popcntStep(n, 0x0F0F0F0F, 4);
  n = popcntStep(n, 0x00FF00FF, 8);
  n = popcntStep(n, 0x0000FFFF, 16);
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
  __m128 half = _mm_set1_ps(0.5f);
  __m128 three = _mm_set1_ps(3.0f);
  __m128 x = _mm_rsqrt_ps(a);
  __m128 ax = _mm_mul_ps(a, x);

  return _mm_mul_ps(_mm_mul_ps(half, ax),
    fnmadd_packed(x, ax, three));
}


/**
 * \brief Computes packed approximate sin
 *
 * Exact in 0, pi/2 and pi. The maximum absolute error is roughly
 * 0.00109, the relative error is below 1.5% across the [0,pi/2]
 * range, except when close to zero. Derivatives around zero are
 * not exact as a result.
 * \returns sin(x) (approx.)
 */
#ifdef __SSE4_1__
inline __m128 approx_sin_packed(__m128 x) {
  __m128 rcppi = _mm_set1_ps(float(1.0 / pi));

  __m128 a = _mm_set1_ps(4.0f);
  __m128 b = _mm_set1_ps(0.225);

  __m128 absmask = _mm_castsi128_ps(_mm_set1_epi32(0x7fffffff));

  __m128 y = _mm_mul_ps(x, rcppi);
  __m128 t = _mm_round_ps(y, _MM_FROUND_TRUNC);
  __m128i i = _mm_slli_epi32(_mm_cvtps_epi32(t), 31);

  y = _mm_sub_ps(y, t);

  __m128 s = _mm_mul_ps(y, fnmadd_packed(_mm_and_ps(y, absmask), a, a));
  s = fmadd_packed(fmsub_packed(s, _mm_and_ps(s, absmask), s), b, s);
  return _mm_xor_ps(s, _mm_castsi128_ps(i));
}
#endif


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
 * \brief Blends vectors with variable mask
 * \returns blendvps result
 */
template<uint8_t Imm>
force_inline __m128 blend_packed(__m128 a, __m128 b) {
  #ifdef __SSE4_1__
  return _mm_blend_ps(a, b, Imm);
  #else
  __m128 mask = _mm_cvtps_epi32(_mm_set_epi32(
    Imm & 0x8 ? ~0u : 0, Imm & 0x4 ? ~0u : 0,
    Imm & 0x2 ? ~0u : 0, Imm & 0x1 ? ~0u : 0));

  return _mm_or_ps(
    _mm_andnot_ps(mask, a),
    _mm_and_ps(mask, b));
  #endif
}


/**
 * \brief Computes cross product
 *
 * The last component will be the product of the
 * last components of both vectors.
 * \returns a × b
 */
force_inline __m128 cross_packed(__m128 a, __m128 b) {
  uint8_t s = _MM_SHUFFLE(3, 0, 2, 1);

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
 * \brief Computes approximate sine
 * \returns sin(x) (approx.)
 */
inline float approx_sin(float x) {
#if defined(AS_HAS_X86_INTRINSICS) && defined(__SSE4_1__)
  return _mm_cvtss_f32(approx_sin_packed(_mm_set_ss(x)));
#else
  constexpr float rpi = float(1.0 / pi);

  float y = x * rpi;
  float t = std::trunc(y);
  y -= t;

  float s = y * (4.0f - 4.0f * std::abs(y));
  s += 0.225f * (s * std::abs(s) - s);

  return (uint32_t(t) & 1) ? -s : s;
#endif
}


/**
 * \brief Computes approximate cosine
 * \returns cos(x) (approx.)
 */
inline float approx_cos(float x) {
  return approx_sin(x + float(pi * 0.5));
}


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
  SinCos result;
#if defined(AS_HAS_X86_INTRINSICS) && defined(__SSE4_1__)
  __m128 xsin = _mm_set_ss(x);
  __m128 xcos = _mm_add_ss(xsin, _mm_set_ss(float(pi * 0.5)));
  __m128 packed = approx_sin_packed(_mm_unpacklo_ps(xsin, xcos));

  // This is all sorts of wonky, but appears to
  // be the best way to extract the lower 64 bits
  uint64_t tmp = _mm_cvtsi128_si64(_mm_castps_si128(packed));
  std::memcpy(&result, &tmp, sizeof(tmp));
#else
  result.sin = approx_sin(x);
  result.cos = approx_cos(x);
#endif
  return result;
}


/**
 * \brief Computes two approximate sine and cosine pairs
 *
 * If possible, this leverages vectorization so that the cost
 * is roughly the same as it one \c approx_sincos call.
 * \returns sincos(x) and sincos(y)
 */
inline std::pair<SinCos, SinCos> approx_sincos(float x, float y) {
#if defined(AS_HAS_X86_INTRINSICS) && defined(__SSE4_1__)
  std::pair<SinCos, SinCos> result;
  __m128 xysin = _mm_set_ps(0.0f, 0.0f, y, x);
  __m128 xycos = _mm_add_ps(xysin, _mm_set1_ps(float(pi * 0.5)));
  __m128 packed = approx_sin_packed(_mm_unpacklo_ps(xysin, xycos));

  uint64_t tmp0 = _mm_cvtsi128_si64(_mm_castps_si128(packed));
  uint64_t tmp1 = _mm_cvtsi128_si64(_mm_castps_si128(_mm_movehl_ps(packed, packed)));

  std::memcpy(&result.first, &tmp0, sizeof(tmp0));
  std::memcpy(&result.second, &tmp1, sizeof(tmp1));
  return result;
#else
  return std::make_pair(approx_sincos(x), approx_sincos(y));
#endif
}


/**
 * \brief Computes approximate tangent
 * \returns tan(x)
 */
inline float approx_tan(float x) {
  SinCos sincos = approx_sincos(x);
  return approx_div(sincos.sin, sincos.cos);
}


/**
 * \brief Computes approximate cotangent
 * \returns tan(x)
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
 * \brief Computes approximate tangent and cotangent
 *
 * Leverages vectorization so that this has no additional
 * cost over computing only one of the values.
 * \returns tan(x) and cot(x) (approx)
 */
inline TanCot approx_tancot(float x) {
  TanCot result;
#ifdef AS_HAS_X86_INTRINSICS
  #ifdef __SSE4_1__
  __m128 xsin = _mm_set_ss(x);
  __m128 xcos = _mm_add_ss(xsin, _mm_set_ss(float(pi * 0.5)));
  __m128 num = approx_sin_packed(_mm_unpacklo_ps(xsin, xcos));
  #else
  SinCos sincos = approx_sincos(x);
  __m128 num = _mm_set_ps(0.0f, 0.0f, sincos.cos, sincos.sin);
  #endif
  __m128 den = _mm_shuffle_ps(num, num, _MM_SHUFFLE(2, 3, 0, 1));
  __m128 quot = approx_div_packed(num, den);

  uint64_t tmp = _mm_cvtsi128_si64(_mm_castps_si128(quot));
  std::memcpy(&result, &tmp, sizeof(tmp));
#else
  SinCos sincos = approx_sincos(x);
  result.tan = approx_div(sincos.sin, sincos.cos);
  result.cot = approx_div(sincos.cos, sincos.sin);
#endif
  return result;
}


/**
 * \brief Computes two approximate tangent and cotangent pairs
 *
 * If possible, this leverages vectorization so that the cost
 * is roughly the same as it one \c approx_tancot call.
 * \returns tancot(x) and tancot(y) (approx)
 */
inline std::pair<TanCot, TanCot> approx_tancot(float x, float y) {
#ifdef AS_HAS_X86_INTRINSICS
  std::pair<TanCot, TanCot> result;
  #ifdef __SSE4_1__
  __m128 xysin = _mm_set_ps(0.0f, 0.0f, y, x);
  __m128 xycos = _mm_add_ps(xysin, _mm_set1_ps(float(pi * 0.5)));
  __m128 num = approx_sin_packed(_mm_unpacklo_ps(xysin, xycos));
  #else
  auto sincos = approx_sincos(x, y);
  __m128 num = _mm_set_ps(
    sincos.second.cos, sincos.second.sin,
    sincos.first.cos, sincos.first.sin);
  #endif
  __m128 den = _mm_shuffle_ps(num, num, _MM_SHUFFLE(2, 3, 0, 1));
  __m128 quot = approx_div_packed(num, den);

  uint64_t tmp0 = _mm_cvtsi128_si64(_mm_castps_si128(quot));
  uint64_t tmp1 = _mm_cvtsi128_si64(_mm_castps_si128(_mm_movehl_ps(quot, quot)));

  std::memcpy(&result.first, &tmp0, sizeof(tmp0));
  std::memcpy(&result.second, &tmp1, sizeof(tmp1));
  return result;
#else
  return std::make_pair(approx_tancot(x), approx_tancot(y));
#endif
}


}