#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace as {

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
class Vector {

public:

  Vector() = default;

  /**
   * \brief Initializes vector
   *
   * Arguments can be scalars of the given type, as well
   * as vectors, provided that the number of components
   * matches \c N.
   * \param [in] args Arguments
   */
  template<typename... Tx>
  explicit Vector(const Tx&... args) {
    init(std::integral_constant<size_t, 0>(), args...);
  }

  /**
   * \brief Initializes vector from a larger vector
   * \param [in] vector Vector to initialize 
   */
  template<size_t M, std::enable_if_t<(M > N), bool> = true>
  explicit Vector(const Vector<T, M>& vector) {
    insert<0>(std::make_integer_sequence<size_t, N>(), vector);
  }

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
  const T& operator [] (uint32_t idx) const {
    return m_data[idx];
  }

  /**
   * \brief Retrieves given vector element for writing
   *
   * \param [in] idx Element index
   * \returns Reference to given element
   */
  T& operator [] (uint32_t idx) {
    return m_data[idx];
  }

  /**
   * \brief Retrieves element with compile-time index
   *
   * \tparam I Element index
   * \returns Reference to given element
   */
  template<size_t I, std::enable_if_t<I < N, bool> = true>
  const T& at() const {
    return m_data[I];
  }

  /**
   * \brief Retrieves element with compile-time index for writing
   *
   * \tparam I Element index
   * \returns Reference to given element
   */
  template<size_t I, std::enable_if_t<I < N, bool> = true>
  T& at() {
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

  bool operator == (const Vector& vector) const = default;
  bool operator != (const Vector& vector) const = default;

private:

  template<size_t I, typename... Tx>
  void init(std::integral_constant<size_t, I>, T arg, const Tx&... args) {
    at<I>() = arg;
    init(std::integral_constant<size_t, I + 1>(), args...);
  }

  template<size_t I, typename T_, size_t N_, typename... Tx>
  void init(std::integral_constant<size_t, I>, const Vector<T_, N_>& arg, const Tx&... args) {
    insert<I>(std::make_index_sequence<N_>(), arg);
    init(std::integral_constant<size_t, I + N_>(), args...);
  }

  void init(std::integral_constant<size_t, N>) { }

  template<size_t I, typename T_, size_t N_, size_t... Indices>
  void insert(std::integer_sequence<size_t, Indices...>, const Vector<T_, N_>& arg) {
    std::tie(at<I + Indices>()...) = std::make_tuple(arg.template at<Indices>()...);
  }

  T m_data[N];

};


using Offset2D = Vector<int32_t, 2>;
using Extent2D = Vector<uint32_t, 2>;
using Vector2D = Vector<float, 2>;

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


using Offset3D = Vector<int32_t, 3>;
using Extent3D = Vector<uint32_t, 3>;
using Vector3D = Vector<float, 3>;

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
