#pragma once

#include <cstddef>
#include <cstdint>
#include <tuple>
#include <type_traits>
#include <utility>

#include "util_log.h"
#include "util_math.h"
#include "util_vector.h"

namespace as {

template<typename T, size_t N>
class Vector;


/**
 * \brief Helpers to scalarize vector constructor arguments
 *
 * Vector arguments need to be broken down into scalars so that they
 * can be trivially used to initialize the underlying array. These
 * functions will create a tuple of appropriately typed scalars.
 */
template<typename T, typename T_, std::enable_if_t<std::is_convertible_v<T_, T>, bool> = true>
auto makeVectorTuple(T_ arg) {
  return std::make_tuple(T(arg));
}

template<typename T, typename T_, size_t N_, size_t... Indices>
auto makeVectorTuple(std::integer_sequence<size_t, Indices...>, const Vector<T_, N_>& arg) {
  return std::make_tuple(T(arg.template at<Indices>())...);
}

template<typename T, typename T_, size_t N_>
auto makeVectorTuple(const Vector<T_, N_>& arg) {
  return makeVectorTuple<T>(std::make_index_sequence<N_>(), arg);
}


/**
 * \brief Helpers to create a tuple from a scalar
 *
 * Essentially creates a tuple that contains
 * the given scalar N times.
 */
template<typename... Tx>
auto makeScalarTuple(std::integral_constant<size_t, 0>(), Tx... args) {
  return std::make_tuple(args...);
}

template<size_t N, typename T, typename... Tx>
auto makeScalarTuple(T arg, Tx... args) {
  return makeScalarTuple(std::integral_constant<size_t, N - 1>(), arg, args..., arg);
}

template<typename T, size_t N>
auto makeScalarTuple(T arg) {
  return makeScalarTuple(std::integral_constant<size_t, N - 1>(), arg);
}


/**
 * \brief Computes vector type alignment
 *
 * Ensures that vectors with a power-of-two component
 * count are strongly aligned, i.e. the alignment will
 * be the same as the vector's size.
 * \tparam T Scalar type
 * \tparam N Component count
 * \returns Vector type alignment
 */
template<typename T, size_t N>
constexpr size_t getVectorAlign() {
  return sizeof(T) * (N & -N);
}


/**
 * \brief Vector
 *
 * This class is mostly intended for ease of use in situations where
 * two- or three-component vectors of any given type are needed,
 * rather than being aimed at high computational performance.
 * \tparam T Component type
 * \tparam N Component count
 */
template<typename T, size_t N>
class alignas(getVectorAlign<T, N>()) Vector {

public:

  static constexpr size_t Components = N;

  using ScalarType = T;

  Vector() = default;

  /**
   * \brief Initializes vector with scalar
   *
   * Broadcasts given scalar to all elements.
   * \param [in] arg Scalar
   */
  explicit Vector(T arg)
  : Vector(makeScalarTuple<T, N>(arg)) { }

  /**
   * \brief Initializes vector
   *
   * Arguments can be scalars of the given type, as well
   * as vectors, provided that the number of components
   * is at least \c N.
   * \param [in] args Arguments
   */
  template<typename... Tx>
  explicit Vector(const Tx&... args)
  : Vector(std::tuple_cat(makeVectorTuple<T>(args)...)) { }

  /**
   * \brief Initializes vector from scalar tuple
   * \param [in] args Tuple of scalars
   */
  template<typename... Tx>
  explicit Vector(const std::tuple<Tx...>& args)
  : Vector(std::make_index_sequence<N>(), args) { }

  /**
   * \brief Component-wise addition
   *
   * \param [in] vector Source operand
   * \returns Reference to self
   */
  Vector& operator += (const Vector& vector) {
    for (size_t i = 0; i < N; i++)
      m_data[i] += vector.m_data[i];
    return *this;
  }

  /**
   * \brief Component-wise subtraction
   *
   * \param [in] vector Source operand
   * \returns Reference to self
   */
  Vector& operator -= (const Vector& vector) {
    for (size_t i = 0; i < N; i++)
      m_data[i] -= vector.m_data[i];
    return *this;
  }

  /**
   * \brief Component-wise multiplication
   *
   * \param [in] vector Source operand
   * \returns Reference to self
   */
  Vector& operator *= (const Vector& vector) {
    for (size_t i = 0; i < N; i++)
      m_data[i] *= vector.m_data[i];
    return *this;
  }

  /**
   * \brief Component-wise division
   *
   * \param [in] vector Source operand
   * \returns Reference to self
   */
  Vector& operator /= (const Vector& vector) {
    for (size_t i = 0; i < N; i++)
      m_data[i] /= vector.m_data[i];
    return *this;
  }

  /**
   * \brief Component-wise right shift
   *
   * \param [in] vector Source operand
   * \returns Reference to self
   */
  Vector& operator >>= (const Vector& vector) {
    for (size_t i = 0; i < N; i++)
      m_data[i] >>= vector.m_data[i];
    return *this;
  }

  /**
   * \brief Component-wise left shift
   *
   * \param [in] vector Source operand
   * \returns Reference to self
   */
  Vector& operator <<= (const Vector& vector) {
    for (size_t i = 0; i < N; i++)
      m_data[i] <<= vector.m_data[i];
    return *this;
  }

  /**
   * \brief Component-wise addition by scalar
   *
   * \param [in] scalar Source operand
   * \returns Reference to self
   */
  Vector& operator += (T scalar) {
    for (size_t i = 0; i < N; i++)
      m_data[i] += scalar;
    return *this;
  }

  /**
   * \brief Component-wise subtraction by scalar
   *
   * \param [in] scalar Source operand
   * \returns Reference to self
   */
  Vector& operator -= (T scalar) {
    for (size_t i = 0; i < N; i++)
      m_data[i] -= scalar;
    return *this;
  }

  /**
   * \brief Component-wise multiplication by scalar
   *
   * \param [in] scalar Source operand
   * \returns Reference to self
   */
  Vector& operator *= (T scalar) {
    for (size_t i = 0; i < N; i++)
      m_data[i] *= scalar;
    return *this;
  }

  /**
   * \brief Component-wise division by scalar
   *
   * \param [in] scalar Source operand
   * \returns Reference to self
   */
  Vector& operator /= (T scalar) {
    for (size_t i = 0; i < N; i++)
      m_data[i] /= scalar;
    return *this;
  }

  /**
   * \brief Component-wise right shift by scalar
   *
   * \param [in] scalar Source operand
   * \returns Reference to self
   */
  Vector& operator >>= (T scalar) {
    for (size_t i = 0; i < N; i++)
      m_data[i] >>= scalar;
    return *this;
  }

  /**
   * \brief Component-wise left shift by scalar
   *
   * \param [in] scalar Source operand
   * \returns Reference to self
   */
  Vector& operator <<= (T scalar) {
    for (size_t i = 0; i < N; i++)
      m_data[i] <<= scalar;
    return *this;
  }

  /**
   * \brief Component-wise addition
   *
   * \param [in] vector Source operand
   * \returns Resulting vector
   */
  Vector operator + (const Vector& vector) const {
    Vector result;
    for (size_t i = 0; i < N; i++)
      result.m_data[i] = m_data[i] + vector.m_data[i];
    return result;
  }

  /**
   * \brief Component-wise subtraction
   *
   * \param [in] vector Source operand
   * \returns Resulting vector
   */
  Vector operator - (const Vector& vector) const {
    Vector result;
    for (size_t i = 0; i < N; i++)
      result.m_data[i] = m_data[i] - vector.m_data[i];
    return result;
  }

  /**
   * \brief Component-wise multiplication
   *
   * \param [in] vector Source operand
   * \returns Resulting vector
   */
  Vector operator * (const Vector& vector) const {
    Vector result;
    for (size_t i = 0; i < N; i++)
      result.m_data[i] = m_data[i] * vector.m_data[i];
    return result;
  }

  /**
   * \brief Component-wise division
   *
   * \param [in] vector Source operand
   * \returns Resulting vector
   */
  Vector operator / (const Vector& vector) const {
    Vector result;
    for (size_t i = 0; i < N; i++)
      result.m_data[i] = m_data[i] / vector.m_data[i];
    return result;
  }

  /**
   * \brief Component-wise right shift
   *
   * \param [in] vector Source operand
   * \returns Resulting vector
   */
  Vector operator >> (const Vector& vector) const {
    Vector result;
    for (size_t i = 0; i < N; i++)
      result.m_data[i] = m_data[i] >> vector.m_data[i];
    return result;
  }

  /**
   * \brief Component-wise left shift
   *
   * \param [in] vector Source operand
   * \returns Resulting vector
   */
  Vector operator << (const Vector& vector) const {
    Vector result;
    for (size_t i = 0; i < N; i++)
      result.m_data[i] = m_data[i] << vector.m_data[i];
    return result;
  }

  /**
   * \brief Component-wise addition by scalar
   *
   * \param [in] vector Source operand
   * \returns Resulting vector
   */
  Vector operator + (T scalar) const {
    Vector result;
    for (size_t i = 0; i < N; i++)
      result.m_data[i] = m_data[i] + scalar;
    return result;
  }

  /**
   * \brief Component-wise subtraction by scalar
   *
   * \param [in] vector Source operand
   * \returns Resulting vector
   */
  Vector operator - (T scalar) const {
    Vector result;
    for (size_t i = 0; i < N; i++)
      result.m_data[i] = m_data[i] - scalar;
    return result;
  }

  /**
   * \brief Component-wise multiplication by scalar
   *
   * \param [in] vector Source operand
   * \returns Resulting vector
   */
  Vector operator * (T scalar) const {
    Vector result;
    for (size_t i = 0; i < N; i++)
      result.m_data[i] = m_data[i] * scalar;
    return result;
  }

  /**
   * \brief Component-wise division by scalar
   *
   * \param [in] vector Source operand
   * \returns Resulting vector
   */
  Vector operator / (T scalar) const {
    Vector result;
    for (size_t i = 0; i < N; i++)
      result.m_data[i] = m_data[i] / scalar;
    return result;
  }

  /**
   * \brief Component-wise right shift by scalar
   *
   * \param [in] vector Source operand
   * \returns Resulting vector
   */
  Vector operator >> (T scalar) const {
    Vector result;
    for (size_t i = 0; i < N; i++)
      result.m_data[i] = m_data[i] >> scalar;
    return result;
  }

  /**
   * \brief Component-wise left shift by scalar
   *
   * \param [in] vector Source operand
   * \returns Resulting vector
   */
  Vector operator << (T scalar) const {
    Vector result;
    for (size_t i = 0; i < N; i++)
      result.m_data[i] = m_data[i] << scalar;
    return result;
  }

  /**
   * \brief Component-wise negation
   * \returns Resulting vector
   */
  Vector operator - () const {
    Vector result;
    for (size_t i = 0; i < N; i++)
      result.m_data[i] = -m_data[i];
    return result;
  }

  /**
   * \brief Retrieves given vector element
   *
   * \param [in] idx Element index
   * \returns Reference to given element
   */
  T operator [] (uint32_t idx) const {
    return m_data[idx];
  }

  /**
   * \brief Retrieves element with compile-time index
   *
   * \tparam I Element index
   * \returns Reference to given element
   */
  template<size_t I, std::enable_if_t<(I < N), bool> = true>
  T at() const {
    return m_data[I];
  }

  /**
   * \brief Swizzles vector or extracts elements
   *
   * Can be used to extract a sub-vector. Duplicate indices
   * are allowed, so the resulting vector may be bigger than
   * the source vector.
   * \tparam Indices Element indices to extract
   * \returns Resulting vector
   */
  template<size_t... Indices>
  auto get() const {
    return Vector<T, sizeof...(Indices)>(at<Indices>()...);
  }

  /**
   * \brief Sets element with compile-time index
   *
   * \tparam I Element index
   * \param [in] value New value
   */
  template<size_t I, std::enable_if_t<(I < Components), bool> = true>
  Vector& set(ScalarType value) {
    m_data[I] = value;
    return *this;
  }

  /**
   * \brief Sets element
   *
   * \param [in] idx Element index
   * \param [in] value New value
   */
  Vector& set(uint32_t idx, T value) {
    m_data[idx] = value;
    return *this;
  }

  bool operator == (const Vector& vector) const = default;
  bool operator != (const Vector& vector) const = default;

private:

  template<size_t... Indices, typename... Tx>
  explicit Vector(std::integer_sequence<size_t, Indices...>, const std::tuple<Tx...>& args)
  : m_data { T(std::get<Indices>(args))... } { }

  T m_data[N];

};


template<typename T, size_t N, typename Fn, size_t... Idx>
auto apply(std::integer_sequence<size_t, Idx...>, const Vector<T, N>& v, const Fn& fn) {
  return Vector<decltype(fn.operator () (T())), N>(fn(v.template at<Idx>())...);
}


/**
 * \brief Applies a function to all vector elements
 *
 * \param [in] vector Input vector
 * \param [in] fn Function
 * \returns Vector of transformed elements
 */
template<typename T, size_t N, typename Fn>
auto apply(Vector<T, N> v, const Fn& fn) {
  return apply(std::make_index_sequence<N>(), v, fn);
}


/**
 * \brief Computes absolute value of vector
 * \returns Component-wise absolute value
 */
template<typename T, size_t N>
Vector<T, N> abs(Vector<T, N> v) {
  return apply(v, [] (T x) { return T(std::abs(x)); });
}


#ifdef AS_HAS_X86_INTRINSICS

/**
 * \brief Four-component float vector
 */
template<>
class alignas(__m128) Vector<float, 4> {

public:

  static constexpr size_t Components = 4;

  using ScalarType = float;

  Vector() = default;

  explicit Vector(__m128 data)
  : m_data(data) { }

  explicit Vector(float f)
  : m_data(_mm_set1_ps(f)) { }

  template<typename... Tx>
  explicit Vector(const Tx&... args)
  : Vector(std::tuple_cat(makeVectorTuple<ScalarType>(args)...)) { }

  template<typename... Tx>
  explicit Vector(const std::tuple<float, float, float, float, Tx...>& args)
  : Vector(_mm_set_ps(std::get<3>(args), std::get<2>(args), std::get<1>(args), std::get<0>(args))) { }

  Vector& operator += (Vector vector) {
    m_data = _mm_add_ps(m_data, vector.m_data);
    return *this;
  }

  Vector& operator -= (Vector vector) {
    m_data = _mm_sub_ps(m_data, vector.m_data);
    return *this;
  }

  Vector& operator *= (Vector vector) {
    m_data = _mm_mul_ps(m_data, vector.m_data);
    return *this;
  }

  Vector& operator /= (Vector vector) {
    m_data = _mm_div_ps(m_data, vector.m_data);
    return *this;
  }

  Vector& operator += (ScalarType scalar) {
    m_data = _mm_add_ps(m_data, _mm_set1_ps(scalar));
    return *this;
  }

  Vector& operator -= (ScalarType scalar) {
    m_data = _mm_sub_ps(m_data, _mm_set1_ps(scalar));
    return *this;
  }

  Vector& operator *= (ScalarType scalar) {
    m_data = _mm_mul_ps(m_data, _mm_set1_ps(scalar));
    return *this;
  }

  Vector& operator /= (ScalarType scalar) {
    m_data = _mm_div_ps(m_data, _mm_set1_ps(scalar));
    return *this;
  }

  Vector operator + (Vector vector) const {
    return Vector(_mm_add_ps(m_data, vector.m_data));
  }

  Vector operator - (Vector vector) const {
    return Vector(_mm_sub_ps(m_data, vector.m_data));
  }

  Vector operator * (Vector vector) const {
    return Vector(_mm_mul_ps(m_data, vector.m_data));
  }

  Vector operator / (Vector vector) const {
    return Vector(_mm_div_ps(m_data, vector.m_data));
  }

  Vector operator + (ScalarType scalar) const {
    return Vector(_mm_add_ps(m_data, _mm_set1_ps(scalar)));
  }

  Vector operator - (ScalarType scalar) const {
    return Vector(_mm_sub_ps(m_data, _mm_set1_ps(scalar)));
  }

  Vector operator * (ScalarType scalar) const {
    return Vector(_mm_mul_ps(m_data, _mm_set1_ps(scalar)));
  }

  Vector operator / (ScalarType scalar) const {
    return Vector(_mm_div_ps(m_data, _mm_set1_ps(scalar)));
  }

  Vector operator - () const {
    return Vector(neg_packed(m_data));
  }

  ScalarType operator [] (uint32_t idx) const {
    alignas(16) float data[4];
    _mm_store_ps(data, m_data);
    return data[idx];
  }

  template<size_t I, std::enable_if_t<(I < Components), bool> = true>
  ScalarType at() const {
    if constexpr (I == 0)
      return _mm_cvtss_f32(m_data);
    else
      return _mm_cvtss_f32(_mm_shuffle_ps(m_data, m_data, I));
  }

  template<size_t... Indices>
  auto get() const {
    return Vector<ScalarType, sizeof...(Indices)>(at<Indices>()...);
  }

  template<size_t I, std::enable_if_t<(I < Components), bool> = true>
  Vector& set(ScalarType value) {
#ifdef __SSE4_1__
    m_data = _mm_insert_ps(m_data, _mm_load_ss(&value), I << 4);
    return *this;
#else
    return set(I, value);
#endif
  }

  Vector& set(uint32_t idx, ScalarType value) {
    alignas(16) float data[4];
    _mm_store_ps(data, m_data);
    data[idx] = value;
    m_data = _mm_load_ps(data);
    return *this;
  }

  bool operator == (Vector vector) const {
    __m128 compare = _mm_cmpeq_ps(m_data, vector.m_data);
    return _mm_movemask_ps(compare) == 0xF;
  }

  bool operator != (Vector vector) const {
    return !operator == (vector);
  }

  explicit operator __m128 () const {
    return m_data;
  }

private:

  __m128  m_data;

};


inline Vector<float, 4> fmadd(Vector<float, 4> a, Vector<float, 4> b, Vector<float, 4> c) {
  return Vector<float, 4>(fmadd_packed(__m128(a), __m128(b), __m128(c)));
}

inline Vector<float, 4> fnmadd(Vector<float, 4> a, Vector<float, 4> b, Vector<float, 4> c) {
  return Vector<float, 4>(fnmadd_packed(__m128(a), __m128(b), __m128(c)));
}

inline Vector<float, 4> fmsub(Vector<float, 4> a, Vector<float, 4> b, Vector<float, 4> c) {
  return Vector<float, 4>(fmsub_packed(__m128(a), __m128(b), __m128(c)));
}

inline Vector<float, 4> fnmsub(Vector<float, 4> a, Vector<float, 4> b, Vector<float, 4> c) {
  return Vector<float, 4>(fnmsub_packed(__m128(a), __m128(b), __m128(c)));
}

inline Vector<float, 4> abs(Vector<float, 4> a) {
  return Vector<float, 4>(abs_packed(__m128(a)));
}

inline Vector<float, 4> approx_rcp(Vector<float, 4> a) {
  return Vector<float, 4>(approx_rcp_packed(__m128(a)));
}

inline Vector<float, 4> approx_div(Vector<float, 4> a, Vector<float, 4> b) {
  return Vector<float, 4>(approx_div_packed(__m128(a), __m128(b)));
}

inline Vector<float, 4> approx_sqrt(Vector<float, 4> a) {
  return Vector<float, 4>(approx_sqrt_packed(__m128(a)));
}

inline Vector<float, 4> approx_rsqrt(Vector<float, 4> a) {
  return Vector<float, 4>(approx_rsqrt_packed(__m128(a)));
}

inline Vector<float, 4> approx_sin(Vector<float, 4> a) {
  return Vector<float, 4>(approx_sin_packed(__m128(a)));
}

inline Vector<float, 4> approx_cos(Vector<float, 4> a) {
  return Vector<float, 4>(approx_sin_packed(
    _mm_add_ps(__m128(a), _mm_set1_ps(float(pi / 2)))));
}

#endif

using Vector2D = Vector<float, 2>;
using Vector3D = Vector<float, 3>;
using Vector4D = Vector<float, 4>;

}
