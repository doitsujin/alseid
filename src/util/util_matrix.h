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
   * \brief Matrix-vector product
   *
   * \param [in] vector Vector
   * \returns Resulting vector
   */
  VectorType operator * (const Vector<T, Cols>& vector) const {
    return multiplyVector(std::make_index_sequence<Cols>(), vector);
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
  VectorType col(uint32_t idx) const {
    return m_cols[idx];
  }

  /**
   * \brief Retrieves row vector
   *
   * \param [in] idx Row index
   * \returns Row vector
   */
  Vector<T, Cols> row(uint32_t idx) {
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
  VectorType col() const {
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
   * \brief Sets column vector
   *
   * \tparam Col Colmn index
   * \param [in] v Column vector
   */
  template<size_t Col>
  void set(VectorType v) {
    m_cols[Col] = v;
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

  template<size_t Col, size_t Cols_>
  VectorType multiplyMatrixFmaChain(
          std::integer_sequence<size_t>,
    const Matrix<T, Cols, Cols_>&       matrix,
          VectorType                    accum) const {
    return accum;
  }

  template<size_t Col, size_t Cols_, size_t I, size_t... Idx>
  VectorType multiplyMatrixFmaChain(
          std::integer_sequence<size_t, I, Idx...>,
    const Matrix<T, Cols, Cols_>&       matrix,
          VectorType                    accum) const {
    accum = multiplyMatrixFmaChain<Col>(std::integer_sequence<size_t, Idx...>(), matrix, accum);
    return fmadd(col<I>(), matrix.template col<Col>().template broadcast<I>(), accum);
  }

  template<size_t Col, size_t Cols_, size_t... Idx>
  VectorType multiplyMatrixColumn(
          std::integer_sequence<size_t, 0, Idx...> inner,
    const Matrix<T, Cols, Cols_>&       matrix) const {
    VectorType accum = col<0>() * matrix.template Col<Col>().template broadcast<0>();
    return multiplyMatrixFmaChain<Col>(std::integer_sequence<size_t, Idx...>(), matrix, accum);
  }

  template<size_t Cols_, size_t... OuterIdx, size_t... InnerIdx>
  Matrix<T, Rows, Cols_> multiplyMatrix(
      std::integer_sequence<size_t, OuterIdx...>,
      std::integer_sequence<size_t, InnerIdx...> inner,
      const Matrix<T, Cols, Cols_>&     matrix) const {
    return Matrix<T, Rows, Cols_>(multiplyMatrixColumn<OuterIdx>(inner, matrix)...);
  }

  VectorType multiplyVectorFmaChain(
          std::integer_sequence<size_t>,
    const Vector<T, Cols>&              vector,
          VectorType                    accum) const {
    return accum;
  }

  template<size_t I, size_t... Idx>
  VectorType multiplyVectorFmaChain(
          std::integer_sequence<size_t, I, Idx...>,
    const Vector<T, Cols>&              vector,
          VectorType                    accum) const {
    accum = multiplyVectorFmaChain(std::integer_sequence<size_t, Idx...>(), vector, accum);
    return fmadd(col<I>(), vector.template broadcast<I>(), accum);
  }

  template<size_t... Idx>
  VectorType multiplyVector(
          std::integer_sequence<size_t, 0, Idx...>,
    const Vector<T, Cols>&              vector) const {
    VectorType accum = col<0>() * vector.template at<0>();
    return multiplyVectorFmaChain(std::integer_sequence<size_t, Idx...>(), vector, accum);
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


template<typename T, size_t Rows, size_t Cols, size_t... ColIdx, size_t... RowIdx>
Matrix<T, Cols, Rows> transpose(
        std::integer_sequence<size_t, RowIdx...>,
  const Matrix<T, Rows, Cols>&          m) {
  return Matrix<T, Cols, Rows>(m.template row<RowIdx>()...);
}

/**
 * \brief Transposes a matrix
 *
 * Flips rows and columns.
 * \param [in] m Matrix to transpose
 * \returns Transposed matrix
 */
template<typename T, size_t Rows, size_t Cols>
Matrix<T, Cols, Rows> transpose(const Matrix<T, Rows, Cols>& m) {
  return transpose(std::make_index_sequence<Rows>(), m);
}


#ifdef AS_HAS_X86_INTRINSICS
inline Matrix<float, 4, 4> transpose(const Matrix<float, 4, 4>& m) {
  using V = Vector<float, 4>;

  __m128 r0 = __m128(m.col<0>());
  __m128 r1 = __m128(m.col<1>());
  __m128 r2 = __m128(m.col<2>());
  __m128 r3 = __m128(m.col<3>());

  __m128 t0 = _mm_unpacklo_ps(r0, r1);
  __m128 t1 = _mm_unpackhi_ps(r0, r1);
  __m128 t2 = _mm_unpacklo_ps(r2, r3);
  __m128 t3 = _mm_unpackhi_ps(r2, r3);

  __m128 c0 = _mm_movelh_ps(t0, t2);
  __m128 c1 = _mm_movehl_ps(t2, t0);
  __m128 c2 = _mm_movelh_ps(t1, t3);
  __m128 c3 = _mm_movehl_ps(t3, t1);

  return Matrix<float, 4, 4>(V(c0), V(c1), V(c2), V(c3));
}
#endif


using Matrix2x2 = Matrix<float, 2, 2>;
using Matrix3x3 = Matrix<float, 3, 3>;
using Matrix4x4 = Matrix<float, 4, 4>;

using Matrix4x3 = Matrix<float, 4, 3>;
using Matrix3x4 = Matrix<float, 3, 4>;


/**
 * \brief Computes a projection matrix
 *
 * The resulting matrix will use reverse Z
 * and have an infinite far plane.
 * \param [in] viewport Viewport size
 * \param [in] f Vertical field of view
 * \param [in] zNear Near plane
 */
inline Matrix4x4 computeProjectionMatrix(Vector2D viewport, float f, float zNear) {
  float a = approx_div(viewport.at<1>(), viewport.at<0>());

  return Matrix4x4(
    Vector4D(f * a, 0.0f,  0.0f,  0.0f),
    Vector4D( 0.0f,    f,  0.0f,  0.0f),
    Vector4D( 0.0f, 0.0f,  0.0f, -1.0f),
    Vector4D( 0.0f, 0.0f, zNear,  0.0f));
}


/**
 * \brief Computes a transformation matrix
 *
 * Equivalent to multiplying a translation matrix on the
 * left side with a rotation matrix on the right side.
 * \param [in] u Normalized axis to rotate around. Last component must be 0.
 * \param [in] th Rotation angle
 * \param [in] v Translation vector. Last component must be 1.
 * \returns Resulting matrix
 */
inline Matrix4x4 computeTransformMatrix(Vector4D u, float th, Vector4D v) {
  SinCos sincos = approx_sincos(th);

  Vector4D usin = u * sincos.sin;
  Vector4D ucos = u * (1.0f - sincos.cos);

  Vector4D c1(   sincos.cos,  usin.at<2>(), -usin.at<1>(), 0.0f);
  Vector4D c2(-usin.at<2>(),    sincos.cos,  usin.at<0>(), 0.0f);
  Vector4D c3( usin.at<1>(), -usin.at<0>(),    sincos.cos, 0.0f);

  c1 = fmadd(u, Vector4D(ucos.at<0>()), c1);
  c2 = fmadd(u, Vector4D(ucos.at<1>()), c2);
  c3 = fmadd(u, Vector4D(ucos.at<2>()), c3);

  return Matrix4x4(c1, c2, c3, v);
}

inline Matrix4x4 computeTransformMatrix(Vector3D u, float th, Vector3D v) {
  return computeTransformMatrix(Vector4D(u, 0.0f), th, Vector4D(v, 1.0f));
}


/**
 * \brief Computes camera matrix
 *
 * Much like \c computeTransformMatrix, but as if the
 * multiplication is performed in reverse order.
 * \param [in] u Normalized axis to rotate around. Last component must be 0.
 * \param [in] th Rotation angle
 * \param [in] v Translation vector. Last component must be 1.
 * \returns Resulting matrix
 */
inline Matrix4x4 computeViewMatrix(Vector4D u, float th, Vector4D v) {
  Matrix4x4 matrix = computeTransformMatrix(u, th, Vector4D(0.0f, 0.0f, 0.0f, 1.0f));
  matrix.set<3>(matrix * v);
  return matrix;
}

inline Matrix4x4 computeViewMatrix(Vector3D u, float th, Vector3D v) {
  return computeViewMatrix(Vector4D(u, 0.0f), th, Vector4D(v, 1.0f));
}


/**
 * \brief Computes a rotation matrix
 *
 * \param [in] u Normalized axis to rotate around. Last component must be 0.
 * \param [in] th Rotation angle
 * \returns Resulting rotation matrix
 */
inline Matrix4x4 computeRotationMatrix(Vector4D u, float th) {
  return computeTransformMatrix(u, th, Vector4D(0.0f, 0.0f, 0.0f, 1.0f));
}

inline Matrix4x4 computeRotationMatrix(Vector3D u, float th) {
  return computeRotationMatrix(Vector4D(u, 0.0f), th);
}


/**
 * \brief Computes a translation matrix
 *
 * \param [in] v Translation vector. Last component must be 1.
 * \returns Resulting translation matrix
 */
inline Matrix4x4 computeTranslationMatrix(Vector4D v) {
  return Matrix4x4(
    Vector4D(1.0f, 0.0f, 0.0f, 0.0f),
    Vector4D(0.0f, 1.0f, 0.0f, 0.0f),
    Vector4D(0.0f, 0.0f, 1.0f, 0.0f),
    v);
}

inline Matrix4x4 computeTranslationMatrix(Vector3D v) {
  return computeTranslationMatrix(Vector4D(v, 1.0f));
}


}
