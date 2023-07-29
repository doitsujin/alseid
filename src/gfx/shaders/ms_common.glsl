// Let the backend decide the optimal workgroup
// size for us, depending on driver preferences.
layout(local_size_x_id = SPEC_CONST_ID_MESH_SHADER_WORKGROUP_SIZE) in;

// Restrict output to 128 vertices and primitives
// by default, unless overridden by the application.
#ifndef MAX_VERT_COUNT
#define MAX_VERT_COUNT (128)
#endif

#ifndef MAX_PRIM_COUNT
#define MAX_PRIM_COUNT (128)
#endif

// Useful constants for some compile-time subgroup optimizations
bool IsSingleSubgroup = (gl_NumSubgroups == 1);
bool IsPackedSubgroup = (gl_NumSubgroups * gl_SubgroupSize == gl_WorkGroupSize.x);

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