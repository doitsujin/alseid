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
template<size_t N, typename T>
auto makeScalarTuple(T arg) {
  if constexpr (N == 1) {
    return std::make_tuple(arg);
  } else {
    return std::tuple_cat(std::make_tuple(arg),
      makeScalarTuple<N - 1>(arg));
  }
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
  : Vector(makeScalarTuple<N, T>(arg)) { }

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
   * \brief Broadcasts a single component
   *
   * Equivalent to creatng a new vector from a given
   * scalar, but potentially more efficient.
   * \tparam I Element index
   * \returns Vector with the given element in all component
   */
  template<size_t I>
  Vector broadcast() const {
    return Vector(at<I>());
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

  /**
   * \brief Negates a specific set of components
   */
  template<size_t... Ix>
  Vector negate() const {
    return negate(std::make_index_sequence<N>(), std::integer_sequence<size_t, Ix...>());
  }

  bool operator == (const Vector& vector) const = default;
  bool operator != (const Vector& vector) const = default;

private:

  template<size_t... Indices, typename... Tx>
  explicit Vector(std::integer_sequence<size_t, Indices...>, const std::tuple<Tx...>& args)
  : m_data { T(std::get<Indices>(args))... } { }

  T m_data[N];

  template<size_t... Sequence, size_t... Ix>
  Vector negate(std::integer_sequence<size_t, Sequence...>, std::integer_sequence<size_t, Ix...> ix) const {
    return Vector(negateComponent<Sequence>(ix)...);
  }

  template<size_t S, size_t... Ix>
  T negateComponent(std::integer_sequence<size_t, Ix...>) const {
    return (... || (S == Ix)) ? -m_data[S] : m_data[S];
  }

};


template<typename T, size_t N, typename Fn, size_t... Idx>
auto apply(std::integer_sequence<size_t, Idx...>, const Vector<T, N>& v, const Fn& fn) {
  return Vector<decltype(fn.operator () (T())), N>(fn(v.template at<Idx>())...);
}


template<typename T, size_t N, typename Fn, size_t... Idx>
auto apply(std::integer_sequence<size_t, Idx...>, const Vector<T, N>& a, const Vector<T, N>& b, const Fn& fn) {
  return Vector<decltype(fn.operator () (T(), T())), N>(fn(a.template at<Idx>(), b.template at<Idx>())...);
}


template<typename T, size_t N, typename V, typename Fn>
V foldr(std::integer_sequence<size_t>, const Vector<T, N>& v, const Fn& fn, V init) {
  return init;
}


template<typename T, size_t N, typename V, typename Fn, size_t I, size_t... Idx>
V foldr(std::integer_sequence<size_t, I, Idx...>, const Vector<T, N>& v, const Fn& fn, V init) {
  return fn(v.template at<I>(), foldr(std::integer_sequence<size_t, Idx...>(), v, fn, init));
}


/**
 * \brief Applies unary function to all vector elements
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
 * \brief Applies binary function to all vector elements
 *
 * \param [in] vector Input vector
 * \param [in] fn Function
 * \returns Vector of transformed elements
 */
template<typename T, size_t N, typename Fn>
auto apply(Vector<T, N> a, Vector<T, N> b, const Fn& fn) {
  return apply(std::make_index_sequence<N>(), a, b, fn);
}


/**
 * \brief Performs right-fold over vector elements
 *
 * \param [in] vector Input vector
 * \param [in] fn Function
 * \returns Function result
 */
template<typename T, size_t N, typename V, typename Fn>
V foldr(Vector<T, N> v, const Fn& fn, V init) {
  return foldr(std::make_index_sequence<N>(), v, fn, init);
}


/**
 * \brief Computes absolute value of vector
 * \returns Component-wise absolute value
 */
template<typename T, size_t N>
Vector<T, N> abs(Vector<T, N> v) {
  return apply(v, [] (T x) { return T(std::abs(x)); });
}


/**
 * \brief Computes component-wise minimum
 * \returns Minimum of two vectors
 */
template<typename T, size_t N>
Vector<T, N> min(Vector<T, N> a, Vector<T, N> b) {
  return apply(a, b, [] (T x, T y) { return T(std::min(x, y)); });
}


/**
 * \brief Computes component-wise maximum
 * \returns Maximum of two vectors
 */
template<typename T, size_t N>
Vector<T, N> max(Vector<T, N> a, Vector<T, N> b) {
  return apply(a, b, [] (T x, T y) { return T(std::max(x, y)); });
}


/**
 * \brief Computes vector dot product
 *
 * \param [in] a First vector
 * \param [in] b Second vector
 * \returns Dot product
 */
template<typename T, size_t N>
T dot(Vector<T, N> a, Vector<T, N> b) {
  return foldr(a * b, [] (T a, T b) { return a + b; }, 0.0f);
}


/**
 * \brief Computes cross product
 *
 * \param [in] a First vector
 * \param [in] b Second vector
 * \returns Cross product
 */
template<typename T>
Vector<T, 3> cross(Vector<T, 3> a, Vector<T, 3> b) {
  return Vector<T, 3>(
    a.template at<1>() * b.template at<2>() - a.template at<2>() * b.template at<1>(),
    a.template at<2>() * b.template at<0>() - a.template at<0>() * b.template at<2>(),
    a.template at<0>() * b.template at<1>() - a.template at<1>() * b.template at<0>());
}


/**
 * \brief Computes cross product with 4D vectors
 *
 * Since the cross product is only defined in 3D space, the
 * 4th component will just contain the component product.
 * \param [in] a First vector
 * \param [in] b Second vector
 * \returns Cross product
 */
template<typename T>
Vector<T, 4> cross(Vector<T, 4> a, Vector<T, 4> b) {
  return Vector<T, 4>(
    a.template at<1>() * b.template at<2>() - a.template at<2>() * b.template at<1>(),
    a.template at<2>() * b.template at<0>() - a.template at<0>() * b.template at<2>(),
    a.template at<0>() * b.template at<1>() - a.template at<1>() * b.template at<0>(),
    a.template at<3>() * b.template at<3>());
}


/**
 * \brief Computes length of a vector
 *
 * \param [in] a Vector
 * \returns Vector length
 */
template<size_t N>
float length(Vector<float, N> a) {
  return approx_sqrt(dot(a, a));
}

template<size_t N>
double length(Vector<double, N> a) {
  return std::sqrt(dot(a, a));
}


/**
 * \brief Normalizes a vector
 *
 * \param [in] a Vector
 * \returns Normalized vector
 */
template<size_t N>
Vector<float, N> normalize(Vector<float, N> a) {
  return a * approx_rcp(length(a));
}

template<size_t N>
Vector<double, N> normalize(Vector<double, N> a) {
  return a / length(a);
}


/**
 * \brief Normalizes a plane equation
 *
 * Divides all components by the
 * length of the normal vector.
 * \param [in] plane Plane equation
 * \returns Normalized plane
 */
template<typename T>
Vector<T, 4> normalizePlane(
        Vector<T, 4>                  plane) {
  Vector<T, 4> normal = plane;
  normal.template set<3>(0.0f);

  return plane * approx_rsqrt(dot(normal, normal));
}


/**
 * \brief Subtracts even components and adds odd ones
 *
 * \param [in] a First vector
 * \param [in] b Second vector
 * \returns Resulting vector
 */
template<typename T>
Vector<T, 2> addsub(Vector<T, 2> a, Vector<T, 2> b) {
  Vector<T, 2> x = a - b;
  Vector<T, 2> y = a + b;

  return Vector<T, 2>(x.template at<0>(), y.template at<1>());
}

template<typename T>
Vector<T, 4> addsub(Vector<T, 4> a, Vector<T, 4> b) {
  Vector<T, 4> x = a - b;
  Vector<T, 4> y = a + b;

  return Vector<T, 4>(
    x.template at<0>(), y.template at<1>(),
    x.template at<2>(), y.template at<3>());
}


/**
 * \brief Multiply-adds and multiply-subtracts vectors
 *
 * \param [in] a First vector to multiply
 * \param [in] b Second vector to multiply
 * \param [in] c Vector to add or subtract
 * \returns Resulting vector
 */
template<typename T, size_t N>
Vector<T, N> fmaddsub(Vector<T, N> a, Vector<T, N> b, Vector<T, N> c) {
  return addsub(a * b, c);
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
    #ifdef __AVX__
    return _mm_cvtss_f32(_mm_permutevar_ps(m_data, _mm_cvtsi32_si128(idx)));
    #else
    alignas(16) float data[4];
    _mm_store_ps(data, m_data);
    return data[idx];
    #endif
  }

  template<size_t I, std::enable_if_t<(I < Components), bool> = true>
  ScalarType at() const {
    if constexpr (I == 0)
      return _mm_cvtss_f32(m_data);
    else
      return _mm_cvtss_f32(_mm_shuffle_ps(m_data, m_data, I));
  }

  template<size_t... Indices, std::enable_if_t<sizeof...(Indices) != 4, bool> = true>
  auto get() const {
    return Vector<ScalarType, sizeof...(Indices)>(at<Indices>()...);
  }

  template<size_t I0, size_t I1, size_t I2, size_t I3>
  auto get() const {
    return Vector(_mm_shuffle_ps(m_data, m_data,
      _MM_SHUFFLE(I3, I2, I1, I0)));
  }

  template<size_t I>
  Vector broadcast() const {
    return Vector(_mm_shuffle_ps(m_data, m_data, I * 0x55));
  }

  template<size_t I, std::enable_if_t<(I < Components), bool> = true>
  Vector& set(ScalarType value) {
#ifdef __SSE4_1__
    m_data = _mm_insert_ps(m_data, _mm_set_ss(value), I << 4);
#else
    constexpr uint8_t mask = _MM_SHUFFLE(
      I == 3 ? 0 : 3, I == 2 ? 0 : 2, I == 1 ? 0 : 1, I);
    if constexpr (I)
      m_data = _mm_shuffle_ps(m_data, m_data, mask);
    m_data = _mm_move_ss(m_data, _mm_set_ss(value));
    if constexpr (I)
      m_data = _mm_shuffle_ps(m_data, m_data, mask);
#endif

    return *this;
  }

  Vector& set(uint32_t idx, ScalarType value) {
    __m128 mask = _mm_castsi128_ps(_mm_cmpeq_epi32(
      _mm_set1_epi32(idx),
      _mm_set_epi32(3, 2, 1, 0)));

    __m128 val = _mm_set1_ps(value);

    #ifdef __SSE4_1__
    m_data = _mm_blendv_ps(m_data, val, mask);
    #else
    m_data = _mm_or_ps(
      _mm_andnot_ps(mask, m_data),
      _mm_and_ps(mask, val));
    #endif

    return *this;
  }

  template<size_t... Ix>
  Vector negate() const {
    __m128 mask = _mm_castsi128_ps(_mm_set_epi32(
      getSignMask<3, Ix...>(), getSignMask<2, Ix...>(),
      getSignMask<1, Ix...>(), getSignMask<0, Ix...>()));

    return Vector(_mm_xor_ps(m_data, mask));
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

  template<size_t S, size_t... Ix>
  static constexpr uint32_t getSignMask() {
    return (... || (S == Ix)) ? 0x80000000u : 0u;
  }

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

inline Vector<float, 4> addsub(Vector<float, 4> a, Vector<float, 4> b) {
  return Vector<float, 4>(addsub_packed(__m128(a), __m128(b)));
}

inline Vector<float, 4> fmaddsub(Vector<float, 4> a, Vector<float, 4> b, Vector<float, 4> c) {
  return Vector<float, 4>(fmaddsub_packed(__m128(a), __m128(b), __m128(c)));
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

inline float dot(Vector<float, 4> a, Vector<float, 4> b) {
  return _mm_cvtss_f32(dot_packed_one(__m128(a), __m128(b)));
}

inline Vector<float, 4> cross(Vector<float, 4> a, Vector<float, 4> b) {
  return Vector<float, 4>(cross_packed(__m128(a), __m128(b)));
}

inline float length(Vector<float, 4> a) {
  return _mm_cvtss_f32(approx_sqrt_packed(dot_packed_one(__m128(a), __m128(a))));
}

inline Vector<float, 4> normalize(Vector<float, 4> a) {
  return Vector<float, 4>(_mm_mul_ps(__m128(a),
    approx_rsqrt_packed(dot_packed(__m128(a), __m128(a)))));
}

inline Vector<float, 4> min(Vector<float, 4> a, Vector<float, 4> b) {
  return Vector<float, 4>(_mm_min_ps(__m128(a), __m128(b)));
}

inline Vector<float, 4> max(Vector<float, 4> a, Vector<float, 4> b) {
  return Vector<float, 4>(_mm_max_ps(__m128(a), __m128(b)));
}

inline Vector<float, 4> clamp(Vector<float, 4> a, Vector<float, 4> lo, Vector<float, 4> hi) {
  return Vector<float, 4>(_mm_max_ps(_mm_min_ps(__m128(a), __m128(hi)), __m128(lo)));
}

#endif

using Vector2D = Vector<float, 2>;
using Vector3D = Vector<float, 3>;
using Vector4D = Vector<float, 4>;

}
