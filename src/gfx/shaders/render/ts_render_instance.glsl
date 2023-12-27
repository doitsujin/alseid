// Task shader template for rendering regular instances.
//
// Provides a task shader implementation that processes geometry meshlets,
// and emits one mesh shader workgroup for each visible meshlet. Emitting
// multiple meshlets per invocation may be useful when rendering multiple
// layers, e.g. for reflection probes or shadow maps.
//
// The thread ID components have the following semantics:
// - x: Meshlet index within the selected LOD.
// - y: Local instance ID of the selected mesh.
// - z: Pass index, for concurrent multi-pass rendering.
//
// The following compile-time defines exist:
//
//    TS_MAX_MESHLET_COUNT: Maximum number of meshlets emitted per invocation.
//                          Used to size the meshlet payload array appropriately.
//
// The interface is defined as follows:
//
// Context object that stores invocation info, as well as pointers to various
// GPU buffers to read data from.
//
//    struct TsContext {
//      TsInvocationInfo  invocation;   /* mandatory */
//      uint64_t          drawListVa;   /* mandatory; stores pointer to draw list */
//      uint64_t          instanceVa;   /* mandatory; stores pointer to instance data */
//      uint64_t          sceneVa;      /* mandatory; stores pointer to node data */
//      uint32_t          frameId;      /* mandatory; stores current frame ID */
//    };
//
// Initializes context object for the task shader workgroup.
// This is always called at the start of a shader invocation.
//
//    TsContext tsGetInstanceContext();
//
// Computes a sub-pass visibility mask for a given meshlet. For each bit set in
// the resulting mask, a mesh shader workgroup will be dispatched.
//
//    uint32_t tsTestMeshletVisibility(
//      in      TsContext             context,  /* Instance context */
//      in      TsMeshletCullingInfo  meshlet); /* Meshlet metadata */
#ifndef TS_RENDER_INSTANCE_H
#define TS_RENDER_INSTANCE_H

#define TS_MAIN tsMain

// Node transforms for the current and previous frames. If the previous
// frame's transform is not valid, the task shader will implicitly reuse
// the current frame's transform in both fields.
shared Transform tsNodeTransformsShared[2];

void tsLoadNodeTransformsFromMemory(in TsContext context, uint32_t nodeIndex) {
  uint32_t tid = gl_LocalInvocationIndex;

  SceneHeader scene = SceneHeader(context.sceneVa); 
  SceneNodeTransformBufferIn nodeTransforms = SceneNodeTransformBufferIn(context.sceneVa + scene.nodeTransformOffset);

  if (tid < 2u) {
    uint32_t invocationFrameId = context.frameId - tid;
    uint32_t transformIndex = nodeComputeTransformIndices(nodeIndex, scene.nodeCount, invocationFrameId).x;

    uint32_t updateFrameId = nodeTransforms.nodeTransforms[transformIndex].updateFrameId;

    if (updateFrameId < invocationFrameId)
      invocationFrameId = context.frameId;

    tsNodeTransformsShared[tid] = nodeTransforms.nodeTransforms[transformIndex].absoluteTransform;
  }

  barrier();
}

uint tsMain() {
  uint32_t tid = gl_LocalInvocationIndex;

  TsContext context = tsGetInstanceContext();

  // Load instance node
  InstanceNodeBufferIn instanceNodes = InstanceNodeBufferIn(context.instanceVa);
  InstanceNode instanceNode = instanceNodes.nodes[context.invocation.instanceIndex];

  tsLoadNodeTransformsFromMemory(context, instanceNode.nodeIndex);

  // Load mesh properties
  GeometryRef geometry = GeometryRef(instanceNode.geometryBuffer);
  Geometry geometryMetadata = geometry.geometry;

  Mesh mesh = geometry.meshes[context.invocation.meshIndex];

  // Compute absolute meshlet index for the current invocation
  MeshLod lod = meshGetLodData(geometry, mesh).lods[context.invocation.lodIndex];

  uint32_t meshletIndex = uint32_t(lod.meshletIndex) + context.invocation.lodMeshletIndex;
  uint32_t meshletCount = uint32_t(lod.meshletIndex) + uint32_t(lod.meshletCount);

  // Locate meshlet data buffer for the selected LOD
  GeometryBufferPointerRef geometryBuffers = geometryGetBuffers(geometry, geometryMetadata);
  MeshletMetadataRef dataBuffer = geometryGetEmbeddedBuffer(geometry, geometryMetadata);

  if (lod.bufferIndex > 0u)
    dataBuffer = geometryBuffers.buffers[lod.bufferIndex - 1u];

  // Load mesh instance and skinning-related properties
  MeshInstance meshInstance = tsLoadMeshInstance(geometry, mesh, context.invocation.meshInstance);

  InstanceDataBufferIn instanceData = InstanceDataBufferIn(instanceNode.propertyBuffer);
  MeshSkinningRef meshSkinning = meshGetSkinningData(geometry, mesh);

  // Compute visibility mask for a given meshlet. For render passes that may emit
  // multiple meshlets per invocation, e.g. when rendering cube maps or shadow maps,
  // each set bit corresponds to a sub-pass, and the bit index will be passed to the
  // mesh shader via the meshlet payload field.
  uint32_t visibilityMask = 0u;
  uint32_t meshletOffset = 0u;

  if (meshletIndex < meshletCount) {
    MeshletMetadata meshlet = dataBuffer.meshlets[meshletIndex];
    meshletOffset = meshlet.dataOffset;

    // Resolve bounding sphere and cone
    vec2 coneAxis = meshlet.coneAxis;
    float coneCutoff = meshlet.coneCutoff;

    TsMeshletCullingInfo meshletCullingInfo;
    meshletCullingInfo.flags = meshlet.flags;
    meshletCullingInfo.sphereCenter = meshlet.sphereCenter;
    meshletCullingInfo.sphereRadius = meshlet.sphereRadius;
    meshletCullingInfo.coneOrigin = meshlet.coneOrigin;
    meshletCullingInfo.coneAxis.xy = coneAxis;
    meshletCullingInfo.coneAxis.z = sqrt(max(0.0f, 1.0f - dot(coneAxis, coneAxis))) * sign(coneCutoff);
    meshletCullingInfo.coneCutoff = abs(coneCutoff);
    meshletCullingInfo.transform = Transform(meshInstance.transform, meshInstance.translate);

    // Apply per-instance mirror mode in local mesh space
    uint32_t mirrorMode = asGetMirrorMode(meshInstance.extra);

    if (mirrorMode != MESH_MIRROR_NONE) {
      meshletCullingInfo.sphereCenter = asMirror(meshletCullingInfo.sphereCenter, mirrorMode);
      meshletCullingInfo.coneOrigin = asMirror(meshletCullingInfo.coneOrigin, mirrorMode);
      meshletCullingInfo.coneAxis = asMirror(meshletCullingInfo.coneAxis, mirrorMode);
    }

    // Apply joint transform as necessary
    if (meshlet.jointIndex < mesh.skinJoints) {
      uint32_t jointIndex = meshSkinning.joints[meshlet.jointIndex];

      if (jointIndex < instanceData.header.jointCount) {
        Transform jointTransform = instanceLoadJoint(instanceNode.propertyBuffer, 0, jointIndex);
        meshletCullingInfo.transform = transChain(jointTransform, meshletCullingInfo.transform);
      }
    }

    // Apply node transform to world space coordinates
    meshletCullingInfo.transform = transChain(tsNodeTransformsShared[0], meshletCullingInfo.transform);

    visibilityMask = tsTestMeshletVisibility(context, meshletCullingInfo);
  }

  // Emit task shader payload
  uint32_t outputCount = bitCount(visibilityMask);
  uint32_t outputIndex = tsAllocateOutputs(outputCount);

  tsPayloadInit(context.invocation, mesh, meshInstance, uint64_t(dataBuffer));

  if (tid < 2u)
    tsPayloadSetTransform(tid, tsNodeTransformsShared[tid]);

  while (visibilityMask != 0u) {
    tsPayloadAddMeshlet(outputIndex, meshletOffset, findLSB(visibilityMask));

    outputIndex += 1u;
    visibilityMask &= visibilityMask - 1u;
  }

  return tsGetOutputCount();
}

#endif // TS_RENDER_INSTANCE_H
