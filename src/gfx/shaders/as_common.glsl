// Specialization constants available to all shaders. Note that the
// mesh shader workgroup size is only meaningful for pipelines that
// have an actual mesh shader.
#define SPEC_CONST_ID_MIN_SUBGROUP_SIZE               (0)
#define SPEC_CONST_ID_MAX_SUBGROUP_SIZE               (1)
#define SPEC_CONST_ID_TASK_SHADER_WORKGROUP_SIZE      (2)
#define SPEC_CONST_ID_MESH_SHADER_WORKGROUP_SIZE      (3)
#define SPEC_CONST_ID_MESH_SHADER_FLAGS               (4)


// Maximum workgroup size that the backend will choose for task shaders
#define MAX_TASK_SHADER_WORKGROUP_SIZE                (64)


// Cull mode and front face state. Used to perform primitive culling
// within the mesh shader. Note that this does not define a front
// face, instead the winding order is used directly.
#define FACE_CULL_MODE_NONE                           (0)
#define FACE_CULL_MODE_CW                             (1)
#define FACE_CULL_MODE_CCW                            (2)


// Convenience macro to generate a scalarization loop for a potentially
// non-uniform value. Will execute one iteration per unique value. Note
// that for floats, you may need to use floatBitsToUint, or otherwise
// this may turn into an infinite loop if any number is NaN.
//
// The subgroupAny condition is only necessary for Nvidia not to hang
// in some situations, other drivers should be able to optimize it away.
//
// Note that for some driver optimizations to kick in, it may be necessary
// to explicitly assign subgroupBroadcastFirst(value) to a local variable.
#define SUBGROUP_SCALARIZE(value)                                           \
  for (bool processed_ = false; !processed_ && subgroupAny(!processed_); )  \
    if (processed_ = (value == subgroupBroadcastFirst(value)))


// Decodes three 10-bit signed normalized integers.
// This is a useful data format for vertex normals.
vec3 unpackSnorm3x10(uint32_t dword) {
  uvec3 uintParts = uvec3(
    bitfieldExtract(dword,  0, 10),
    bitfieldExtract(dword, 10, 10),
    bitfieldExtract(dword, 20, 10));

  // Sign-extend the integers by multiplying
  // the MSB into the higher 26 bits
  uintParts |= (uintParts & 0x200) * 0x3ffffff;

  // Bitcast into a signed integer representation
  ivec3 intParts = ivec3(uintParts);

  // There are two representations for -1, be sure to clamp
  return max(vec3(intParts) * (1.0f / 511.0f), vec3(-1.0f));
}


// Decodes three 10-bit unsigned normalized integers.
vec3 unpackUnorm3x10(uint32_t dword) {
  uvec3 uintParts = uvec3(
    bitfieldExtract(dword,  0, 10),
    bitfieldExtract(dword, 10, 10),
    bitfieldExtract(dword, 20, 10));

  return vec3(uintParts) * (1.0f / 1023.0f);
}


// Encodes three floats as 10-bit signed normalized integers.
uint packSnorm3x10(vec3 data) {
  uvec3 uintParts = clamp(ivec3(roundEven(data * 511.0f)), -511, 511);

  uint result = bitfieldInsert(0, uintParts.x,  0, 10);
  result = bitfieldInsert(result, uintParts.y, 10, 10);
  result = bitfieldInsert(result, uintParts.z, 20, 10);
  return result;
}


// Encodes three floats as 10-bit unsigned normalized integers.
uint packUnorm3x10(vec3 data) {
  uvec3 uintParts = min(uvec3(roundEven(data * 1023.0f)), 1023);

  uint result = bitfieldInsert(0, uintParts.x,  0, 10);
  result = bitfieldInsert(result, uintParts.y, 10, 10);
  result = bitfieldInsert(result, uintParts.z, 20, 10);
  return result;
}


// Fast approximate integer division using floats. This is
// accurate as long as the numbers are both reasonably small.
// Returns uvec2(a / b, a % b).
uvec2 approxIdiv(uint a, uint b) {
  uint quot = uint((float(a) + 0.5f) / float(b));
  return uvec2(quot, a - b * quot);
}


// Computes sin(arccos(s))
float sinForCos(float c) {
  return sqrt(1.0f - c * c);
}


// Computes tan(arccos(s))
float tanForCos(float c) {
  return sinForCos(c) / c;
}


// Tests whether a point lies within a cone. The cone axis must
// be normalized, and the cone cutoff is equal to cos(angle/2).
bool testConePoint(vec3 point, vec3 coneOrigin, vec3 coneAxis, float coneCutoff) {
  vec3 dir = normalize(point - coneOrigin);
  return dot(dir, coneAxis) > coneCutoff;
}


// Tests whether a sphere intersects a cone. The cone axis must be
// normalized, and the absolute cone cutoff must be less than 1.
bool testConeSphere(vec3 sphereCenter, float sphereRadius, vec3 coneOrigin, vec3 coneAxis, float coneCutoff) {
  vec3 dir = sphereCenter - coneOrigin;

  float coneSin = sinForCos(coneCutoff);
  float coneDot = dot(dir, coneAxis);

  // Use Charles Bloom's algorithm to compute the distance
  // between the sphere center and the surface of the cone
  float s = sqrt(dot(dir, dir) - coneDot * coneDot);
  float x = coneCutoff * s - coneDot * coneSin;
  return x <= sphereRadius;
}


// Computes number of workgroups to dispatch for a desired thread
// or item count. Takes the number of items to process, as well as
// the number of items that can be processed in a single workgroup.
uint32_t asComputeWorkgroupCount1D(uint32_t count, uint32_t workgroupSize) {
  return (count + workgroupSize - 1u) / workgroupSize;
}
