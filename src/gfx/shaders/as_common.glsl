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
