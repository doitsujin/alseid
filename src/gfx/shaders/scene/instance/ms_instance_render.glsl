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
//    MS_NO_MOTION_VECTORS: Set if motion vectors are not used for the material
//                          or render pass. Disables all code that computes the
//                          vertex position based on previous frame data.
//    MS_EXPORT_LAYER:      Set if the render pass needs to export
//                          the gl_Layer built-in.
//    MS_EXPORT_VIEWPORT:   Set if the render pass needs to export
//                          the gl_ViewportIndex built-in.
//    MS_CLIP_PLANES:       Set to the number of clip distances to export.
//                          If not defined, clip planes will be disabled.
//    FS_INPUT:             List of fragment shader input variables.
//                          Each item must use the FS_INPUT_VAR macro.
//    FS_UNIFORM:           List of per-primitive fragment shader input
//                          variables. Works the same way as FS_INPUT.
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
//      MsInvocationInfo  invocation;   /* mandatory */
//      MsRenderState     renderState;  /* mandatory */
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
// Per-vertex output data. Stores built-in vertex outputs such as
// the position, and can be passed back to functions that compute
// fragment shader inputs. Should be as compact as possible since
// this structure is cached in shared memory.
//
//    struct MsVertexOut {
//      vec4  position;             /* mandatory */
//      vec3  oldPosition;          /* mandatory for motion vectors,
//                                   * stores xyw components only. */
//      float clip[MS_CLIP_PLANES]; /* if MS_CLIP_PLANES is set */
//      ...
//    };
//
//
// Workgroup-uniform data used to compute fragment shader inputs.
//
// #ifndef MS_NO_UNIFORM_OUTPUT
//    struct MsUniformOut {
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
//    MsUniformOut msComputeUniformOutput(in MsContext context)
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
// This is also used to compute the previous frame's vertex
// positions for motion vectors, in which case only the final
// vertex position should be computed, and no shared memory
// should be written.
//
//    MsVertexOut msComputeVertexOutput(
//      in      MsContext   context,    /* Context object */
//              uint        index,      /* Vertex index */
//      in      MsVertexIn  vertex,     /* Morphed input data */
//      in      MsVertexIn  vertexOld,  /* Previous frame's morphed input data */
//      in      Transform   transform,; /* Joint transform */
//      in      Transform   transformOld); /* Previous frame's joint transform */
//
//
// Computes fragment shader inputs. The parameters passed to
// this function may vary depending on compile-time defines.
// Applications may read previously stored shared memory data
// using the provided vertex index.
//
//    FsInput msComputeFsInput(
//      in      MsContext   context,    /* Context object */
//              uint        vertexIndex, /* Vertex index */
//      in      MsVertexIn  vertexIn,   /* if MS_NO_VERTEX_DATA is not set */
//      in      MsVertexOut vertexOut,  /* Final vertex position, etc. */
//      in      MsShadingIn shadingIn,  /* if MS_NO_SHADING_DATA is not set */
//      in      MsUniformOut uniformOut); /* if MS_NO_UNIFORM_OUTPUT is not set */
#ifndef MS_INSTANCE_RENDER_H
#define MS_INSTANCE_RENDER_H

#include "as_instance.glsl"

#define MS_MAIN msMain


// Per-vertex fragment shader inputs
#ifdef FS_INPUT
#undef FS_INPUT_VAR
#define FS_INPUT_VAR(l, t, n) layout l out t fs_##n[];
FS_INPUT
#endif // FS_INPUT


// Per-primitive fragment shader inputs
#ifdef FS_UNIFORM
#undef FS_INPUT_VAR
#define FS_INPUT_VAR(l, t, n) layout l perprimitiveEXT flat out t fs_##n[];
FS_UNIFORM
#endif // FS_UNIFORM


// Re-declare built-in vertex outputs with position
// as invariant, so that depth passes are reliable.
out gl_MeshPerVertexEXT {
  invariant vec4 gl_Position;
#if MS_CLIP_PLANES > 0
  float gl_ClipDistance[MS_CLIP_PLANES];
#endif // MS_CLIP_PLANES
} gl_MeshVerticesEXT[];


// Helper function to export vertex data. Non-trivial since
// the amount of exports can vary based on application needs.
void msExportVertex(uint index, in MsVertexOut vertex) {
  gl_MeshVerticesEXT[index].gl_Position = vec4(vertex.position);

#if MS_CLIP_PLANES > 0
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
#endif // FS_INPUT

// Helper function to export primitive indices.
void msExportPrimitive(uint index, u8vec3 indices) {
  gl_PrimitiveTriangleIndicesEXT[index] = indices;
}


// Helper function to export uniforms as primitive data.
#ifndef MS_NO_UNIFORM_OUTPUT
void msExportUniform(uint count, in MsUniformOut msUniform) {
  uint32_t tid = gl_LocalInvocationIndex;

  if (tid < count) {
#ifdef MS_EXPORT_LAYER
    gl_MeshPrimitivesEXT[tid].gl_Layer = uint(msUniform.layer);
#endif // MS_EXPORT_LAYER

#ifdef MS_EXPORT_VIEWPORT
    gl_MeshPrimitivesEXT[tid].gl_ViewportIndex = uint(msUniform.viewport);
#endif // MS_EXPORT_VIEWPORT

#ifdef FS_UNIFORM
#undef FS_INPUT_VAR
#define FS_INPUT_VAR(l, t, n) fs_##n[tid] = msUniform.n;
    FS_UNIFORM
#endif // FS_UNIFORM
  }
}
#endif // MS_NO_UNIFORM_OUTPUT


// Shared primitive and vertex allocators. When running a single
// subgroup, a subgroup-uniform global variable will be used,
// otherwise we have to resort to shared memory.
shared uint msPrimAllocatorShared;
shared uint msVertAllocatorShared;

uint msPrimAllocatorGlobal;
uint msVertAllocatorGlobal;

void msInitDataAllocators() {
  if (IsSingleSubgroup) {
    msPrimAllocatorGlobal = 0u;
    msVertAllocatorGlobal = 0u;
  } else {
    if (gl_LocalInvocationIndex == 0u) {
      msPrimAllocatorShared = 0u;
      msVertAllocatorShared = 0u;
    }

    barrier();
  }
}


// Helper function to allocate up to one primitive and vertex for the
// current thread. Used to store and export compacted output data.
u32vec2 msAllocateVertexAndPrimitive(
        bool                          vert,
        bool                          prim) {
  uvec4 vertBallot = subgroupBallot(vert);
  uvec4 primBallot = subgroupBallot(prim);

  uint vertCount = subgroupBallotBitCount(vertBallot);
  uint primCount = subgroupBallotBitCount(primBallot);

  uint vertIndex = subgroupBallotExclusiveBitCount(vertBallot);
  uint primIndex = subgroupBallotExclusiveBitCount(primBallot);

  uint firstVert;
  uint firstPrim;

  if (IsSingleSubgroup) {
    firstVert = msVertAllocatorGlobal;
    firstPrim = msPrimAllocatorGlobal;

    msVertAllocatorGlobal += vertCount;
    msPrimAllocatorGlobal += primCount;
  } else {
    if (subgroupElect()) {
      firstVert = atomicAdd(msVertAllocatorShared, vertCount);
      firstPrim = atomicAdd(msPrimAllocatorShared, primCount);
    }

    firstVert = subgroupBroadcastFirst(firstVert);
    firstPrim = subgroupBroadcastFirst(firstPrim);
  }

  return u32vec2(
    firstVert + vertIndex,
    firstPrim + primIndex);
}


// Returns total number of allocated primitives and vertices.
u32vec2 msGetOutputCounts() {
  if (IsSingleSubgroup)
    return u32vec2(msVertAllocatorGlobal, msPrimAllocatorGlobal);
  else
    return u32vec2(msVertAllocatorShared, msPrimAllocatorShared);
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


#ifndef MS_NO_MOTION_VECTORS
// Checks whether motion vectors can be used for the current instance.
// This is the case if the instance has been updated in the current
// frame and does not have the flag to explicitly disable them.
bool msInstanceUsesMotionVectors(in MsContext context) {
  return !asTest(context.flags, MS_NO_MOTION_VECTORS_BIT)
      && !asTest(context.invocation.instanceNode.flags, INSTANCE_NO_MOTION_VECTORS_BIT);
}
#endif // MS_NO_MOTION_VECTORS


// Number of joint and morph target sets
#ifndef MS_NO_MOTION_VECTORS
#define MS_SKIN_SETS (2u)
#else
#define MS_SKIN_SETS (1u)
#endif


#ifndef MS_NO_MORPH_DATA
// Buffer reference type for morph data. Morph data may be loaded from
// memory multiple times since the amount of data is expected to be large.
layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer MsInputMorphDataRef {
  MsMorphIn vertices[];
};


// Helper function to load morph target data for a given vertex. Must only
// be called for vertices which are affected by the given morph target.
MsMorphIn msLoadMorphData(
        MsInputMorphDataRef           morphData,
        uint32_t                      morphTarget,
  in    MeshletMorphTargetVertexInfo  vertex) {
  uint32_t localIndex = bitCount(uint32_t(vertex.morphTargetMask) & ((1u << morphTarget) - 1u));
  return morphData.vertices[uint32_t(vertex.morphDataIndex) + localIndex];
}


// Morph target weights, and the active morph target mask. Any target with
// a weight of zero can be skipped.
shared int16_t msMorphTargetWeightsShared[MS_SKIN_SETS][MESHLET_LOCAL_MORPH_TARGET_COUNT];
shared uint32_t msMorphTargetActiveMaskShared;

int32_t msMorphTargetWeightsGlobal;
uint32_t msMorphTargetActiveMaskGlobal;


// Retrieves active morph target mask for the given set.
// Excludes any morph targets with a weight of zero.
uint32_t msGetMorphTargetMask(
        uint32_t                      weightSet) {
  uint32_t mask = msMorphTargetActiveMaskGlobal;

  return bitfieldExtract(mask,
    int(MESHLET_LOCAL_MORPH_TARGET_COUNT * weightSet),
    int(MESHLET_LOCAL_MORPH_TARGET_COUNT));
}


// Retrieves the weight of a morph target as a floating point number.
// The morph target index must be uniform.
float msGetMorphTargetWeight(
        uint32_t                      weightSet,
        uint32_t                      index) {
  int32_t weight;

  if (IsFullSubgroup && gl_SubgroupSize >= MESHLET_LOCAL_MORPH_TARGET_COUNT) {
    i16vec2 packed = unpack16(subgroupBroadcast(msMorphTargetWeightsGlobal, index));
    weight = weightSet == 0u ? packed.x : packed.y;
  } else {
    weight = msMorphTargetWeightsShared[weightSet][index];
  }

  return clamp(weight / 32767.0f, -1.0f, 1.0f);
}


// Loads morph target weights from memory and stores them either in
// a shared array, or in a single vector register if the subgroup
// size is sufficiently large.
void msLoadMorphTargetWeightsFromMemory(
  in    MsContext                     context,
  in    Meshlet                       meshlet) {
  bool useSubgroupPath = IsFullSubgroup && gl_SubgroupSize >= MESHLET_LOCAL_MORPH_TARGET_COUNT;

  MeshletSkinIndexRef skinIndices = MeshletSkinIndexRef(
    meshletComputeAddress(context.invocation.meshletVa, meshlet.skinIndexOffset));

  uint32_t tid = useSubgroupPath
    ? gl_SubgroupInvocationID
    : gl_LocalInvocationIndex;

  i16vec2 weights = i16vec2(0s, 0s);

  if (tid < meshlet.morphTargetCount) {
    uint32_t index = skinIndices.indices[meshlet.jointCount + tid];

    if (index < context.invocation.instanceInfo.weightCount) {
      weights.x = instanceLoadMorphTargetWeight(
        context.invocation.instanceNode.propertyBuffer, 0u, index);

#ifndef MS_NO_MOTION_VECTORS
      weights.y = instanceLoadMorphTargetWeight(
        context.invocation.instanceNode.propertyBuffer, 1u, index);
#endif // MS_NO_MOTION_VECTORS
    }
  }

  if (useSubgroupPath) {
    // Pack the weight pair to make sure we only use one register.
    // We can use simple ballots to generate the mask,
    msMorphTargetWeightsGlobal = pack32(weights);
    msMorphTargetActiveMaskGlobal = subgroupBallot(weights.x != 0s).x;

#ifndef MS_NO_MOTION_VECTORS
    msMorphTargetActiveMaskGlobal |= subgroupBallot(weights.y != 0s).x << MESHLET_LOCAL_MORPH_TARGET_COUNT;
#endif // MS_NO_MOTION_VECTORS
  } else {
    if (tid == 0u)
      msMorphTargetActiveMaskShared = 0u;

    barrier();

    // Store weights as they are
    if (tid < MESHLET_LOCAL_MORPH_TARGET_COUNT) {
      msMorphTargetWeightsShared[0u][tid] = weights.x;
#ifndef MS_NO_MOTION_VECTORS
      msMorphTargetWeightsShared[1u][tid] = weights.y;
#endif // MS_NO_MOTION_VECTORS
    }

    // Compute morph target weight and let each subgroup
    // write its individual portion to the shared mask.
    uint32_t mask = 0u;

    if (weights.x != 0s)
      mask |= 1u << (tid);
    if (weights.y != 0s)
      mask |= 1u << (tid + MESHLET_LOCAL_MORPH_TARGET_COUNT);

    mask = subgroupOr(mask);

    if (subgroupElect() && mask != 0u)
      atomicOr(msMorphTargetActiveMaskShared, mask);

    barrier();

    msMorphTargetActiveMaskGlobal = msMorphTargetActiveMaskShared;
  }
}


// Loads morph target metadata for a given vertex.
MeshletMorphTargetVertexInfo msLoadMorphTargetVertexInfoFromMemory(
  in    MsContext                     context,
  in    Meshlet                       meshlet,
        uint32_t                      vertex) {
  return MeshletMorphTargetVertexInfoRef(meshletComputeAddress(
    context.invocation.meshletVa, meshlet.morphTargetOffset)).vertexInfos[vertex];
}


// Applies all active morph targets to vertex input data. Essentially
// iterates over all active morph targets and processes them one by one.
void msMorphVertexData(
  in    MsContext                     context,
  in    Meshlet                       meshlet,
  inout MsVertexParameters            vertexArgs,
  in    MeshletMorphTargetVertexInfo  vertexInfo) {
  MsInputMorphDataRef morph = MsInputMorphDataRef(meshletComputeAddress(
    context.invocation.meshletVa, meshlet.morphDataOffset));

  uint32_t mask = msGetMorphTargetMask(0u);

#ifndef MS_NO_MOTION_VECTORS
  if (msInstanceUsesMotionVectors(context))
    mask |= msGetMorphTargetMask(1u);
#endif // MS_NO_MOTION_VECTORS

  mask &= uint32_t(vertexInfo.morphTargetMask);
  mask = subgroupOr(mask);

  while (mask != 0u) {
    uint32_t index = findLSB(mask);

    // Load weights. If motion vectors are enabled for the instance, load
    // the previous frame's weight, otherwise use the same weight for both.
    f32vec2 weights = f32vec2(msGetMorphTargetWeight(0u, index));

#ifndef MS_NO_MOTION_VECTORS
    if (msInstanceUsesMotionVectors(context))
      weights.y = msGetMorphTargetWeight(1u, index);
#endif // MS_NO_MOTION_VECTORS

    // Apply morph target to all affected vertices
    if (asTest(uint32_t(vertexInfo.morphTargetMask), 1u << index)) {
      MsMorphIn morphData = msLoadMorphData(morph, index, vertexInfo);
      msMorphVertex(vertexArgs.currFrame.vertexData, morphData, weights.x);

#ifndef MS_NO_MOTION_VECTORS
      msMorphVertex(vertexArgs.prevFrame.vertexData, morphData, weights.y);
#endif // MS_NO_MOTION_VECTORS
    }

    mask &= mask - 1u;
  }
}


#ifndef MS_NO_SHADING_DATA
// Applies all active morph targets to vertex input data. Essentially
// iterates over all active morph targets and processes them one by one.
void msMorphShadingData(
  in    MsContext                     context,
  in    Meshlet                       meshlet,
  inout MsShadingIn                   shadingData,
  in    MeshletMorphTargetVertexInfo  vertexInfo) {
  MsInputMorphDataRef morph = MsInputMorphDataRef(meshletComputeAddress(
    context.invocation.meshletVa, meshlet.morphDataOffset));

  uint32_t mask = msGetMorphTargetMask(0u);
  mask &= uint32_t(vertexInfo.morphTargetMask);
  mask = subgroupOr(mask);

  while (mask != 0u) {
    uint32_t index = findLSB(mask);

    // Load weights. If motion vectors are enabled for the instance, load
    // the previous frame's weight, otherwise use the same weight for both.
    float weight = msGetMorphTargetWeight(0u, index);

    // Apply morph target to all affected vertices
    if (asTest(uint32_t(vertexInfo.morphTargetMask), 1u << index)) {
      MsMorphIn morphData = msLoadMorphData(morph, index, vertexInfo);
      msMorphShading(shadingData, morphData, weight);
    }

    mask &= mask - 1u;
  }
}
#endif // MS_NO_SHADING_DATA

#endif // MS_NO_MORPH_DATA


#ifndef MS_NO_SKINNING
// Buffer reference type for per-vertex joint influence data.
layout(buffer_reference, buffer_reference_align = 4, scalar)
readonly buffer JointInfluenceRef {
  uint16_t data[];
};


// Joint data. Just use shared memory here for simplicity.
shared f32vec4 msJointDualQuatSharedR[MS_SKIN_SETS][MESHLET_LOCAL_JOINT_COUNT];
shared f32vec4 msJointDualQuatSharedD[MS_SKIN_SETS][MESHLET_LOCAL_JOINT_COUNT];


// Joint transform for when only one joint is used in the meshlet.
// This is uniform within the workgroup.
Transform msMeshletJointTransformGlobal[MS_SKIN_SETS];


// Helper function to retrieve the dual quaternion for a given local joint.
DualQuat msLoadJointDualQuat(
        uint32_t                      jointSet,
        uint32_t                      index) {
  return DualQuat(
    msJointDualQuatSharedR[jointSet][index],
    msJointDualQuatSharedD[jointSet][index]);
}


// Helper function to store a dual-quaternion in LDS.
void msStoreJointDualQuat(
        uint32_t                      jointSet,
        uint32_t                      localIndex,
  in    DualQuat                      dq) {
  if (jointSet < MS_SKIN_SETS) {
    msJointDualQuatSharedR[jointSet][localIndex] = dq.r;
    msJointDualQuatSharedD[jointSet][localIndex] = dq.d;
  }

  barrier();
}


// Helper function to load a joint transform from the instance.
Transform msLoadInstanceJointTransform(
  in    MsContext                     context,
        uint32_t                      jointSet,
        uint32_t                      jointIndex) {
  InstanceJointBufferIn jointBuffer = InstanceJointBufferIn(
    context.invocation.instanceNode.propertyBuffer +
    context.invocation.instanceInfo.jointAbsoluteOffset);
  return jointBuffer.joints[jointIndex + jointSet * context.invocation.instanceInfo.jointCount].transform;
}


// Helper functions to load joints from memory.
void msLoadJointsFromMemory(
  in    MsContext                     context,
  in    Meshlet                       meshlet) {
  MeshSkinningRef skinningBuffer = MeshSkinningRef(context.invocation.skinningVa);

  if (meshlet.jointCount == 0us) {
    uint32_t index = meshlet.jointIndex;

    if (index != 0xffffu)
      index = skinningBuffer.joints[context.invocation.meshInstance.jointIndex + index];

    if (index < context.invocation.instanceInfo.jointCount) {
      // Load the joint transforms from memory as normal.
      [[unroll]]
      for (uint32_t i = 0u; i < MS_SKIN_SETS; i++)
        msMeshletJointTransformGlobal[i] = msLoadInstanceJointTransform(context, i, index);
    } else {
      // If no joint is defined for the current meshlet, initialize the
      // global transform with the identity transform so we won't have
      // to make this a special case later.
      [[unroll]]
      for (uint32_t i = 0u; i < MS_SKIN_SETS; i++)
        msMeshletJointTransformGlobal[i] = transIdentity();
    }
  } else {
    MeshletSkinIndexRef skinIndices = MeshletSkinIndexRef(
      meshletComputeAddress(context.invocation.meshletVa, meshlet.skinIndexOffset));

    uint32_t tid = gl_LocalInvocationIndex;

    // Zero-initialize initial set of transforms in case any
    // local joints have an invalid mapping in the skin buffer.
    Transform transforms = transIdentity();

    uint32_t localSet = tid / MESHLET_LOCAL_JOINT_COUNT;
    uint32_t localJoint = tid & (MESHLET_LOCAL_JOINT_COUNT - 1u);

    if (localSet < MS_SKIN_SETS && localJoint < uint32_t(meshlet.jointCount)) {
      uint32_t index = skinIndices.indices[localJoint];

      if (index != 0xffffu)
        index = skinningBuffer.joints[context.invocation.meshInstance.jointIndex + index];

      if (index < context.invocation.instanceInfo.jointCount)
        transforms = msLoadInstanceJointTransform(context, localSet, index);
    }

    msStoreJointDualQuat(localSet, localJoint, transToDualQuat(transforms));
  }
}


// Accumulates all joint transforms for the given vertex. This will
// essentially iterate over the per-vertex list of joint influences
// and interpolate the dual quaternions using the joint weights.
Transform msComputeJointTransform(
  in    MsContext                     context,
  in    Meshlet                       meshlet,
        uint32_t                      jointSet,
        uint32_t                      vertexIndex) {
  if (meshlet.jointCount == 0us) {
    // Return the global joint transform, which may be an identity
    // transform if no joint is specified for the meshlet.
    return msMeshletJointTransformGlobal[jointSet];
  } else {
    JointInfluenceRef jointData = JointInfluenceRef(meshletComputeAddress(
      context.invocation.meshletVa, meshlet.jointDataOffset));

    JointInfluence joint = meshletDecodeJointInfluence(jointData.data[vertexIndex]);

    // Load first joint, and account for the possibility that the
    // vertex may not be attached to any joint. This works because
    // joint influences are ordered by weight.
    DualQuat accum = msLoadJointDualQuat(jointSet, joint.index);
    accum.r *= joint.weight;
    accum.d *= joint.weight;

    if (joint.weight == 0.0f)
      accum.r.w = 1.0f;

    for (uint32_t i = 1; i < meshlet.jointCountPerVertex; i++) {
      JointInfluence joint = meshletDecodeJointInfluence(
        jointData.data[i * meshlet.vertexDataCount + vertexIndex]);

      if (joint.weight == 0.0f)
        break;

      DualQuat dq = msLoadJointDualQuat(jointSet, joint.index);
      accum.r = fma(dq.r, f32vec4(joint.weight), accum.r);
      accum.d = fma(dq.d, f32vec4(joint.weight), accum.d);
    }

    accum = dualQuatNormalize(accum);
    return dualQuatToTrans(accum);
  }
}

#endif // MS_NO_SKINNING


// Checks whether we need to export vertex data locally and pipe it through
// the shared memory array, or if locally caching the data is enough.
bool msUseLocalInvocationVertexExport() {
  return asTest(MsFlags, MS_PREFER_LOCAL_VERTEX_EXPORT);
}


// Checks whether we need to export primitive data locally.
bool msUseLocalInvocationPrimitiveExport() {
  return asTest(MsFlags, MS_PREFER_LOCAL_PRIMITIVE_EXPORT);
}


// Output vertex cache. Stores vertex data computed by each invocation
// either in shared memory, or a global array variable.
shared MsVertexOut msVertexExportDataShared[gl_WorkGroupSize.x];

MsVertexOut msVertexExportDataLocal;
uint32_t msVertexExportIndexLocal = 0u;


#ifndef MS_NO_SHADING_DATA
shared uint8_t msShadingDataIndexShared[gl_WorkGroupSize.x];
uint32_t msShadingDataIndexLocal = 0u;
#endif // MS_NO_SHADING_DATA


#ifndef MS_NO_MORPH_DATA
shared MeshletMorphTargetVertexInfo msVertexMorphTargetInfosShared[gl_WorkGroupSize.x];
MeshletMorphTargetVertexInfo msVertexMorphTargetInfosLocal;
#endif // MS_NO_MORPH_DATA


// Helper function to store computed vertex output.
void msStoreVertexExportData(
        uint32_t                      localGroup,
        uint32_t                      localIndex,
        u8vec2                        dualIndex,
        uint32_t                      vertexIndex,
  in    MeshletMorphTargetVertexInfo  morphInfo,
  in    MsVertexOut                   vertexData,
        bool                          useVertex) {
  if (msUseLocalInvocationVertexExport()) {
    if (useVertex) {
      msVertexExportDataShared[vertexIndex] = vertexData;

#ifndef MS_NO_SHADING_DATA
      msShadingDataIndexShared[vertexIndex] = dualIndex.y;
#endif // MS_NO_SHADING_DATA

#ifndef MS_NO_MORPH_DATA
      if (msGetMorphTargetMask(0u) != 0u)
        msVertexMorphTargetInfosShared[vertexIndex] = morphInfo;
#endif // MS_NO_MORPH_DATA
    }
  } else {
    msVertexExportDataLocal = vertexData;
    msVertexExportIndexLocal = useVertex ? vertexIndex : gl_WorkGroupSize.x;

#ifndef MS_NO_SHADING_DATA
    msShadingDataIndexLocal = uint32_t(dualIndex.y);
#endif // MS_NO_SHADING_DATA

#ifndef MS_NO_MORPH_DATA
    msVertexMorphTargetInfosLocal = morphInfo;
#endif // MS_NO_MORPH_DATA
  }
}


// Helper function to export all vertices. Must only be called after
// allocating output storage for the mesh shader workgroup.
void msExportVertexData(uint32_t exportCount) {
  if (msUseLocalInvocationVertexExport()) {
    uint32_t index = gl_LocalInvocationIndex;

    if (index < exportCount)
      msExportVertex(index, msVertexExportDataShared[index]);
  } else {
    if (msVertexExportIndexLocal < exportCount)
      msExportVertex(msVertexExportIndexLocal, msVertexExportDataLocal);
  }
}


// Loads cached dual index data for a given output vertex
uint32_t msLoadVertexDualIndexData(
        uint32_t                      exportIndex) {
  if (msUseLocalInvocationVertexExport()) {
    return msShadingDataIndexShared[exportIndex];
  } else {
    return msShadingDataIndexLocal;
  }
}


// Loads output vertex data for a given output vertex.
MsVertexOut msLoadVertexOutputData(
        uint32_t                      exportIndex) {
  if (msUseLocalInvocationVertexExport())
    return msVertexExportDataShared[exportIndex];
  else
    return msVertexExportDataLocal;
}


// Loads export index for a given output vertex. The returned
// index may be out of bounds, in which case it must be ignored.
uint32_t msLoadVertexExportIndex(
        uint32_t                      outputIndex) {
  if (msUseLocalInvocationVertexExport())
    return outputIndex;
  else
    return msVertexExportIndexLocal;
}


// Loads per-vertex morph target infos.
#ifndef MS_NO_MORPH_DATA
MeshletMorphTargetVertexInfo msLoadVertexMorphTargetInfo(
        uint32_t                      exportIndex) {
  if (msUseLocalInvocationVertexExport())
    return msVertexMorphTargetInfosShared[exportIndex];
  else
    return msVertexMorphTargetInfosLocal;
}
#endif // MS_NO_MORPH_DATA

// Output primitive cache. Works essentially the same way as the
// vertex cache, except that we only store vertex indices here.
shared u8vec4 msPrimitiveExportDataShared[gl_WorkGroupSize.x];

uint32_t msPrimitiveExportDataLocal;
uint32_t msPrimitiveExportIndexLocal = 0u;


// Helper function to store primitive data for later use.
void msStorePrimitiveExportData(
        uint32_t                      localGroup,
        uint32_t                      localIndex,
        uint32_t                      primitiveIndex,
        u8vec3                        primitiveData,
        bool                          usePrimitive) {
  if (msUseLocalInvocationPrimitiveExport()) {
    if (usePrimitive)
      msPrimitiveExportDataShared[primitiveIndex] = u8vec4(primitiveData, 0u);
  } else {
    msPrimitiveExportDataLocal = pack32(u8vec4(primitiveData, 0u));
    msPrimitiveExportIndexLocal = usePrimitive ? primitiveIndex : gl_WorkGroupSize.x;
  }
}


// Helper function to export primitive indices.
void msExportPrimitiveIndices(uint32_t exportCount) {
  if (msUseLocalInvocationPrimitiveExport()) {
    uint32_t index = gl_LocalInvocationIndex;

    if (index < exportCount)
      msExportPrimitive(index, msPrimitiveExportDataShared[index].xyz);
  } else {
    if (msPrimitiveExportIndexLocal < exportCount)
      msExportPrimitive(msPrimitiveExportIndexLocal, unpack8(msPrimitiveExportDataLocal).xyz);
  }
}


// Helper function to perform primitive culling.
#define MS_CLIP_MASK_NEGATIVE_W (1u << 8u)
#define MS_CLIP_MASK_IGNORE     (1u << 9u)

shared f32vec2 msCullPositionsShared[gl_WorkGroupSize.x];
shared uint16_t msClipMasksShared[gl_WorkGroupSize.x];

bool msCullPrimitive(
  in    MsContext                     context,
        uint32_t                      primCount,
        uint32_t                      localGroup,
        uint32_t                      localIndex,
        u32vec3                       indices,
  in    MsVertexOut                   vertexData) {
  // Cull primitives that are entirely behind the camera.
  uint32_t clipMask = vertexData.position.w <= 0.0f ? MS_CLIP_MASK_NEGATIVE_W : 0u;

  // Do not bother culling primitives that have a vertex close to
  // or behind the camera, since handling w coordinates becomes
  // very tricky. This should not happen very often.
  clipMask |= vertexData.position.w <= 0.0005f ? MS_CLIP_MASK_IGNORE : 0u;

  // Set a bit in the clip mask for each plane for which the
  // current vertex lies outside the clip space.
#if MS_CLIP_PLANES > 0
  [unroll]
  for (uint32_t i = 0u; i < MS_CLIP_PLANES; i++)
    clipMask |= vertexData.clip[i] < 0.0f ? 1u << i : 0u;
#endif

  // Read vertex positions and clipping info from other invocations while
  // still in uniform control flow.
  f32vec2 position = f32vec2(vertexData.position.xy) / vertexData.position.w;
  f32vec2 a, b, c;
  u32vec3 clip;

  if (IsFullSubgroup && gl_SubgroupSize == MESHLET_GROUP_SIZE) {
    a = subgroupShuffle(position, indices.x);
    b = subgroupShuffle(position, indices.y);
    c = subgroupShuffle(position, indices.z);

    clip = u32vec3(
      subgroupShuffle(clipMask, indices.x),
      subgroupShuffle(clipMask, indices.y),
      subgroupShuffle(clipMask, indices.z));
  } else {
    uint32_t base = MESHLET_GROUP_SIZE * localGroup;

    msCullPositionsShared[base + localIndex] = position;
    msClipMasksShared[base + localIndex] = uint16_t(clipMask);

    barrier();

    a = msCullPositionsShared[base + indices.x];
    b = msCullPositionsShared[base + indices.y];
    c = msCullPositionsShared[base + indices.z];

    clip = u32vec3(
      msClipMasksShared[base + indices.x],
      msClipMasksShared[base + indices.y],
      msClipMasksShared[base + indices.z]);
  }

  // Bound-check the primitive index now, we don't want to
  // lose invocations while doing the subgroup shuffles.
  if (localIndex >= primCount)
    return false;

  // Investigate the clipping plane mask of each vertex.
  uint32_t clipCull = (clip.x & clip.y & clip.z) & ~MS_CLIP_MASK_IGNORE;
  uint32_t clipIgnore = (clip.x | clip.y | clip.z) & MS_CLIP_MASK_IGNORE;

  if (clipCull != 0u)
    return false;

  if (clipIgnore != 0u)
    return true;

  // Cull primitives outside the view frustum.
  vec2 maxCoord = max(a.xy, max(b.xy, c.xy));
  vec2 minCoord = min(a.xy, min(b.xy, c.xy));

  maxCoord = max(minCoord, -maxCoord);

  if (max(maxCoord.x, maxCoord.y) > 1.0f)
    return false;

  // Perform back-face culling first. Doing this may be useful when
  // rendering heavily-animated objects for which cone culling is
  // not an option.
  uint cullMode = context.flags & MS_CULL_FACE_ANY;

  if (cullMode != 0u) {
    vec2 ab = b.xy - a.xy;
    vec2 ac = c.xy - a.xy;

    vec2 z = ab.xy * ac.yx;

    float face = z.x - z.y;

    if (cullMode == MS_CULL_FACE_CCW_BIT)
      face = -face;

    if (face <= 0.0f)
      return false;
  }

  return true;
}


// Helper function to compute the vertices used in visible primitives.
// Any vertices not set in this mask can safely be discarded.
shared uint32_t msVertexMasksShared[gl_WorkGroupSize.x / MESHLET_GROUP_SIZE];

uint32_t msComputeVertexMask(
        uint32_t                      localGroup,
        uint32_t                      localIndex,
        u8vec3                        indices,
        bool                          usePrimitive) {
  uint32_t mask = 0u;

  if (usePrimitive) {
    mask |= 1u << uint32_t(indices.x);
    mask |= 1u << uint32_t(indices.y);
    mask |= 1u << uint32_t(indices.z);
  }

  if (IsFullSubgroup && gl_SubgroupSize >= MESHLET_GROUP_SIZE) {
    // Large subgroups make this trivial
    mask = subgroupClusteredOr(mask, MESHLET_GROUP_SIZE);
  } else {
    // Otherwise, just do a reduction in LDS
    if (localIndex == 0u)
      msVertexMasksShared[localGroup] = 0u;

    barrier();

    atomicOr(msVertexMasksShared[localGroup], mask);
    barrier();

    mask = msVertexMasksShared[localGroup];
    barrier();
  }

  return mask;
}


// Helper function to remap local primitive indices to export indices.
shared uint8_t msVertexIndexMapShared[gl_WorkGroupSize.x];

u8vec3 msRemapPrimitiveIndices(
        uint32_t                      localGroup,
        uint32_t                      localIndex,
        u8vec3                        primitive,
        bool                          usePrimitive,
        uint32_t                      vertexIndex,
        uint32_t                      vertexMask) {
  u32vec3 result = u32vec3(primitive);

  if (IsFullSubgroup && gl_SubgroupSize >= MESHLET_GROUP_SIZE) {
    // Use vertex mask to compute local vertex indices, which is consistent
    // with how export indices are allocated on this code path.
    uint32_t baseIndex;

    if (gl_SubgroupSize == MESHLET_GROUP_SIZE) {
      // Fast path where each subgroup processes exactly one group. For some
      // reason, NV breaks with subgroupBroadcastFirst here despite control
      // flow being uniform.
      baseIndex = subgroupBroadcast(vertexIndex, 0u);
    } else if (gl_SubgroupSize == MESHLET_GROUP_SIZE * 2u) {
      // Operate on the assumption that this is faster than a shuffle,
      // which should be the case on AMD hardware in wave64 mode.
      uint32_t lo = subgroupBroadcast(vertexIndex, 0u);
      uint32_t hi = subgroupBroadcast(vertexIndex, MESHLET_GROUP_SIZE);
      baseIndex = asTest(gl_SubgroupInvocationID, MESHLET_GROUP_SIZE) ? hi : lo;
    } else {
      // Otherwise, just fall back to a simple shuffle
      baseIndex = subgroupShuffle(vertexIndex, gl_SubgroupInvocationID & ~(MESHLET_GROUP_SIZE - 1u));
    }

    if (usePrimitive) {
      result.x = baseIndex + bitCount(bitfieldExtract(vertexMask, 0, int(result.x)));
      result.y = baseIndex + bitCount(bitfieldExtract(vertexMask, 0, int(result.y)));
      result.z = baseIndex + bitCount(bitfieldExtract(vertexMask, 0, int(result.z)));
    }
  } else {
    uint32_t localOffset = MESHLET_GROUP_SIZE * localGroup;
    msVertexIndexMapShared[localOffset + localIndex] = uint8_t(vertexIndex);

    barrier();

    if (usePrimitive) {
      result.x = msVertexIndexMapShared[localOffset + result.x];
      result.y = msVertexIndexMapShared[localOffset + result.y];
      result.z = msVertexIndexMapShared[localOffset + result.z];
    }
  }

  return u8vec3(result);
}


// Convenience method to load node transforms from memory.
Transform msLoadNodeTransform(
  in    MsContext                     context,
        uint32_t                      transformSet) {
  uint32_t transformIndex = transformSet == 0u
    ? context.invocation.nodeTransformIndices.x
    : context.invocation.nodeTransformIndices.y;

  SceneNodeTransformBufferIn nodeTransforms = SceneNodeTransformBufferIn(context.invocation.nodeTransformVa);
  return nodeTransforms.nodeTransforms[transformIndex].absoluteTransform;
}


// Helpers to store and retrieve primitive group information. If subgroups
// are sufficiently large, we will use a single vector register to store
// all relevant information, otherwise we will use LDS.
#define MS_MAX_GROUP_COUNT (16u)

shared uint32_t msPrimitiveGroupInfosPackedShared[MS_MAX_GROUP_COUNT];

uint32_t msPrimitiveGroupInfosPackedGlobal;

struct MsMeshletPrimitiveGroupInfo {
  uint32_t vertIndex;
  uint32_t vertCount;
  uint32_t primIndex;
  uint32_t primCount;
};

void msStorePrimitiveGroupInfo(
  in    MsMeshletPrimitiveGroupInfo   groupInfo) {
  uint32_t packed = (groupInfo.vertIndex <<  0) | (groupInfo.vertCount <<  9) |
                    (groupInfo.primIndex << 16) | (groupInfo.primCount << 25);

  if (IsFullSubgroup && gl_SubgroupSize >= MS_MAX_GROUP_COUNT) {
    msPrimitiveGroupInfosPackedGlobal = packed;
  } else {
    uint32_t tid = gl_LocalInvocationIndex;

    if (tid < MS_MAX_GROUP_COUNT)
      msPrimitiveGroupInfosPackedShared[tid] = packed;

    barrier();
  }
}


// Retrieves info for a given primitive group. This expects the
// group index to be uniform within each set of 32 threads.
MsMeshletPrimitiveGroupInfo msGetPrimitiveGroupInfo(
        uint32_t                      groupIndex) {
  uint32_t packed;

  if (IsFullSubgroup && gl_SubgroupSize >= MS_MAX_GROUP_COUNT) {
    packed = gl_SubgroupSize > MESHLET_GROUP_SIZE
      ? subgroupShuffle(msPrimitiveGroupInfosPackedGlobal, groupIndex)
      : subgroupBroadcast(msPrimitiveGroupInfosPackedGlobal, groupIndex);
  } else {
    packed = msPrimitiveGroupInfosPackedShared[groupIndex];
  }

  MsMeshletPrimitiveGroupInfo result;
  result.vertIndex = bitfieldExtract(packed,  0,  9);
  result.vertCount = bitfieldExtract(packed,  9,  7);
  result.primIndex = bitfieldExtract(packed, 16,  9);
  result.primCount = bitfieldExtract(packed, 25,  7);
  return result;
}


// Helper function to read primitive group information from the meshlet
// buffer and return the relevant counts and indices for the current group.
layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer MeshletPrimitiveGroupRef {
  MeshletPrimitiveGroup groups[];
};

void msLoadPrimitiveGroupInfosFromMemory(
  in    MsContext                     context) {
  MeshletRef meshlet = MeshletRef(context.invocation.meshletVa);

  if (IsFullSubgroup && gl_SubgroupSize >= MS_MAX_GROUP_COUNT) {
    uint32_t sid = gl_SubgroupInvocationID;

    // Fast path, just load all groups at once and compute
    // indices by performing a subgroup reduction.
    MsMeshletPrimitiveGroupInfo result;
    result.vertCount = 0u;
    result.primCount = 0u;

    if (sid < context.invocation.groupCount) {
      MeshletPrimitiveGroup group = meshlet.groups[sid];
      result.vertCount = group.vertexCount;
      result.primCount = group.primitiveCount;
    }

    result.vertIndex = subgroupExclusiveAdd(result.vertCount);
    result.primIndex = subgroupExclusiveAdd(result.primCount);

    msStorePrimitiveGroupInfo(result);
  } else {
    // Otherwise we need to use LDS. Use the array that stores
    // packed group info for the temporary reduction as well.
    uint32_t tid = gl_LocalInvocationIndex;

    MsMeshletPrimitiveGroupInfo result;
    result.vertCount = 0u;
    result.primCount = 0u;

    if (tid < context.invocation.groupCount) {
      MeshletPrimitiveGroup group = meshlet.groups[tid];
      result.vertCount = group.vertexCount;
      result.primCount = group.primitiveCount;
    }

    if (tid < MS_MAX_GROUP_COUNT) {
      msPrimitiveGroupInfosPackedShared[tid] = pack32(
        u16vec2(result.vertCount, result.primCount));
    }

    barrier();

    // Do an inclusive scan over the counts to compute indices
    for (uint32_t i = 1u; i < MS_MAX_GROUP_COUNT; i += i) {
      u16vec2 read;

      if (tid >= i && tid < MS_MAX_GROUP_COUNT) {
        read = unpack16(msPrimitiveGroupInfosPackedShared[tid]) +
          unpack16(msPrimitiveGroupInfosPackedShared[tid - i]);
      }

      barrier();

      if (tid >= i && tid < MS_MAX_GROUP_COUNT)
        msPrimitiveGroupInfosPackedShared[tid] = pack32(read);

      barrier();
    }

    if (tid < MS_MAX_GROUP_COUNT) {
      // Adjust indices to store the exclusive scan results
      u16vec2 packedIndices = unpack16(msPrimitiveGroupInfosPackedShared[tid]);
      result.vertIndex = packedIndices.x - result.vertCount;
      result.primIndex = packedIndices.y - result.primCount;
    }

    barrier();

    // This writes to LDS, barrier above is not optional.
    msStorePrimitiveGroupInfo(result);
  }
}


// Helper function to load vertex input data from the meshlet buffer.
MsVertexIn msLoadVertexDataFromMemory(
  in    MsContext                     context,
  in    Meshlet                       meshlet,
        uint32_t                      vertexIndex) {
  return MsInputVertexDataRef(meshletComputeAddress(
    context.invocation.meshletVa, meshlet.vertexDataOffset)).vertices[vertexIndex];
}


// Helper function to load shading data from the meshlet buffer.
MsShadingIn msLoadShadingDataFromMemory(
  in    MsContext                     context,
  in    Meshlet                       meshlet,
        uint32_t                      vertexIndex) {
  return MsInputShadingDataRef(meshletComputeAddress(
    context.invocation.meshletVa, meshlet.shadingDataOffset)).vertices[vertexIndex];
}


// Helper function to load a pair of vertex indices from the meshlet buffer. The
// first index points to the vertex input data, the second to the shading data.
u8vec2 msLoadVertexIndicesFromMemory(
  in    MsContext                     context,
  in    Meshlet                       meshlet,
        uint32_t                      index) {
  return MeshletVertexIndexRef(meshletComputeAddress(
    context.invocation.meshletVa, meshlet.groupVertexOffset)).vertices[index];
}


// Helper function to load primitive indices from the meshlet buffer.
uint16_t msLoadPrimitiveFromMemory(
  in    MsContext                     context,
  in    Meshlet                       meshlet,
        uint32_t                      index) {
  return MeshletPrimitiveDataRef(meshletComputeAddress(
    context.invocation.meshletVa, meshlet.groupPrimitiveOffset)).primitives[index];
}


// Computes local group index within the current workgroup.
uint32_t msComputeLocalGroup() {
  if (IsFullSubgroup && gl_SubgroupSize <= MESHLET_GROUP_SIZE)
    return (gl_SubgroupID * gl_SubgroupSize) / MESHLET_GROUP_SIZE;
  else if (IsFullSubgroup && gl_SubgroupSize > MESHLET_GROUP_SIZE)
    return (gl_SubgroupInvocationID + gl_SubgroupID * gl_SubgroupSize) / MESHLET_GROUP_SIZE;
  else
    return gl_LocalInvocationIndex / MESHLET_GROUP_SIZE;
}


// Computes local vertex and primitive index within the current group.
uint32_t msComputeLocalIndex() {
  uint32_t tid = IsFullSubgroup
    ? gl_SubgroupInvocationID + gl_SubgroupID * gl_SubgroupSize
    : gl_LocalInvocationIndex;

  return tid & (MESHLET_GROUP_SIZE - 1u);
}


// Generic mesh shader template.
void msMain() {
  msInitDataAllocators();

  MsContext context = msGetInstanceContext();
  Meshlet meshlet = MeshletRef(context.invocation.meshletVa).header;

  // Load information about the 32-wide primitive group
  msLoadPrimitiveGroupInfosFromMemory(context);

  // Load node transforms for both the previous and current frame
  // from memory. Ignore whether or not motion vectors are valid
  // for now to not stall on these loads for the time being.
  Transform currNodeTransform = msLoadNodeTransform(context, 0u);

#ifndef MS_NO_MOTION_VECTORS
  Transform prevNodeTransform = msLoadNodeTransform(context, 1u);
#endif // MS_NO_MOTION_VECTORS

#ifndef MS_NO_SKINNING
  msLoadJointsFromMemory(context, meshlet);
#endif // MS_NO_SKINNING

#ifndef MS_NO_MORPH_DATA
  msLoadMorphTargetWeightsFromMemory(context, meshlet);
#endif // MS_NO_MORPH_DATA

  // Main loop that computes vertex outputs for all primitive groups.
  // The maximum iteration count is known at compile time.
  uint32_t localGroup = msComputeLocalGroup();
  uint32_t localIndex = msComputeLocalIndex();

  uint32_t groupIndex = context.invocation.firstGroup + localGroup;

  // The group index can technically be out of bounds here, however
  // this is not an issue as vertex and primitive counts would be 0.
  MsMeshletPrimitiveGroupInfo groupInfo = msGetPrimitiveGroupInfo(groupIndex);

  // Load vertex indices first since we need them to load vertex data
  u8vec2 dualIndex = u8vec2(0u);

  if (localIndex < groupInfo.vertCount)
    dualIndex = msLoadVertexIndicesFromMemory(context, meshlet, groupInfo.vertIndex + localIndex);

  // Load morph target data from memory
  MeshletMorphTargetVertexInfo morphVertex = { };

#ifndef MS_NO_MORPH_DATA
  if (meshlet.morphTargetCount != 0u && localIndex < groupInfo.vertCount)
    morphVertex = msLoadMorphTargetVertexInfoFromMemory(context, meshlet, groupInfo.vertIndex + localIndex);
#endif // MS_NO_MORPH_DATA

  // Load primitive data immediately after, so that we can
  // access it later with no additional latency.
  uint16_t primitiveData = 0us;

  if (localIndex < groupInfo.primCount)
    primitiveData = msLoadPrimitiveFromMemory(context, meshlet, groupInfo.primIndex + localIndex);

  // Default-initialize vertex data since we we need to run the
  // primitive culling code using it in uniform control flow.
  MsVertexIn vertexIn = { };
  MsVertexOut vertexOut = { };

  if (localIndex < groupInfo.vertCount) {
    vertexIn = msLoadVertexDataFromMemory(context, meshlet, dualIndex.x);

    MsVertexParameters vertexArgs;
    vertexArgs.currFrame.vertexData = vertexIn;
    vertexArgs.currFrame.node = currNodeTransform;

#ifndef MS_NO_SKINNING
    vertexArgs.currFrame.joint = msComputeJointTransform(
      context, meshlet, 0u, dualIndex.x);
#endif // MS_NO_SKINNING


#ifndef MS_NO_MOTION_VECTORS
    if (msInstanceUsesMotionVectors(context)) {
      vertexArgs.prevFrame.vertexData = vertexIn;
      vertexArgs.prevFrame.node = prevNodeTransform;

#ifndef MS_NO_SKINNING
      vertexArgs.prevFrame.joint = msComputeJointTransform(
        context, meshlet, 1u, dualIndex.x);
#endif // MS_NO_SKINNING
    } else {
      // If motion vectors are disabled for the instance, just use
      // the current frame's data to produce consistent results.
      vertexArgs.prevFrame = vertexArgs.currFrame;
    }
#endif // MS_NO_MOTION_VECTORS

#ifndef MS_NO_MORPH_DATA
    // Morphing vertex data is special in that we'll handle both
    // old and new data in one go in order to reduce overhead.
    msMorphVertexData(context, meshlet, vertexArgs, morphVertex);
#endif // MS_NO_MORPH_DATA

    vertexOut = msComputeVertexOutput(context, vertexArgs);
  }

  // Perform primitive culling using the computed vertex positions.
  u8vec3 primitive = meshletDecodePrimitive(primitiveData);

  bool usePrimitive = msCullPrimitive(context, groupInfo.primCount,
    localGroup, localIndex, primitive, vertexOut);

  // Compute active vertex mask for all primitives that were not culled.
  uint32_t vertexMask = msComputeVertexMask(
    localGroup, localIndex, primitive, usePrimitive);

  bool useVertex = bitfieldExtract(vertexMask, int(localIndex), 1) != 0u;

  // Allocate primitives in a way that allows us to compact the output,
  // regarless of mesh shader flags.
  u32vec2 outputIndices = msAllocateVertexAndPrimitive(useVertex, usePrimitive);

  msStoreVertexExportData(localGroup, localIndex,
    dualIndex, outputIndices.x, morphVertex, vertexOut, useVertex);

  // Compute final vertex export indices for the local primitive.
  primitive = msRemapPrimitiveIndices(localGroup, localIndex,
    primitive, usePrimitive, outputIndices.x, vertexMask);

  msStorePrimitiveExportData(localGroup, localIndex,
    outputIndices.y, primitive, usePrimitive);

  // Ensure that all the above code actually finishes before allocating
  // outputs and exporting primitives, since we may read LDS for it.
  barrier();

  // Allocate output storage and immediately export already available
  // vertex data to potentially free up some registers.
  u32vec2 outputCounts = msGetOutputCounts();

  if (!msSetMeshOutputs(outputCounts.x, outputCounts.y))
    return;

  msExportVertexData(outputCounts.x);
  msExportPrimitiveIndices(outputCounts.y);


#ifndef MS_NO_UNIFORM_OUTPUT
  // Compute uniform fragment shader inputs and export immediately.
  MsUniformOut msUniform = msComputeUniformOut(context);
  msExportUniform(outputCounts.y, msUniform);
#endif // MS_NO_UNIFORM_OUTPUT


#ifdef FS_INPUT
  // At this point, we will no longer need cross-thread communication,
  // however we should make sure to export to the current local thread
  // index on devices where local export is preferred.
  uint32_t exportIndex = msLoadVertexExportIndex(gl_LocalInvocationIndex);

  if (exportIndex < outputCounts.x) {
    MsShadingParameters args;

#ifndef MS_NO_VERTEX_DATA
    args.vertexData = msLoadVertexOutputData(exportIndex);
#endif // MS_NO_VERTEX_DATA


#ifndef MS_NO_SHADING_DATA
    uint32_t shadingDataIndex = msLoadVertexDualIndexData(exportIndex);
    args.shadingData = msLoadShadingDataFromMemory(context, meshlet, shadingDataIndex);

#ifndef MS_NO_MORPH_DATA
    if (msGetMorphTargetMask(0u) != 0u) {
      msMorphShadingData(context, meshlet, args.shadingData,
        msLoadVertexMorphTargetInfo(exportIndex));
    }
#endif // MS_NO_MORPH_DATA
#endif // MS_NO_SHADING_DATA


#ifndef MS_NO_UNIFORM_OUTPUT
    args.uniformData = msUniform;
#endif // MS_NO_UNIFORM_OUTPUT

    FsInput fsInput = msComputeFsInput(context, args);
    msExportVertexFsInput(exportIndex, fsInput);
  }
#endif // FS_INPUT
}

#endif // MS_INSTANCE_RENDER_H
