#pragma once

#include "util_vector.h"

namespace as {

/**
 * \brief Quaternion
 *
 * Implements common quaternion operations on top of
 * the existing 4D vector class.
 */
template<typename T>
class Quaternion {

public:

  using VectorType = Vector<T, 4>;

  Quaternion() = default;

  explicit Quaternion(VectorType vector)
  : m_vector(vector) { }

  explicit Quaternion(Vector<T, 3> im, T r)
  : m_vector(im, r) { }

  explicit Quaternion(T i, T j, T k, T r)
  : m_vector(i, j, k, r) { }

  Quaternion operator + (const Quaternion& other) const {
    return Quaternion(m_vector + other.m_vector);
  }

  Quaternion operator - (const Quaternion& other) const {
    return Quaternion(m_vector - other.m_vector);
  }

  Quaternion& operator += (const Quaternion& other) {
    m_vector += other.m_vector;
    return *this;
  }

  Quaternion& operator -= (const Quaternion& other) {
    m_vector += other.m_vector;
    return *this;
  }

  Quaternion operator - () const {
    return Quaternion(-m_vector);
  }

  /**
   * \brief Retrieves real part
   * \returns Real part
   */
  T getReal() const {
    return m_vector.template at<3>();
  }

  /**
   * \brief Retrieves imaginary part
   * \returns Imaginary part vector
   */
  Vector<T, 3> getIm() const {
    return m_vector.template get<0, 1, 2>();
  }

  /**
   * \brief Computes quaternion product
   *
   * \param [in] other Quaternion to multiply with
   * \returns Quaternion product
   */
  Quaternion operator * (const Quaternion& other) const {
    return Quaternion(prod(m_vector, other.m_vector));
  }

  Quaternion& operator *= (const Quaternion& other) {
    m_vector = prod(m_vector, other.m_vector);
    return *this;
  }

  /**
   * \brief Scales quaternion
   *
   * \param [in] scale Squared scaling factor
   * \returns Scaled quaternion
   */
  Quaternion operator * (T scale) const {
    return Quaternion(m_vector * scale);
  }

  Quaternion& operator *= (T scale) const {
    m_vector *= scale;
    return *this;
  }

  /**
   * \brief Applies quaternion rotation to vector
   *
   * Does not require normalization, and will scale the
   * vector by the squared length of the quaternion.
   * \param [in] vector Input vector
   * \returns Rotated vector
   */
  VectorType apply(const VectorType& vector) const {
    VectorType v = vector;
    v.template set<3>(T(0.0));

    return ((*this) * (Quaternion(v) * conjugate())).getVector();
  }

  /**
   * \brief Applies quaternion rotation to vector
   *
   * Assumes that the quaternion is normalized.
   * \param [in] vector Input vector
   * \returns Rotated vector
   */
  VectorType applyNorm(const VectorType& vector) const {
    VectorType a = cross(m_vector, vector) * 2.0f;
    VectorType b = cross(m_vector, a);
    return vector + fmadd(a, m_vector.template broadcast<3>(), b);
  }

  /**
   * \brief Computes conjugate
   * \returns Conjugate quaternion
   */
  Quaternion conjugate() const {
    return Quaternion(m_vector.template negate<0, 1, 2>());
  }

  /**
   * \brief Computes inverse
   * \returns Inverse quaternion
   */
  Quaternion inverse() const {
    T factor = approx_rcp(dot(m_vector, m_vector));
    return Quaternion(conjugate().m_vector * factor);
  }

  /**
   * \brief Normalizes quaternion
   * \returns Normalized quaternion
   */
  Quaternion normalize() const {
    return Quaternion(normalize(m_vector));
  }

  /**
   * \brief Computes norm of quaternion
   * \returns Quaternion norm
   */
  T norm() const {
    return length(m_vector);
  }

  /**
   * \brief Computes scaling factor of quaternion
   *
   * Equivalent to the squared norm of the quaternion.
   * \returns Scaling factor
   */
  T scaling() const {
    return dot(m_vector, m_vector);
  }

  /**
   * \brief Queries raw vector
   *
   * Type casts do not work with this for some reason so
   * we have to proide a method to extract the vector.
   * \returns Raw 4D vector representation. Note that
   *    the scalar part is stored in the last component.
   */
  VectorType getVector() const {
    return m_vector;
  }

  /**
   * \brief Computes identity quaternion
   * \returns Identity quaternion
   */
  static Quaternion identity() {
    return Quaternion(T(0.0), T(0.0), T(0.0), T(1.0));
  }

private:

  VectorType m_vector;

  static VectorType prod(const VectorType& a, const VectorType& b) {
    VectorType k =
      a.template get<1, 2, 2, 2>() *
      b.template get<2, 0, 3, 2>();

    VectorType j = fmadd(
      a.template get<0, 1, 0, 1>(),
      b.template get<3, 3, 1, 1>(), k
    ).template negate<3>();

    VectorType i = fmsub(
      a.template get<2, 0, 1, 0>(),
      b.template get<1, 2, 0, 0>(), j);

    return fmsub(a.template broadcast<3>(), b, i);
  }

};


/**
 * \brief Quaternion transform
 *
 * Combines a quaternion with a translation vector,
 * and implements basic transforms on top of it.
 */
template<typename T>
class QuaternionTransform {

public:

  using VectorType = typename Quaternion<T>::VectorType;

  QuaternionTransform() = default;

  QuaternionTransform(
    const Quaternion<T>&                q,
    const VectorType&                   v)
  : m_quat(q), m_pos(v) { }

  QuaternionTransform(
    const VectorType&                   q,
    const VectorType&                   v)
  : m_quat(q), m_pos(v) { }

  /**
   * \brief Queries quaternion
   * \returns Rotation quaternion
   */
  Quaternion<T> getRotation() const {
    return m_quat;
  }

  /**
   * \brief Queries translation vector
   * \returns Translation vector
   */
  VectorType getTranslation() const {
    return m_pos;
  }

  /**
   * \brief Computes inverse transform
   *
   * Assumes that the quaternion is normalized.
   * \returns Inverse transform
   */
  QuaternionTransform inverse() const {
    Quaternion inverseQuat = m_quat.conjugate();

    return QuaternionTransform(
      inverseQuat,
      inverseQuat.apply(-m_pos));
  }

  /**
   * \brief Applies transform to another transform
   *
   * Results in a transform that is equivalent to first
   * applying \c other, then this object's transform.
   * \param [in] other Transform to apply this transform to
   * \returns Resulting quaternion transform
   */
  QuaternionTransform chain(const QuaternionTransform& other) const {
    return QuaternionTransform(
      m_quat * other.m_quat,
      m_quat.apply(other.m_pos) + m_pos);
  }

  /**
   * \brief Applies normalized transform to another transform
   *
   * Requires this transform to have a normalized rotation.
   * The second transform and the result may not be normalized.
   * \param [in] other Transform to apply this transform to
   * \returns Resulting quaternion transform
   */
  QuaternionTransform chainNorm(const QuaternionTransform& other) const {
    return QuaternionTransform(
      m_quat * other.m_quat,
      m_quat.applyNorm(other.m_pos) + m_pos);
  }

  /**
   * \brief Applies transform to a vector
   *
   * First applies the rotation, then the translation.
   * \param [in] vector Input vector
   * \returns Transformed vector
   */
  VectorType apply(const VectorType& vector) const {
    return m_quat.apply(vector) + m_pos;
  }

  /**
   * \brief Applies normalized transform to a vector
   *
   * Requires the rotation quaternion to be normalized.
   * \param [in] vector Input vector
   * \returns Transformed vector
   */
  VectorType applyNorm(const VectorType& vector) const {
    return m_quat.applyNorm(vector) + m_pos;
  }

  /**
   * \brief Applies rotation to a vector
   *
   * Omits the translation. Equivalent to only
   * applying the rotation quaternion.
   * \param [in] vector Input vector
   * \returns Rotated vector
   */
  VectorType applyRotation(const VectorType& vector) const {
    return m_quat.apply(vector);
  }

  /**
   * \brief Applies rotation to a vector
   *
   * Requires the rotation quaternion to be normalized.
   * \param [in] vector Input vector
   * \returns Rotated vector
   */
  VectorType applyRotationNorm(const VectorType& vector) const {
    return m_quat.applyNorm(vector);
  }

  /**
   * \brief Computes identity transform
   * \returns Identity transform
   */
  static QuaternionTransform identity() {
    return QuaternionTransform(
      Vector<T, 4>(T(0.0), T(0.0), T(0.0), T(1.0)),
      Vector<T, 4>(T(0.0), T(0.0), T(0.0), T(0.0)));
  }

private:

  Quaternion<T> m_quat;
  VectorType    m_pos;

};


// Default aliases for float types
using Quat          = Quaternion<float>;
using QuatTransform = QuaternionTransform<float>;

static_assert(sizeof(Quat)          == 16);
static_assert(sizeof(QuatTransform) == 32);


/**
 * \brief Computes roration quaternion
 *
 * \param [in] axis Rotation axis
 * \param [in] angle Rotation angle
 * \returns Quaternion representing the given rotation
 */
inline Quat computeRotationQuaternion(Vector3D axis, float angle) {
  SinCos sincos = approx_sincos(angle * 0.5f);
  // Normalize resulting quaternion since sincos approximation
  // errors may otherwise accumulate into a scaling quaternion.
  return Quat(normalize(Vector4D(axis * sincos.sin, sincos.cos)));
}


/**
 * \brief Computes roration quaternion
 *
 * Convenience overload that takes a 4D vector as the axis.
 * The last component of the axis vector will be ignored.
 * \param [in] axis Rotation axis
 * \param [in] angle Rotation angle
 * \returns Quaternion representing the given rotation
 */
inline Quat computeRotationQuaternion(Vector4D axis, float angle) {
  SinCos sincos = approx_sincos(angle * 0.5f);

  // Same logic as the 3D version, just fill the
  // vector in a more SSE-friendly way
  Vector4D vector = axis * sincos.sin;
  vector.set<3>(sincos.cos);

  return Quat(normalize(vector));
}


/**
 * \brief Computes quaternion transform from matrix
 *
 * The matrix must be decomposable into rotation and
 * translation, with no scaling or mirroring applied.
 * \param [in] matrix Matrix
 * \return Quaternion transform
 */
inline QuatTransform computeTransformFromMatrix(const Matrix4x4& matrix) {
  Vector4D xaxis = matrix.col<0>();
  Vector4D yaxis = matrix.col<1>();
  Vector4D zaxis = matrix.col<2>();
  Vector4D translation = matrix.col<3>();

  // Pieced together from various random StackOverflow posts
  // but this generally seems to work for some weird reason
  float trace = xaxis.at<0>() + yaxis.at<1>() + zaxis.at<2>();

  Vector4D q;

  if (trace > 0.0f) {
    float r = trace + 1.0f;
    float s = 0.5f * approx_rsqrt(r);

    q = Vector4D(
      (yaxis.at<2>() - zaxis.at<1>()),
      (zaxis.at<0>() - xaxis.at<2>()),
      (xaxis.at<1>() - yaxis.at<0>()),
      r) * s;
  } else if (xaxis.at<0>() > max(yaxis.at<1>(), zaxis.at<2>())) {
    float r = 1.0f + xaxis.at<0>() - (yaxis.at<1>() + zaxis.at<2>());
    float s = 0.5f * approx_rsqrt(r);

    q = Vector4D(r,
      (xaxis.at<1>() + yaxis.at<0>()),
      (xaxis.at<2>() + zaxis.at<0>()),
      (yaxis.at<2>() - zaxis.at<1>())) * s;
  } else if (yaxis.at<1>() > zaxis.at<2>()) {
    float r = 1.0f + yaxis.at<1>() - (xaxis.at<0>() + zaxis.at<2>());
    float s = 0.5f * approx_rsqrt(r);

    q = Vector4D(
      (yaxis.at<0>() + xaxis.at<1>()),
      r,
      (zaxis.at<1>() + yaxis.at<2>()),
      (zaxis.at<0>() - xaxis.at<2>())) * s;
  } else {
    float r = 1.0f + zaxis.at<2>() - (xaxis.at<0>() + yaxis.at<1>());
    float s = 0.5f * approx_rsqrt(r);

    q = Vector4D(
      (xaxis.at<2>() + zaxis.at<0>()),
      (yaxis.at<2>() + zaxis.at<1>()),
      r,
      (xaxis.at<1>() - yaxis.at<0>())) * s;
  }

  return QuatTransform(q, translation);
}


/**
 * \brief Computes view transform
 *
 * \param [in] eye Camera position
 * \param [in] dir Directional vector
 * \param [in] up Vector pointing upwards
 */
inline QuatTransform computeViewTransform(Vector4D eye, Vector4D dir, Vector4D up) {
  Vector4D zaxis = normalize(dir);
  Vector4D xaxis = normalize(cross(up, zaxis));
  Vector4D yaxis = cross(zaxis, xaxis);

  Matrix4x4 matrix(xaxis, yaxis, zaxis, eye);
  return computeTransformFromMatrix(matrix).inverse();
}


inline QuatTransform computeViewTransform(Vector3D eye, Vector3D dir, Vector3D up) {
  return computeViewTransform(Vector4D(eye, 0.0f), Vector4D(dir, 0.0f), Vector4D(up, 0.0f));
}

}
