#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

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
   * \brief Sets element
   *
   * \param [in] idx Element index
   * \param [in] value New value
   */
  void set(uint32_t idx, T value) {
    m_data[idx] = value;
  }

  bool operator == (const Vector& vector) const = default;
  bool operator != (const Vector& vector) const = default;

private:

  template<size_t... Indices, typename... Tx>
  explicit Vector(std::integer_sequence<size_t, Indices...>, const std::tuple<Tx...>& args)
  : m_data { T(std::get<Indices>(args))... } { }

  T m_data[N];

};


/**
 * \brief Matrix
 *
 * Simple matrix type. Much like the \c Vector class, this is
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
  using VectorType = Vector<T, Rows>;

  Matrix() = default;

  /**
   * \brief Initializes matrix
   *
   * Initializes matrix with the given vector arguments.
   * \param [in] args Arguments
   */
  template<typename... Tx, std::enable_if_t<sizeof...(Tx) == Cols, bool> = true>
  explicit Matrix(const Tx&... args)
  : m_cols { args... } { }

  /**
   * \brief Initializes matrix from another one
   *
   * The other matrix type must use a compatible scalar
   * type and be larger or the same size as the matrix
   * being initialized.
   * \param [in] matrix The other matrix
   */
  template<typename T_, size_t Rows_, size_t Cols_, std::enable_if_t<
    (std::is_convertible_v<T_, T> && Rows_ >= Rows && Cols_ >= Cols), bool> = true>
  explicit Matrix(const Matrix<T_, Rows_, Cols_>& matrix)
  : Matrix(std::make_index_sequence<Cols>(), matrix) { }

  /**
   * \brief Matrix scaling
   *
   * \param [in] factor Source operand
   * \returns Reference to self
   */
  Matrix& operator *= (T factor) {
    for (size_t c = 0; c < Cols; c++)
      m_cols[c] *= factor;

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

    for (size_t c = 0; c < Cols; c++)
      result.m_cols[c] = m_cols[c] * factor;

    return result;
  }

  /**
   * \brief Matrix product
   *
   * \param [in] matrix Source operand
   * \returns Resulting matrix
   */
  template<size_t Cols_>
  Matrix<T, Rows, Cols_> operator * (const Matrix<T, Cols, Cols_>& matrix) const {
    Matrix<T, Rows, Cols_> result;

    for (size_t c = 0; c < Cols_; c++) {
      VectorType sum = m_cols[0] * matrix.m_cols[c][0];

      for (size_t v = 1; v < Cols; v++)
        sum += m_cols[v] * matrix.m_cols[c][v];

      result.m_cols[c] = sum;
    }

    return result;
  }

  /**
   * \brief Retrieves given column
   *
   * \param [in] idx Column index
   * \returns Reference to given column vector
   */
  const VectorType& col(uint32_t idx) const {
    return m_cols[idx];
  }

  /**
   * \brief Retrieves given column for writing
   *
   * \param [in] idx Column index
   * \returns Reference to given column vector
   */
  VectorType& col(uint32_t idx) {
    return m_cols[idx];
  }

  /**
   * \brief Retrieves row vector
   *
   * \param [in] idx Row index
   * \returns Row vector
   */
  Vector<T, Cols>& row(uint32_t idx) {
    return getRow(std::make_index_sequence<Cols>(), idx);
  }

  /**
   * \brief Retrieves column with compile-time index
   *
   * \tparam J Row index
   * \tparam I Column index
   * \returns Reference to given column vector
   */
  template<size_t Col, std::enable_if_t<(Col < Cols), bool> = true>
  const VectorType& col() const {
    return m_cols[Col];
  }

  /**
   * \brief Retrieves column with compile-time index for writing
   *
   * \tparam Col Column index
   * \returns Reference to given column vector
   */
  template<size_t Col, std::enable_if_t<(Col < Cols), bool> = true>
  VectorType& col() {
    return m_cols[Col];
  }

  /**
   * \brief Retrieves row vector with compile-time index
   *
   * \tparam Row Row index
   * \returns Row vector
   */
  template<size_t Row, std::enable_if_t<(Row < Rows), bool> = true>
  Vector<T, Cols> row() const {
    return getRow<Row>(std::make_index_sequence<Cols>());
  }

  /**
   * \brief Retrieves element
   *
   * \param [in] index Pair of row and column indices, row first
   * \returns Reference to the given element
   */
  T operator [] (std::pair<uint32_t, uint32_t> index) const {
    return m_cols[index.first][index.second];
  }

  /**
   * \brief Retrieves element with compile-time index
   *
   * \tparam Row Row index
   * \tparam Col Column index
   * \returns Requested element
   */
  template<size_t Row, size_t Col, std::enable_if_t<(Row < Rows && Col < Cols), bool> = true>
  T at() const {
    return m_cols[Col].template at<Row>();
  }

  bool operator == (const Matrix& matrix) const = default;
  bool operator != (const Matrix& matrix) const = default;

  /**
   * \brief Transposes the matrix
   *
   * Flips rows and columns.
   * \returns Transposed matrix
   */
  Matrix<T, Cols, Rows> transpose() const {
    return transposeMatrix(std::make_index_sequence<Cols>(), std::make_index_sequence<Rows>());
  }

  /**
   * \brief Creates identity matrix
   * \returns Matrix with diagonal set to 1
   */
  static Matrix identity() {
    return makeIdentityMatrix(std::make_index_sequence<Cols>(), std::make_index_sequence<Rows>());
  }

private:

  template<typename T_, size_t Rows_, size_t Cols_, size_t... Indices>
  explicit Matrix(std::integer_sequence<size_t, Indices...>, const Matrix<T_, Rows_, Cols_>& matrix)
  : Matrix(VectorType(matrix.template col<Indices>())...) { }

  template<size_t Row, size_t... ColIdx>
  Vector<T, Cols> getRow(std::integer_sequence<size_t, ColIdx...>) const {
    return Vector<T, Cols>(at<Row, ColIdx>()...);
  }

  template<size_t... ColIdx>
  Vector<T, Cols> getRow(std::integer_sequence<size_t, ColIdx...>, uint32_t idx) const {
    return Vector<T, Cols>((col<ColIdx>()[idx])...);
  }

  template<size_t... ColIdx, size_t... RowIdx>
  Matrix<T, Cols, Rows> transposeMatrix(std::integer_sequence<size_t, ColIdx...> cols, std::integer_sequence<size_t, RowIdx...> rows) const {
    return Matrix<T, Cols, Rows>(getRow<RowIdx>(cols)...);
  }

  template<size_t Col, size_t... RowIdx>
  static VectorType makeIdentityColumn(std::integer_sequence<size_t, RowIdx...>) {
    return VectorType((Col == RowIdx ? T(1) : T(0))...);
  }

  template<size_t... ColIdx, size_t... RowIdx>
  static Matrix makeIdentityMatrix(std::integer_sequence<size_t, ColIdx...>, std::integer_sequence<size_t, RowIdx...> rows) {
    return Matrix(makeIdentityColumn<ColIdx>(rows)...);
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

using Matrix4x3 = Matrix<float, 4, 3>;
using Matrix3x4 = Matrix<float, 3, 4>;

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
