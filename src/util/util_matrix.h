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
    return m_cols[index.second][index.first];
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
    VectorType accum = col<0>() * matrix.template col<Col>().template broadcast<0>();
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
 * \brief Compact representation of a projection
 *
 * Stores relevant data for an orthographic or perspective
 * projection. In case of perspective projection, the Z
 * factor for the W component is implied to be -1.
 */
struct Projection {
  /** X scaling factor. For perspective projection, this is
   *  the vertical FOV divided by the aspect ratio. Can have
   *  arbitrary values for orthographic projections. */
  float xScale;
  /** Y scaling factor. For perspective projection, this is
   *  the vertical FOV unmodified. Can have arbitrary values
   *  for orthographic projections. */
  float yScale;
  /** Z scaling factor. If 0, the projection is a perspective
   *  projection and the last row of the marix will be filled
   *  out accordingly. */
  float zScale;
  /** Z bias. Equal to \c zNear for perspective projections. */
  float zBias;
};

static_assert(sizeof(Projection) == 16);


/**
 * \brief View frustum
 *
 * Stores frustum planes in view space and provides convenience
 * methods to perform culling. Note that this does not include
 * a far plane since zFar is infinite for perspective projection,
 * and orthographic projections should not need z culling.
 */
struct ViewFrustum {
  Vector4D xNeg;
  Vector4D xPos;
  Vector4D yNeg;
  Vector4D yPos;
  Vector4D zNear;
  Vector4D zFar;
};


/**
 * \brief Computes orthographic projection
 *
 * Z is linear here, but for consistency with perspective projection,
 * this will still use inverse Z so that depth tests do not have to
 * be adjusted depending on the projection used.
 * \param [in] viewport Viewport size
 * \param [in] zNear Near plane
 * \param [in] zFar Far plane
 */
inline Projection computeOrthographicProjection(Vector2D viewport, float zNear, float zFar) {
  Projection result;
  result.xScale = 2.0f * approx_rcp(viewport.at<0>());
  result.yScale = 2.0f * approx_rcp(viewport.at<1>());
  result.zScale = approx_rcp(zFar - zNear);
  result.zBias = zFar * result.zScale;
  return result;
}


/**
 * \brief Computes perspective projection
 *
 * The resulting matrix will use inverse Z and have an infinite far
 * plane. The w component of any projected vertex will be equal to
 * the negative z component of the input vector.
 * \param [in] viewport Viewport size
 * \param [in] f Vertical field of view
 * \param [in] zNear Near plane
 */
inline Projection computePerspectiveProjection(Vector2D viewport, float f, float zNear) {
  float aspect = approx_div(viewport.at<1>(), viewport.at<0>());

  Projection result;
  result.xScale = f * aspect;
  result.yScale = f;
  result.zScale = 0.0f;
  result.zBias = zNear;
  return result;
}


/**
 * \brief Computes view frustum for projection
 *
 * Note that the zFar plane is only meaningful for orthographic
 * projections.
 */
inline ViewFrustum computeViewFrustum(
  const Projection&                     projection) {
  ViewFrustum result;

  bool isPerspective = projection.zScale == 0.0f;

  float wz = isPerspective ? -1.0f : 0.0f;
  float ww = isPerspective ?  0.0f : 1.0f;

  float zNear = projection.zBias;
  float zFar = 0.0f;

  if (!isPerspective) {
    // Need to reconstruct the z components manually
    float invScale = approx_rcp(projection.zScale);

    zFar = projection.zBias * invScale;
    zNear = zFar - invScale;
  }

  result.xNeg = normalizePlane(Vector4D(-projection.xScale, 0.0f, wz, ww));
  result.xPos = normalizePlane(Vector4D( projection.xScale, 0.0f, wz, ww));
  result.yNeg = normalizePlane(Vector4D(0.0f, -projection.yScale, wz, ww));
  result.yPos = normalizePlane(Vector4D(0.0f,  projection.yScale, wz, ww));
  result.zNear = Vector4D(0.0f, 0.0f, -1.0f, -zNear);

  // zFar is infinitely far away for perspective projections,
  // just set everything to 0 so that culling always passes.
  result.zFar  = isPerspective
    ? Vector4D(0.0f)
    : Vector4D(0.0f, 0.0f, 1.0f, zFar);

  return result;
}


/**
 * \brief Computes projection from compact projection
 *
 * \param [in] p Compact projection
 * \returns Equivalent projection matrix
 */
inline Matrix4x4 computeProjectionMatrix(const Projection& p) {
  return Matrix4x4(
    Vector4D(p.xScale,   0.0f,     0.0f,     0.0f),
    Vector4D(  0.0f,   p.yScale,   0.0f,     0.0f),
    Vector4D(  0.0f,     0.0f,   p.zScale, p.zScale == 0.0f ? -1.0f : 0.0f),
    Vector4D(  0.0f,     0.0f,   p.zBias,  p.zScale == 0.0f ?  0.0f : 1.0f));
}


/**
 * \brief Computes a transformation matrix
 *
 * Equivalent to multiplying a translation matrix on the
 * left side with a rotation matrix on the right side.
 * \param [in] u Normalized axis to rotate around.
 * \param [in] th Rotation angle
 * \param [in] v Translation vector.
 * \returns Resulting matrix
 */
inline Matrix4x4 computeTransformMatrix(Vector4D u, float th, Vector4D v) {
  SinCos sincos = approx_sincos(th);

  u.set<3>(0.0f);
  v.set<3>(1.0f);

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
  return computeTransformMatrix(Vector4D(u, 0.0f), th, Vector4D(v, 0.0f));
}


/**
 * \brief Computes camera matrix
 *
 * \param [in] eye Camera position
 * \param [in] dir Directional vector
 * \param [in] up Vector pointing upwards
 * \returns Resulting matrix
 */
inline Matrix4x4 computeViewMatrix(Vector4D eye, Vector4D dir, Vector4D up) {
  eye.set<3>(-1.0f);
  dir.set<3>(0.0f);

  Vector4D zaxis = normalize(dir);
  Vector4D xaxis = normalize(cross(up, zaxis));
  Vector4D yaxis = cross(zaxis, xaxis);
  Vector4D wpart = Vector4D(0.0f, 0.0f, 0.0f, 1.0f);

  Matrix4x4 matrix = transpose(Matrix4x4(xaxis, yaxis, zaxis, wpart));
  matrix.set<3>(matrix * -eye);

  return matrix;
}

inline Matrix4x4 computeViewMatrix(Vector3D eye, Vector3D dir, Vector3D up) {
  return computeViewMatrix(Vector4D(eye, 0.0f), Vector4D(dir, 0.0f), Vector4D(up, 0.0f));
}


/**
 * \brief Computes a rotation matrix
 *
 * \param [in] u Normalized axis to rotate around. Last component must be 0.
 * \param [in] th Rotation angle
 * \returns Resulting rotation matrix
 */
inline Matrix4x4 computeRotationMatrix(Vector4D u, float th) {
  return computeTransformMatrix(u, th, Vector4D(0.0f));
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
  v.set<3>(1.0f);

  return Matrix4x4(
    Vector4D(1.0f, 0.0f, 0.0f, 0.0f),
    Vector4D(0.0f, 1.0f, 0.0f, 0.0f),
    Vector4D(0.0f, 0.0f, 1.0f, 0.0f),
    v);
}

inline Matrix4x4 computeTranslationMatrix(Vector3D v) {
  return computeTranslationMatrix(Vector4D(v, 0.0f));
}


/**
 * \brief Packs transformation matrix
 *
 * Transposes the matrix and gets rid of the last row, which
 * is implied to be the unit vector for that row. This format
 * is useful when storing matrices in GPU memory.
 * \param [in] matrix Input matrix
 */
inline Matrix4x3 packTransformMatrix(const Matrix4x4& matrix) {
  return Matrix4x3(transpose(matrix));
}


}
