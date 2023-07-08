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
