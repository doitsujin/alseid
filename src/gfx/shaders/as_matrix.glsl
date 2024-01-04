// Computes complete 4x4 matrix from partial 4x3 matrix. This is
// a useful way to compactly store transform matrices where the
// last row is implied to be (0, 0, 0, 1).
mat4 unpack3x4(mat3x4 matrix) {
  return mat4(transpose(matrix));
}


// Packs 4x4 matrix to 4x3 matrix by omitting the last row and
// transposing. The transpose may be useful for memory layouts.
mat3x4 pack3x4(mat4 matrix) {
  return transpose(mat4x3(matrix));
}


// Compact projection representation. Supports either projection
// or orthographic projections without needing the full matrix.
struct Projection {
  float xScale;
  float yScale;
  float zScale;
  float zBias;
};


vec4 projApply(in Projection p, vec3 v) {
  bool isPerspective = (p.zScale == 0.0f);

  vec4 result;
  result.x = v.x * p.xScale;
  result.y = v.y * p.yScale;
  result.z = v.z * p.zScale + p.zBias;
  result.w = isPerspective ? -v.z : 1.0f;
  return result;
}


// View frustum. Can be computed directly from a projection.
struct ViewFrustum {
  f32vec4 planes[6];
};

ViewFrustum projComputeViewFrustum(in Projection p) {
  bool isPerspective = p.zScale == 0.0f;

  vec2 zw = isPerspective
    ? vec2(-1.0f, 0.0f)
    : vec2(0.0f, 1.0f);

  float zNear, zFar;

  if (isPerspective) {
    zNear = p.zBias;
    zFar = 0.0f;
  } else {
    float invScale = 1.0f / p.zScale;
    zFar = p.zBias * invScale;
    zNear = zFar - invScale;
  }

  ViewFrustum f;
  f.planes[0] = planeNormalize(vec4(-p.xScale, 0.0f, zw));
  f.planes[1] = planeNormalize(vec4( p.xScale, 0.0f, zw));
  f.planes[2] = planeNormalize(vec4(0.0f, -p.yScale, zw));
  f.planes[3] = planeNormalize(vec4(0.0f,  p.yScale, zw));
  f.planes[4] = vec4(0.0f, 0.0f, -1.0f, -zNear);
  f.planes[5] = isPerspective ? vec4(0.0f) : vec4(0.0f, 0.0f, 1.0f, zFar);
  return f;
}
