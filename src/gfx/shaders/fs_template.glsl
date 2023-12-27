// Fragment shader template.
//
// Trivial in that this only defines an interface which
// takes the fragment shader inputs as an argument.
//
//
// The interface is defined as follows:
//
// Main function of the fragment shader.
//
//    #define FS_MAIN fsMain
//    void fsMain(in FsInput input)


// Need to enable mesh shader extensions for per-primitive
// inputs, but avoid using it if not necessary.
#ifdef FS_UNIFORM
#extension GL_EXT_mesh_shader : enable
#endif


// Per-vertex fragment shader inputs
#ifdef FS_INPUT
#undef FS_INPUT_VAR
#define FS_INPUT_VAR(l, t, n) layout l in t fs_##n;
FS_INPUT
#endif


// Per-primitive fragment shader inputs
#ifdef FS_UNIFORM
#undef FS_INPUT_VAR
#define FS_INPUT_VAR(l, t, n) layout l perprimitiveEXT flat in t fs_##n;
FS_UNIFORM
#endif


// Fragment shader entry point
void main() [[subgroup_uniform_control_flow]] {
#ifdef FS_INPUT
#undef FS_INPUT_VAR
#define FS_INPUT_VAR(l, t, n) fsInput.n = fs_##n;
  FsInput fsInput;
  FS_INPUT
#endif

#ifdef FS_UNIFORM
#undef FS_INPUT_VAR
#define FS_INPUT_VAR(l, t, n) fsUniform.n = fs_##n;
  FsUniform fsUniform;
  FS_UNIFORM
#endif

  FS_MAIN(
#ifdef FS_INPUT
    fsInput
#endif
#ifdef FS_UNIFORM
#ifdef FS_INPUT
    ,
#endif
    fsUniform
#endif
  );
}
