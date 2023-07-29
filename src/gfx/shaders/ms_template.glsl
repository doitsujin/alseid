// Mesh shader template.
//
// Provides a mesh shader implementation that operates on the
// defined geometry structures, and is intended to be used as
// a base for all mesh shaders within a project.
//
// Feature-wise, the implementation supports dual-indexing with
// special optimizations for render passes that do not have any
// fragment shader outputs, morph animations with user-defined
// weights, as well as rudimentary per-primitive culling that
// will compact the final index buffer and discard the entire
// meshlet early if no primitives pass the frustum or back-face
// culling tests.
//
// The following compile-time defines exist:
//
//    MS_NO_SHADING_DATA:   Set if the mesh does not have a data
//                          stream for shading data.
//    MS_NO_VERTEX_DATA:    Set if vertex position data is not needed
//                          to compute fragment shader inputs.
//    MS_NO_MORPH_DATA:     Set if the material does not support morph
//                          target animations.
//    MS_NO_SKINNING:       Set if the geometry is not skinned. Disables
//                          all joint-related computations.
//    MS_NO_UNIFORM_OUTPUT: Set if there are no uniform fragment shader
//                          input values to compute for this shader.
//                          Note that uniform output is required if the
//                          shader exports layer or viewport indices.
//    MS_EXPORT_LAYER:      Set if the render pass needs to export
//                          the gl_Layer built-in.
//    MS_EXPORT_VIEWPORT:   Set if the render pass needs to export
//                          the gl_ViewportIndex built-in.
//    MS_CLIP_PLANES:       Set to the number of clip distances to export.
//                          If not defined, clip planes will be disabled.
//    FS_INPUT:             List of fragment shader input variables.
//                          Each item must use the FS_INPUT_VAR macro.
//
//
// The interface is designed in such a way that most functions
// can work in vertex and compute shaders, and cannot assume
// that they will be called from uniform control flow. Functions
// may be called multiple times for a single vertex.
//
//
// The interface is defined as follows:
//
// Context object that stores meshlet metadata and is passed through
// to all user functions. This may be used to pre-compute transforms.
//
//    struct MsContext {
//      MeshletRef      meshlet;    /* mandatory */
//      uint            cullMode;   /* mandatory, FACE_CULL_MODE_xx */
//      bool            faceFlip;   /* mandatory, swaps vertex order */
// #ifndef MS_NO_SKINNING
//      MeshSkinningRef skinData;   /* Pointer to joint index array */
//      uint            skinOffset; /* Index into joint index array */
// #endif
//      ...
//    };
//
//
// Input vertex data structure as it is stored in the meshlet
// data buffer. The data layout must match exactly. This should
// contain properties such as vertex positions and joint weights.
//
//    struct MsVertexIn { ... };
//
//
// Input shading data structure as it is stored in the meshlet
// data buffer. The data layout must match exactly. Should contain
// all vertex properties that do not contribute to the final position,
// but may contribute to fragment shader inputs, such as texture
// coordinates and the vertex normal.
//
// #ifndef MS_NO_SHADING_DATA
//    struct MsShadingIn { ... };
// #endif
//
//
// Input morph data structure as it is stored in the meshlet
// data buffer. The data layout must match exactly. This is not
// used if MS_NO_MORPH_DATA is set. Should contain delta values
// for all input attributes that are affected by morph targets.
//
// ifndef MS_NO_MORPH_DATA
//    struct MsMorphIn { ... };
// endif
//
//
// Per-vertex fragment shader inputs. This is structure is passed
// directly to the fragment shader at the given location.
//
// #ifdef FS_INPUT
//    struct FsInput { ... };
// #endif
//
//
// Per-vertex output data. Stores built-in vertex outputs such as
// the position, and can be passed back to functions that compute
// fragment shader inputs. Should be as compact as possible since
// this structure is cached in shared memory.
//
//    struct MsVertexOut {
//      vec4  position;             /* mandatory */
//      float clip[MS_CLIP_PLANES]; /* if MS_CLIP_PLANES is set */
//      ...
//    };
//
//
// Workgroup-uniform data used to compute fragment shader inputs.
//
// #ifndef MS_NO_UNIFORM_OUTPUT
//    struct FsUniform {
//      uint layer;             /* if MS_EXPORT_LAYER is set */
//      uint viewport;          /* if MS_EXPORT_VIEWPORT is set */
//      ...
//    };
// #endif
//
//
// Initializes context object for the mesh shader workgroup.
// This is always called at the start of a shader invocation.
//
//    MsContext msGetInstanceContext();
//
//
// Computes uniform data for fragment shader inputs. Within mesh
// shaders, this is always called exactly once, within uniform
// control flow, and only if the meshlet has not been discarded.
// This function is intended to perform one-time computations,
// and may use shared memory and emit barriers.
//
// #ifndef MS_NO_UNIFORM_OUTPUT
//    FsUniform msComputeFsUniform(in MsContext context)
// #endif
//
//
// Loads morph target weight for a specific target. Must apply
// per-instance morph target index offsets as necessary.
//
// #ifndef MS_NO_MORPH_DATA
//    float msLoadMorphTargetWeight(in MsContext context, uint index);
// #endif
//
//
// Loads a joint from memory. The given joint index is already
// fully resolved, i.e. it is absolute within the geometry and
// can be used to index into a constant buffer directly.
//
// #ifndef MS_NO_SKINNING
//    Transform msLoadJointTransform(in MsContext context, uint index);
// #endif
//
//
// Applies morph target to an input vertex. Implementations
// should lay out data in such a way that this can be done
// with simple multiplications and additions.
//
// #ifndef MS_NO_MORPH_DATA
//    void msMorphVertex(
//      inout   MsVertexIn  vertex,     /* Read-write vertex data */
//      in      MsMorphIn   morph,      /* Morph data for this vertex */
//              float       weight);    /* Morph target weight */
// #endif
//
//
// Applies morph target to shading data.
//
// #if !defined(MS_NO_MORPH_DATA) && !defined(MS_NO_SHADING_DATA)
//    void msMorphShading(
//      inout   MsShadingIn shading,    /* Read-write vertex data */
//      in      MsMorphIn   morph,      /* Morph data for this vertex */
//              float       weight);    /* Morph target weight */
// #endif
//
//
// Computes final position and other built-ins of a vertex.
// This should apply all transforms, including projection.
// Applications may store per-vertex data in shared memory,
// using the provided vertex index.
//
//    MsVertexOut msComputeVertexPos(
//      in      MsContext   context,    /* Context object */
//              uint        index,      /* Vertex index */
//      in      MsVertexIn  vertex,     /* Morphed input data */
//      in      Transform   transform); /* Joint transform */
//
//
// Computes fragment shader inputs. The parameters passed to
// this function may vary depending on compile-time defines.
// Applications may read previously stored shared memory data
// using the provided vertex index.
//
//    FsInput msComputeFsInput(
//      in      MsContext   context,    /* Context object */
//      in      MsVertexIn  vertex,     /* if MS_NO_VERTEX_DATA is not set */
//              uint        index,      /* Vertex index */
//      in      MsVertexOut vertexOut,  /* Final vertex position, etc. */
//      in      MsShadingIn shading,    /* if MS_NO_SHADING_DATA is not set */
//      in      FsUniform   fsUniform); /* if MS_NO_UNIFORM_OUTPUT is not set */



// Disable shading data entirely if we do not
// produce any data for a fragment shader
#ifndef MS_NO_SHADING_DATA
  #ifndef FS_INPUT
  #define MS_NO_SHADING_DATA 1
  #endif // FS_INPUT
#endif // MS_NO_SHADING_DATA


// Work out whether we can define the combined
// vertex input data struct at all
#ifndef MS_NO_VERTEX_DATA
#define MS_COMBINED_DATA 1
#endif

#ifndef MS_NO_SHADING_DATA
#define MS_COMBINED_DATA 1
#endif

// Maximum number of morph targets that can concurrently
// affect a single meshlet, i.e. have a non-zero weight.
#define MAX_MORPH_TARGET_COUNT (16)

layout(triangles,
  max_vertices = MAX_VERT_COUNT,
  max_primitives = MAX_PRIM_COUNT) out;


// Fragment shader inputs
#ifdef FS_INPUT
#undef FS_INPUT_VAR
#define FS_INPUT_VAR(l, t, n) layout l out t fs_##n[];
FS_INPUT
#endif // FS_INPUT

// Re-declare built-in vertex outputs with position
// as invariant, so that depth passes are reliable.
out gl_MeshPerVertexEXT {
  invariant vec4 gl_Position;
#ifdef MS_CLIP_PLANES
  float gl_ClipDistance[MS_CLIP_PLANES];
#endif
} gl_MeshVerticesEXT[];


// Helper function to export vertex data. Non-trivial since
// the amount of exports can vary based on application needs.
void msExportVertex(uint index, in MsVertexOut vertex) {
  gl_MeshVerticesEXT[index].gl_Position = vec4(vertex.position);

#ifdef MS_CLIP_PLANES
  for (uint i = 0; i < MS_CLIP_PLANES; i++)
    gl_MeshVerticesEXT[index].gl_ClipDistance[i] = float(vertex.clip[i]);
#endif // MS_CLIP_PLANES
}

// Helper function to emit fragment shader inputs. Needs
// to handle each FS input variable separately.
#ifdef FS_INPUT
#undef FS_INPUT_VAR
#define FS_INPUT_VAR(l, t, n) fs_##n[index] = data.n;

void msExportVertexFsInput(uint index, in FsInput data) {
  FS_INPUT
}
#endif

// Helper function to export primitive data, including indices.
void msExportPrimitive(uint index, u8vec3 indices
#ifndef MS_NO_UNIFORM_OUTPUT
  , in FsUniform fsUniform
#endif
) {
  gl_PrimitiveTriangleIndicesEXT[index] = indices;

#ifdef MS_EXPORT_LAYER
  gl_MeshPrimitivesEXT[index].gl_Layer = uint(fsUniform.layer);
#endif // MS_EXPORT_LAYER

#ifdef MS_EXPORT_VIEWPORT
  gl_MeshPrimitivesEXT[index].gl_ViewportIndex = uint(fsUniform.viewport);
#endif // MS_EXPORT_VIEWPORT
}


// Checks whether dual indexing is enabled for the current meshlet.
bool msMeshletHasDualIndexing(in Meshlet meshlet) {
  return (meshlet.flags & MESHLET_DUAL_INDEX_BIT) != 0;
}


// Helper to load primitive indices
uvec3 msLoadPrimitive(in MsContext context, in Meshlet meshlet, uint index) {
  uint64_t address = meshletComputeAddress(
    context.meshlet, meshlet.primitiveOffset);

  uvec3 result = uvec3(MeshletPrimitiveDataRef(address).primitives[index]);
  return context.faceFlip ? result.xzy : result.xyz;;
}


// Buffer reference type for vertex and shading data. The exact
// data structures are defined by the application.
layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer MsInputVertexDataRef {
  MsVertexIn vertices[];
};


#ifndef MS_NO_SHADING_DATA
layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer MsInputShadingDataRef {
  MsShadingIn vertices[];
};
#endif // MS_NO_SHADING_DATA


// Helper struct to combine both vertex and shading data into
// one structure. Used mostly for morphing everything in one go.
#ifdef MS_COMBINED_DATA
struct MsCombinedIn {
#ifndef MS_NO_VERTEX_DATA
  MsVertexIn vertex;
#endif // MS_NO_VERTEX_DATA
#ifndef MS_NO_SHADING_DATA
  MsShadingIn shading;
#endif // MS_NO_SHADING_DATA
  uint dummy;
};
#endif // MS_COMBINED_DATA


// Buffer reference type for morph data. Morph data may be loaded from
// memory multiple times since the amount of data is expected to be large.
#ifndef MS_NO_MORPH_DATA
layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer MsInputMorphDataRef {
  MsMorphIn vertices[];
};

MsInputMorphDataRef msGetMeshletMorphData(in MsContext context, in Meshlet meshlet) {
  return MsInputMorphDataRef(meshletComputeAddress(context.meshlet, meshlet.morphDataOffset));
}
#endif // MS_NO_MORPH_DATA


// Buffer reference type for per-vertex joint influence data.
#ifndef MS_NO_SKINNING
layout(buffer_reference, buffer_reference_align = 4, scalar)
readonly buffer JointInfluenceRef {
  uint16_t data[];
};

JointInfluenceRef msGetJointInfluenceData(in MsContext context, in Meshlet meshlet) {
  uint64_t address = meshletComputeAddress(context.meshlet, meshlet.jointDataOffset);
  return JointInfluenceRef(address);
}


// Buffer that stores dual quaternions for each local joint. If
// local joints are enabled for a meshlet, we can use this to
// avoid having to laod and convert the transform multiple times.
shared uvec2 msJointDualQuatRShared[MESHLET_LOCAL_JOINT_COUNT];
shared vec4 msJointDualQuatDShared[MESHLET_LOCAL_JOINT_COUNT];

// Convenience method to load a joint transform and convert it to
// a dual-quaternion. We cannot use the representation that uses a
// rotation quaternion with a translation vector directly, since
// interpolating the translation vectors yields wrong results.
DualQuat msLoadJointDualQuatFromMemory(in MsContext context, uint joint) {
  Transform transform = msLoadJointTransform(context, joint);
  return transToDualQuat(transform);
}

// Loads joint from LDS or memory, depending on meshlet flags.
DualQuat msLoadJointDualQuat(in MsContext context, in Meshlet meshlet, uint joint) {
  return DualQuat(
    quatUnpack(msJointDualQuatRShared[joint]),
    msJointDualQuatDShared[joint]);
}

// Loads local joints into shared memory.
void msLoadLocalJointsFromMemory(in MsContext context, in Meshlet meshlet) {
  uint jointCount = min(uint(meshlet.jointCount), MESHLET_LOCAL_JOINT_COUNT);

  MS_LOOP_WORKGROUP(index, jointCount, MESHLET_LOCAL_JOINT_COUNT) {
    uint joint = uint(context.meshlet.jointIndices[index]);

    if (joint < 0xffffu) {
      joint = context.skinData.joints[context.skinOffset + joint];

      DualQuat dq = msLoadJointDualQuatFromMemory(context, joint);
      msJointDualQuatRShared[index] = quatPack(dq.r);
      msJointDualQuatDShared[index] = dq.d;
    }
  }
}

// Accumulates all joint transforms for the given vertex. This will
// essentially iterate over the per-vertex list of joint influences
// and interpolate the dual quaternions using the joint weights.
Transform msComputeJointTransform(in MsContext context, in Meshlet meshlet, in JointInfluenceRef jointData, uint vertexIndex) {
  // If the weight of the first joint is zero, the vertex is not
  // attached to any joints. This works because joint influences
  // are ordered by weight.
  JointInfluence joint = JointInfluence(0u, 0.0f);

  if (meshlet.jointCountPerVertex > 0)
    joint = jointInfluenceUnpack(jointData.data[vertexIndex]);

  if (joint.weight > 0) {
    // Load first joint and skip all the expensive code below if
    // vertices in this meshlet are only influenced by one joint.
    DualQuat accum = msLoadJointDualQuat(context, meshlet, joint.index);

    if (meshlet.jointCountPerVertex > 1) {
      accum.r *= joint.weight;
      accum.d *= joint.weight;

      for (uint i = 1; i < meshlet.jointCountPerVertex; i++) {
        JointInfluence joint = jointInfluenceUnpack(
          jointData.data[i * meshlet.vertexDataCount + vertexIndex]);

        if (joint.weight == 0.0f)
          break;

        DualQuat dq = msLoadJointDualQuat(context, meshlet, joint.index);
        accum.r = fma(dq.r, vec4(joint.weight), accum.r);
        accum.d = fma(dq.d, vec4(joint.weight), accum.d);
      }

      accum = dualQuatNormalize(accum);
    }

    return dualQuatToTrans(accum);
  } else {
    // Return identity transform. Note that this different
    // from running the above code with zero iterations.
    return transIdentity();
  }
}


#endif // MS_NO_SKINNING


// Dual vertex index pairs. Only used if dual indexing is actually
// enabled, otherwise we'll compute the correct numbers on the fly.
shared u8vec2 msVertexDualIndicesShared[MAX_VERT_COUNT];

void msLoadDualVertexIndicesFromMemory(in MsContext context, in Meshlet meshlet) {
  MeshletDualIndexDataRef dualIndexData = MeshletDualIndexDataRef(
    meshletComputeAddress(context.meshlet, meshlet.dualIndexOffset));

  MS_LOOP_WORKGROUP(index, meshlet.vertexCount, MAX_VERT_COUNT) {
    msVertexDualIndicesShared[index] = dualIndexData.vertices[index];
  }
}

uvec2 msGetDualVertexIndex(uint vertexIndex) {
  return uvec2(msVertexDualIndicesShared[vertexIndex]);
}


// Input vertex data. If dual indexing is enabled for the meshlet, or
// if vertex input data is needed in order to compute fragment shader
// input data, it is copied to LDS to reduce memory loads.
shared MsVertexIn msVertexInputShared[MAX_VERT_COUNT];

void msLoadVertexDataFromMemory(in MsContext context, in Meshlet meshlet) {
  MsInputVertexDataRef vertexData = MsInputVertexDataRef(
    meshletComputeAddress(context.meshlet, meshlet.vertexDataOffset));

  MS_LOOP_WORKGROUP(index, meshlet.vertexDataCount, MAX_VERT_COUNT) {
    msVertexInputShared[index] = vertexData.vertices[index];
  }
}


// Output vertex position data. If dual-indexing is used, this is
// generally compacted in such a way that each output position may
// be used by multiple output vertices.
shared MsVertexOut msVertexDataShared[MAX_VERT_COUNT];

bool msCullPrimitive(in MsContext context, uvec3 indices) {
  vec4 a = msVertexDataShared[indices.x].position;
  vec4 b = msVertexDataShared[indices.y].position;
  vec4 c = msVertexDataShared[indices.z].position;

  float minW = min(a.w, min(b.w, c.w));
  float maxW = max(a.w, max(b.w, c.w));

  // Cull primitives behind the camera
  if (maxW < 0.0f)
    return true;

  // Do not bother culling primitives that have a vertex close to
  // or behind the camera, since handling w coordinates becomes
  // very tricky. This should not happen very often.
  if (minW < 0.0005f)
    return false;

  a.xy /= a.w;
  b.xy /= b.w;
  c.xy /= c.w;

  // Perform back-face culling first. Doing this may be useful when
  // rendering heavily-animated objects for which cone culling is
  // not an option.
  if (context.cullMode != FACE_CULL_MODE_NONE) {
    vec2 ab = b.xy - a.xy;
    vec2 ac = c.xy - a.xy;

    vec2 z = ab.xy * ac.yx;

    float face = z.x - z.y;

    if (context.cullMode == FACE_CULL_MODE_CCW)
      face = -face;

    if (face <= 0.0f)
      return true;
  }

  // Cull primitives outside the view frustum. 
  vec2 maxCoord = max(a.xy, max(b.xy, c.xy));
  vec2 minCoord = min(a.xy, min(b.xy, c.xy));

  maxCoord = max(minCoord, -maxCoord);

  if (any(greaterThan(maxCoord, vec2(1.0f))))
    return true;

#ifdef MS_CLIP_PLANES
  // Cull primitive if all vertices are on the
  // wrong side of one of the clipping planes
  for (uint i = 0; i < MS_CLIP_PLANES; i++) {
    float dist = float(max(max(
      msVertexDataShared[indices.x].clip[i],
      msVertexDataShared[indices.y].clip[i]),
      msVertexDataShared[indices.z].clip[i]));

    if (dist <= 0.0f)
      return true;
  }
#endif

  return false;
}


#ifndef MS_NO_MORPH_DATA
// Morph target weight data and mask. We will discard any morph
// targets with a weight of zero in order to speed up processing.
shared MeshletMorphTarget msMorphTargetMetadataShared[MAX_MORPH_TARGET_COUNT];
shared float msMorphTargetWeightsShared[MAX_MORPH_TARGET_COUNT];

shared uint msMorphTargetCountShared;

// Initialize morph target allocator
void msInitMorphTargetAllocator() {
  if (gl_LocalInvocationIndex == 0)
    msMorphTargetCountShared = 0;
}


// Allocates morph target output. Returns index into shared arrays
// to write to. Must be checked against MAX_MORPH_TARGET_COUNT.
uint msAllocateMorphTarget() {
  uvec4 invocationMask = subgroupBallot(true);

  uint count = subgroupBallotBitCount(invocationMask);
  uint index = subgroupBallotBitCount(invocationMask & gl_SubgroupLtMask);

  uint first;

  if (subgroupElect())
    first = atomicAdd(msMorphTargetCountShared, count);

  first = subgroupBroadcastFirst(first);
  return first + index;
}


// Load morph target weights into shared memory and generate a
// precise mask of morph targets that influence the meshlets.
// Any morph targets with a weight of zero will be disabled.
uint msLoadMorphTargetMetadataFromMemory(in MsContext context, in Meshlet meshlet) {
  // Fast path, no morph targets affect this mesh at all
  if (meshlet.morphTargetCount == 0)
    return 0;

  // Location where morph targets for the current meshlet are stored
  MeshletMorphTargetRef morphTargets = MeshletMorphTargetRef(
    meshletComputeAddress(context.meshlet, meshlet.morphTargetOffset));

  MS_LOOP_WORKGROUP(index, meshlet.morphTargetCount, meshlet.morphTargetCount) {
    MeshletMorphTarget metadata = morphTargets.metadata[index];

    float weight = msLoadMorphTargetWeight(context, uint(metadata.targetIndex));

    if (abs(weight) > 0.0001f) {
      uint index = msAllocateMorphTarget();

      if (index < MAX_MORPH_TARGET_COUNT) {
        msMorphTargetMetadataShared[index] = metadata;
        msMorphTargetWeightsShared[index] = weight;
      }
    }
  }

  barrier();

  return min(msMorphTargetCountShared, MAX_MORPH_TARGET_COUNT);
}


// Computes index of morph data structure within a given morph
// target for a given vertex. This essentially works by counting
// all set bits at an index lower than the vertex index.
int msScanMorphTargetVertexMask(in MeshletMorphTarget morphTarget, uint vertexIndex) {
  uint result = 0;

  uint dword = (vertexIndex / 32);
  uint shift = (vertexIndex % 32);
  uint bit = 1u << shift;

  bool set = false;

  for (uint i = 0; i < 4; i++) {
    uint mask = morphTarget.vertexMask[i];

    if (i == dword) {
      set = (mask & bit) != 0;
      mask &= bit - 1;
    }

    if (i <= dword)
      result += bitCount(mask);
  }

  return set ? int(result) : -1;
}


// This unreadable mess generates a function to apply all active
// morph targets within the meshlet to the given set of vertex
// data.
#define MS_DEFINE_MORPH_FUNCTION(function, type, call)                    \
void function(in MsContext context, in Meshlet meshlet,                   \
    inout type variable, uint vertexIndex, uint morphTargetCount) {       \
  MsInputMorphDataRef morphData = msGetMeshletMorphData(context, meshlet);\
  for (uint i = 0; i < morphTargetCount; i++) {                           \
    MeshletMorphTarget target = msMorphTargetMetadataShared[i];           \
    float weight = msMorphTargetWeightsShared[i];                         \
    int dataIndex = msScanMorphTargetVertexMask(target, vertexIndex);     \
    if (dataIndex >= 0) {                                                 \
      MsMorphIn delta = morphData.vertices[dataIndex + target.dataIndex]; \
      call(variable, delta, weight);                                      \
    }                                                                     \
  }                                                                       \
}

MS_DEFINE_MORPH_FUNCTION(msMorphVertexData, MsVertexIn, msMorphVertex)

// Helper Function to morph both vertex and shading inputs in one go, so
// that we can avoid having to iterate over morph targets multiple times.
#ifdef MS_COMBINED_DATA
void msMorphCombined(inout MsCombinedIn data, in MsMorphIn morph, float weight) {
#ifndef MS_NO_VERTEX_DATA
  msMorphVertex(data.vertex, morph, weight);
#endif // MS_NO_VERTEX_DATA
#ifndef MS_NO_SHADING_DATA
  msMorphShading(data.shading, morph, weight);
#endif // MS_NO_SHADING_DATA
}

MS_DEFINE_MORPH_FUNCTION(msMorphCombinedData, MsCombinedIn, msMorphCombined)
#endif // MS_COMBINED_DATA

#endif // MS_NO_MORPH_DATA


// Shared index data array. When written, this is already in output
// order so that emitting primitive data becomes a lot more simple.
shared u8vec4 msIndexDataShared[MAX_PRIM_COUNT];


// Primitive allocation helpers for writing compacted
// index data into local shared memory.
shared uint msIndexDataOffsetShared;

void msInitPrimitiveAllocator() {
  if (gl_LocalInvocationIndex == 0)
    msIndexDataOffsetShared = 0;
}


uint msAllocatePrimitive() {
  uvec4 invocationMask = subgroupBallot(true);

  uint count = subgroupBallotBitCount(invocationMask);
  uint index = subgroupBallotBitCount(invocationMask & gl_SubgroupLtMask);

  uint first;

  if (subgroupElect())
    first = atomicAdd(msIndexDataOffsetShared, count);

  first = subgroupBroadcastFirst(first);
  return first + index;
}


uint msGetOutputPrimitiveCount() {
  return msIndexDataOffsetShared;
}


// Generic mesh shader template with primitive culling,
// optional morphing, and support for dual indexing.
void msMain() {
  msInitPrimitiveAllocator();

#ifndef MS_NO_MORPH_DATA
  msInitMorphTargetAllocator();
#endif

  // Load some data, compute projection matrix etc.
  MsContext context = msGetInstanceContext();
  Meshlet meshlet = context.meshlet.header;

  // Load input vertex data into shared memory for later use
  msLoadVertexDataFromMemory(context, meshlet);

  // If the meshlet has dual-indexing enabled, load index pairs into
  // shared memory as well. We will use this data multiple times.
  bool hasDualIndexing = msMeshletHasDualIndexing(meshlet);

  if (hasDualIndexing)
    msLoadDualVertexIndicesFromMemory(context, meshlet);

  // If local joints are used for this meshlet, load all joints
  // into shared memory so we can reuse them later on.
#ifndef MS_NO_SKINNING
  JointInfluenceRef jointData = msGetJointInfluenceData(context, meshlet);
  msLoadLocalJointsFromMemory(context, meshlet);
#endif

  // Insert a barrier here since LDS initialization needs
  // to have finished before processing morph targets.
  barrier();

  // Find all active morph targets for the current meshlet, given
  // the morph target weights provided by the application.
  uint morphTargetCount = 0;
#ifndef MS_NO_MORPH_DATA
  morphTargetCount = msLoadMorphTargetMetadataFromMemory(context, meshlet);
#endif // MS_NO_MORPH_DATA

  // If any morph targets are active, we cannot compact vertex
  // position data and need to process each output vertex separately.
  bool hasCompactVertexOutputs = hasDualIndexing && morphTargetCount == 0;
  bool hasUnpackedVertexOutputs = hasDualIndexing && morphTargetCount != 0;

  uint outputVertexCount = hasUnpackedVertexOutputs
    ? meshlet.vertexCount
    : meshlet.vertexDataCount;

  // Compute vertex positions and write the results to shared memory
  MS_LOOP_WORKGROUP(index, outputVertexCount, MAX_VERT_COUNT) {
    uint inputIndex = index;

    if (hasUnpackedVertexOutputs)
      inputIndex = msGetDualVertexIndex(index).x;

    MsVertexIn vertexIn = msVertexInputShared[inputIndex];
#ifndef MS_NO_MORPH_DATA
    msMorphVertexData(context, meshlet, vertexIn, index, morphTargetCount);
#endif // MS_NO_MORPH_DATA

#ifndef MS_NO_SKINNING
    Transform jointTransform = msComputeJointTransform(context, meshlet, jointData, inputIndex);
#endif

    msVertexDataShared[index] = msComputeVertexPos(context
      , index, vertexIn
#ifndef MS_NO_SKINNING
      , jointTransform
#endif // MS_NO_SKINNING
    );
  }

  barrier();

  // In case we do not generate any fragment shader outputs, we can
  // export vertex data as-is, but we have to remap vertex indices
  bool hasCompactVertexExports = false;
#ifndef FS_INPUT
  hasCompactVertexExports = hasCompactVertexOutputs;
#endif // FS_INPUT

  // Load primitives and check which ones are visible. We need
  // to potentially remap indices anyway, so culling primitives
  // is not overly expensive.
  MS_LOOP_WORKGROUP(index, meshlet.primitiveCount, MAX_PRIM_COUNT) {
    uvec3 indicesRead = msLoadPrimitive(context, meshlet, index);
    uvec3 indicesExport = indicesRead;

    // If vertex output data is compacted in LDS, we
    // need to remap the indices we use for reading.
    if (hasCompactVertexOutputs) {
      indicesRead = uvec3(
        msGetDualVertexIndex(indicesRead.x).x,
        msGetDualVertexIndex(indicesRead.y).x,
        msGetDualVertexIndex(indicesRead.z).x);
    }

    // If vertex exports are also compacted, we need to
    // adjust the indices we are going to export as well
    if (hasCompactVertexExports)
      indicesExport = indicesRead;

    // Vertex positions are already in shared memory at this
    // point, so we can perform culling and write compacted
    // indices to the correct output location right away.
    if (!msCullPrimitive(context, indicesRead)) {
      uint outputIndex = msAllocatePrimitive();
      msIndexDataShared[outputIndex] = u8vec4(indicesExport.xyz, 0);
    }
  }

  barrier();

  // Allocate output storage for vertex data. We can exit early if all
  // triangles were culled. We will not compact vertex data any further
  // since doing so is generally rather expensive and complicated, and
  // we generally expect most primitives to be visible anyway.
  uint outputPrimitiveCount = msGetOutputPrimitiveCount();

  if (!hasCompactVertexExports)
    outputVertexCount = meshlet.vertexCount;

  // Exit early if all primitives were culled
  if (!msSetMeshOutputs(outputVertexCount, outputPrimitiveCount))
    return;

  // Compute uniform fragment shader inputs
#ifndef MS_NO_UNIFORM_OUTPUT
  FsUniform fsUniform = msComputeFsUniform(context);
#endif

  // Export vertex positions and fragment shader inputs
  MsInputShadingDataRef shadingData = MsInputShadingDataRef(
    meshletComputeAddress(context.meshlet, meshlet.shadingDataOffset));

  MS_LOOP_WORKGROUP(index, outputVertexCount, MAX_VERT_COUNT) {
    // Compute vertex and shading data indices
    uvec2 dualIndex = uvec2(index);

    if (hasDualIndexing && !hasCompactVertexExports)
      dualIndex = msGetDualVertexIndex(index);

    // Compute index of vertex output data in LDS
    uint outputIndex = dualIndex.x;
    
    if (hasUnpackedVertexOutputs)
      outputIndex = index;

    // Export vertex position and built-ins
    MsVertexOut vertexOut = msVertexDataShared[outputIndex];
    msExportVertex(index, vertexOut);

#ifdef FS_INPUT
#ifdef MS_COMBINED_DATA
    MsCombinedIn vertexIn;
#ifndef MS_NO_VERTEX_DATA
    vertexIn.vertex = msVertexInputShared[dualIndex.x];
#endif // MS_NO_VERTEX_DATA
#ifndef MS_NO_SHADING_DATA
    vertexIn.shading = shadingData.vertices[dualIndex.y];
#endif // MS_NO_SHADING_DATA

#ifndef MS_NO_MORPH_DATA
    msMorphCombinedData(context, meshlet, vertexIn, index, morphTargetCount);
#endif // MS_NO_MORPH_DATA
#endif // MS_COMBINED_DATA

    FsInput fsInput = msComputeFsInput(context
      , outputIndex
#ifndef MS_NO_VERTEX_DATA
      , vertexIn.vertex
#endif // MS_NO_VERTEX_DATA
      , vertexOut
#ifndef MS_NO_SHADING_DATA
      , vertexIn.shading
#endif // MS_NO_SHADING_DATA
#ifndef MS_NO_UNIFORM_OUTPUT
      , fsUniform
#endif // MS_NO_UNIFORM_OUTPUT
    );

    msExportVertexFsInput(index, fsInput);
#endif // FS_INPUT
  }

  // Export primitives. This is trivial since the final
  // indices are already stored in shared memory.
  MS_LOOP_WORKGROUP(index, outputPrimitiveCount, MAX_PRIM_COUNT) {
    u8vec3 indices = msIndexDataShared[index].xyz;
    msExportPrimitive(index, indices
#ifndef MS_NO_UNIFORM_OUTPUT
      , fsUniform
#endif // MS_NO_UNIFORM_OUTPUT
    );
  }
}


// Mesh shader entry point
void main() {
  MS_MAIN();
}
