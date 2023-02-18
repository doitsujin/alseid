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

  static constexpr size_t Components = N;

  using ScalarType = T;

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


/**
 * \brief Matrix
 *
 * Simple matrix type. Much like the \ref Vector class, this is
 * \e not aimed to provide high performance, and only provides
 * very basic functionality. The data layout is column-major to
 * match graphics APIs.
 * \tparam T Scalar type
 * \tparam N Row count
 * \tparam M Column count
 */
template<typename T, size_t N, size_t M>
class Matrix {

public:

  static constexpr size_t Rows = N;
  static constexpr size_t Cols = M;

  using ScalarType = T;
  using VectorType = Vector<T, N>;

  Matrix() = default;

  /**
   * \brief Initializes matrix
   *
   * Initializes matrix with the given
   * vector arguments.
   * \param [in] args Arguments
   */
  template<typename... Tx>
  explicit Matrix(const Tx&... args) {
    init(std::integral_constant<size_t, 0>(), args...);
  }

  /**
   * \brief Matrix scaling
   *
   * \param [in] factor Source operand
   * \returns Reference to self
   */
  Matrix& operator *= (T factor) {
    for (size_t c = 0; c < Cols; c++) {
      for (size_t r = 0; r < Rows; r++)
        m_cols[c][r] *= factor;
    }

    return *this;
  }

  /**
   * \brief Matrix product
   *
   * This only works for square matrices and
   * only serves as a convenience method.
   * \param [in] matrix Source operand
   * \returns Reference to self
   */
  template<std::enable_if_t<Rows == Cols, bool> = true>
  Matrix& operator *= (const Matrix& matrix) {
    return (*this = ((*this) * matrix));
  }

  /**
   * \brief Matrix scaling
   *
   * \param [in] factor Source operand
   * \returns Resulting matrix
   */
  Matrix operator * (T factor) {
    Matrix result;

    for (size_t c = 0; c < Cols; c++) {
      for (size_t r = 0; r < Rows; r++)
        result.m_cols[c][r] = m_cols[c][r] * factor;
    }

    return result;
  }

  /**
   * \brief Matrix product
   *
   * \param [in] matrix Source operand
   * \returns Resulting matrix
   */
  template<size_t M_>
  Matrix<T, N, M_> operator * (const Matrix<T, M, M_>& matrix) const {
    Matrix<T, N, M_> result;

    for (size_t c = 0; c < M_; c++) {
      for (size_t r = 0; r < N; r++) {
        T sum = T(0);

        for (size_t v = 0; v < Cols; v++)
          sum += m_cols[v][r] * matrix.m_cols[c][v];

        result.m_cols[c][r] = sum;
      }
    }

    return result;
  }

  /**
   * \brief Retrieves given column
   *
   * \param [in] idx Column index
   * \returns Reference to given column vector
   */
  const Vector<T, N>& operator [] (uint32_t idx) const {
    return m_cols[idx];
  }

  /**
   * \brief Retrieves given column for writing
   *
   * \param [in] idx Column index
   * \returns Reference to given column vector
   */
  Vector<T, N>& operator [] (uint32_t idx) {
    return m_cols[idx];
  }

  /**
   * \brief Retrieves column with compile-time index
   *
   * \tparam J Row index
   * \tparam I Column index
   * \returns Reference to given column vector
   */
  template<size_t I, std::enable_if_t<I < Cols, bool> = true>
  const VectorType& at() const {
    return m_cols[I];
  }

  /**
   * \brief Retrieves column with compile-time index for writing
   *
   * \tparam I Column index
   * \returns Reference to given column vector
   */
  template<size_t I, std::enable_if_t<I < Cols, bool> = true>
  VectorType& at() {
    return m_cols[I];
  }

  /**
   * \brief Retrieves element with compile-time index
   *
   * \tparam J Row index
   * \tparam I Column index
   * \returns Reference to given element
   */
  template<size_t J, size_t I, std::enable_if_t<J < Rows && I < Cols, bool> = true>
  const T& at() const {
    return m_cols[I].template at<J>();
  }

  /**
   * \brief Retrieves element with compile-time index for writing
   *
   * \tparam J Row index
   * \tparam I Column index
   * \returns Reference to given element
   */
  template<size_t J, size_t I, std::enable_if_t<J < Rows && I < Cols, bool> = true>
  T& at() {
    return m_cols[I].template at<J>();
  }

  bool operator == (const Matrix& matrix) const = default;
  bool operator != (const Matrix& matrix) const = default;

  /**
   * \brief Creates identity matrix
   * \returns Matrix with diagonal set to 1
   */
  static Matrix identity() {
    Matrix result;

    for (size_t c = 0; c < Cols; c++) {
      for (size_t r = 0; r < Rows; r++)
        result[c][r] = c == r ? T(1) : T(0);
    }

    return result;
  }

private:

  void init(std::integral_constant<size_t, M>) { }

  template<size_t I, typename... Tx>
  void init(std::integral_constant<size_t, I>, Vector<T, N> arg, const Tx&... args) {
    at<I>() = arg;
    init(std::integral_constant<size_t, I + 1>(), args...);
  }

  VectorType m_cols[M];

};


using Offset2D = Vector<int32_t, 2>;
using Offset3D = Vector<int32_t, 3>;

using Extent2D = Vector<uint32_t, 2>;
using Extent3D = Vector<uint32_t, 3>;

using Vector2D = Vector<float, 2>;
using Vector3D = Vector<float, 3>;
using Vector4D = Vector<float, 4>;

using Matrix2x2 = Matrix<float, 2, 2>;
using Matrix3x3 = Matrix<float, 3, 3>;
using Matrix4x4 = Matrix<float, 4, 4>;

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
