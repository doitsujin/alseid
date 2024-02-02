#ifndef MS_COMMON_H
#define MS_COMMON_H 1

// Let the backend decide the optimal workgroup
// size for us, depending on driver preferences.
layout(local_size_x_id = SPEC_CONST_ID_MESH_SHADER_WORKGROUP_SIZE) in;

// Restrict output to 128 vertices and primitives
// by default, unless overridden by the application.
#ifndef MS_MAX_VERT_COUNT
#define MS_MAX_VERT_COUNT (128)
#endif

#ifndef MS_MAX_PRIM_COUNT
#define MS_MAX_PRIM_COUNT (128)
#endif

// Convenience macro to perform count invocations of
// subsequent code with arbitrary workgroup sizes.
#define MS_LOOP_WORKGROUP(var, cond, count)                         \
  for (uint i_ = 0, var; i_ < (count); i_ += gl_WorkGroupSize.x)    \
    if ((var = gl_LocalInvocationIndex + i_) < (cond))

// Helper function to set up mesh output count. Sets
// the vertex count to 0 if the primitive count is
// also 0, and returns false in that case.
bool msSetMeshOutputs(uint verts, uint prims) {
  SetMeshOutputsEXT(prims != 0u ? verts : 0u, prims);
  return prims != 0u;
}

#endif // MS_COMMON_H
