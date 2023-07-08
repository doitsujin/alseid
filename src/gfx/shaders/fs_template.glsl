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



// Per-vertex fragment shader inputs
#ifdef FS_INPUT
#undef FS_INPUT_VAR
#define FS_INPUT_VAR(l, t, n) layout l in t fs_##n;
FS_INPUT
#endif


// Fragment shader entry point
void main() {
#ifdef FS_INPUT
#undef FS_INPUT_VAR
#define FS_INPUT_VAR(l, t, n) fsInput.n = fs_##n;
  FsInput fsInput;
  FS_INPUT
#endif

  FS_MAIN(
#ifdef FS_INPUT
    fsInput
#endif
  );
}
