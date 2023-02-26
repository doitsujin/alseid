#pragma once

#include "util_vector.h"

namespace as {

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
    return multiplyMatrix(std::make_index_sequence<Cols_>(), std::make_index_sequence<Cols>(), matrix);
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

  template<size_t Col, size_t Cols_>
  VectorType multiplyMatrixFmaChain(
      std::integer_sequence<size_t>,
      const Matrix<T, Cols, Cols_>&     matrix,
            VectorType                  accum) const {
    return accum;
  }

  template<size_t Col, size_t Cols_, size_t I, size_t... Idx>
  VectorType multiplyMatrixFmaChain(
      std::integer_sequence<size_t, I, Idx...>,
      const Matrix<T, Cols, Cols_>&     matrix,
            VectorType                  accum) const {
    accum = multiplyMatrixFmaChain<Col>(std::integer_sequence<size_t, Idx...>(), matrix, accum);
    return fmadd(col<I>(), VectorType(matrix.template at<I, Col>()), accum);
  }

  template<size_t Col, size_t Cols_, size_t... Idx>
  VectorType multiplyMatrixColumn(
      std::integer_sequence<size_t, 0, Idx...> inner,
      const Matrix<T, Cols, Cols_>&     matrix) const {
    VectorType accum = col<0>() * VectorType(matrix.template at<0, Col>());
    return multiplyMatrixFmaChain<Col>(std::integer_sequence<size_t, Idx...>(), matrix, accum);
  }

  template<size_t Cols_, size_t... OuterIdx, size_t... InnerIdx>
  Matrix<T, Rows, Cols_> multiplyMatrix(
      std::integer_sequence<size_t, OuterIdx...>,
      std::integer_sequence<size_t, InnerIdx...> inner,
      const Matrix<T, Cols, Cols_>&     matrix) const {
    return Matrix<T, Rows, Cols_>(multiplyMatrixColumn<OuterIdx>(inner, matrix)...);
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


using Matrix2x2 = Matrix<float, 2, 2>;
using Matrix3x3 = Matrix<float, 3, 3>;
using Matrix4x4 = Matrix<float, 4, 4>;

using Matrix4x3 = Matrix<float, 4, 3>;
using Matrix3x4 = Matrix<float, 3, 4>;

}
