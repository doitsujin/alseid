// Multiplies two quaternions
vec4 quatMul(vec4 a, vec4 b) {
  vec4 result;
  result.xyz = a.w * b.xyz + a.xyz * b.w + cross(a.xyz, b.xyz);
  result.w   = a.w * b.w - dot(a.xyz, b.xyz);
  return result;
}


// Computes conjugate of quaternion
vec4 quatConjugate(vec4 q) {
  return vec4(-q.xyz, q.w);
}


// Computes inverse of quaternion
vec4 quatInverse(vec4 q) {
  return quatConjugate(q) / dot(q, q);
}


// Computes scaling factor of quaternion
float quatGetScale(vec4 q) {
  return dot(q, q);
}



// Quaterion slerp. Note that this is slow and
// should only be used if absolutely necessary.
vec4 quatSlerp(vec4 v0, vec4 v1, float t) {
  float d = clamp(dot(v0, v1), -1.0f, 1.0f);

  if (d > 0.9995f)
    return normalize(mix(v0, v1, t));

  float theta = acos(d) * t;

  vec4 v2 = normalize(v1 - v0 * d);
  return normalize(v0 * cos(theta) + v2 * sin(theta));
}


// Applies generic quaternion to a vector
vec3 quatApply(vec4 q, vec3 v) {
  return quatMul(q, quatMul(vec4(v, 0.0f), quatConjugate(q))).xyz;
}


// Applies normalized quaternion to a vector. Considerably
// faster, but requires the quaternion to be normalized.
vec3 quatApplyNorm(vec4 q, vec3 v) {
  vec3 a = cross(q.xyz, v) * 2.0f;
  vec3 b = cross(q.xyz, a);
  return v + a * q.w + b;
}


// Transform consisting of a rotation
// quaternion and translation vector.
struct Transform {
  vec4 rot;
  vec3 pos;
};


// Chains two transforms together. Follows same logic as
// matrix multiplication in terms of transform order.
// Assumes that the rotation quaternion is normalized.
Transform transChain(in Transform a, in Transform b) {
  Transform result;
  result.rot = quatMul(a.rot, b.rot);
  result.pos = quatApply(a.rot, b.pos) + a.pos;
  return result;
}


// Chains two normalized transforms together. Same as above,
// except that the quaternion of the first transform must be
// normalized. The second transform as well as the resulting
// transform may not be normalized.
Transform transChainNorm(in Transform a, in Transform b) {
  Transform result;
  result.rot = quatMul(a.rot, b.rot);
  result.pos = quatApplyNorm(a.rot, b.pos) + a.pos;
  return result;
}


// Applies generic transform to a vector. Equivalent to
// first applying the rotation quaternion and then the
// translation vector.
vec3 transApply(in Transform a, vec3 v) {
  return quatApply(a.rot, v) + a.pos;
}


// Applies normalized transform to a vector. Same as above,
// except that the rotation quaternion must be normalized.
vec3 transApplyNorm(in Transform a, vec3 v) {
  return quatApplyNorm(a.rot, v) + a.pos;
}
