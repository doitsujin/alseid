// Disable shading data entirely if we do not
// produce any data for a fragment shader
#ifndef MS_NO_SHADING_DATA
  #ifndef FS_INPUT
  #define MS_NO_SHADING_DATA 1
  #endif // FS_INPUT
#endif // MS_NO_SHADING_DATA


// Define fragment shader inputs.
#ifdef FS_INPUT
#undef FS_INPUT_VAR
#define FS_INPUT_VAR(l, t, n) layout l out t fs_##n;
FS_INPUT
#endif


// Buffer reference type for vertex and shading data. The exact
// data structures are defined by the application.
layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer VsInputVertexDataRef {
  MsVertexIn vertices[];
};


#ifndef MS_NO_SHADING_DATA
layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer VsInputShadingDataRef {
  MsShadingIn vertices[];
};
#endif // MS_NO_SHADING_DATA


// Buffer reference type for morph data.
#ifndef MS_NO_MORPH_DATA
layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer VsInputMorphDataRef {
  MsMorphIn vertices[];
};
#endif



void vsExportPrimitive(in MsContext context) {
#ifdef EXPORT_LAYER
  gl_Layer = int(context.layer);
#endif

#ifdef EXPORT_VIEWPORT
  gl_ViewportIndex = int(context.viewport);
#endif
}


out gl_PerVertex {
  invariant vec4 gl_Position;
#ifdef CLIP_PLANES
  float gl_ClipDistance[CLIP_PLANES];
#endif
};


void vsExportVertex(in MsVertexOut vertex) {
  gl_Position = vec4(vertex.position);

#ifdef CLIP_PLANES
  for (uint i = 0; i < CLIP_PLANES; i++)
    gl_ClipDistance[i] = vertex.clip[i];
#endif
}


void vsMain() {

}


// Vertex shader entry point
void main() {
  VS_MAIN();
}
