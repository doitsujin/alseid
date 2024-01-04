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
//      uint64_t          passInfoVa;   /* mandatory; stores pointer to pass infos */
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
#ifndef TS_INSTANCE_RENDER_H
#define TS_INSTANCE_RENDER_H

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

    transformIndex = nodeComputeTransformIndices(nodeIndex, scene.nodeCount, invocationFrameId).x;
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
  uint32_t viewMask = 0u;
  uint32_t meshletOffset = 0u;

  if (meshletIndex < meshletCount) {
    MeshletMetadata meshlet = dataBuffer.meshlets[meshletIndex];
    meshletOffset = meshlet.dataOffset;

    // Load render pass info and default to rendering each view,
    // since meshlets may have all culling options disabled.
    PassInfoBufferIn passBuffer = PassInfoBufferIn(context.passInfoVa);
    PassInfo pass = passBuffer.passes[context.invocation.passIndex];

    uint32_t viewCount = (pass.flags & PASS_IS_CUBE_MAP_BIT) != 0u ? 6u : 1u;
    viewMask = (1u << viewCount) - 1u;

    // Resolve bounding sphere and cone
    vec2 coneAxis = meshlet.coneAxis;
    float coneCutoff = meshlet.coneCutoff;

    if (meshlet.flags != 0u) {
      Transform meshletTransform = Transform(meshInstance.transform, meshInstance.translate);

      // Apply joint transform as necessary
      if (meshlet.jointIndex < mesh.skinJoints) {
        uint32_t jointIndex = meshSkinning.joints[meshlet.jointIndex];

        if (jointIndex < instanceData.header.jointCount) {
          Transform jointTransform = instanceLoadJoint(instanceNode.propertyBuffer, 0, jointIndex);
          meshletTransform = transChain(jointTransform, meshletTransform);
        }
      }

      // Apply transforms from model to view space
      meshletTransform = transChain(tsNodeTransformsShared[0], meshletTransform);
      meshletTransform = transChainNorm(pass.currTransform.transform, meshletTransform);

      // Check mirror mode, which has to be applied in mesh instance space
      uint32_t mirrorMode = asGetMirrorMode(meshInstance.extra);

      // Perform cone culling if that is enabled for the meshlet. Since the
      // angles don't change with a view rotation, we only need to do this once.
      if ((meshlet.flags & MESHLET_CULL_CONE_BIT) != 0u) {
        float coneCutoff = float(meshlet.coneCutoff);
        vec3 coneOrigin = vec3(meshlet.coneOrigin);
        vec2 coneAxis2D = vec2(meshlet.coneAxis);
        vec3 coneAxis = vec3(coneAxis2D, sqrt(max(0.0f, 1.0f - dot(coneAxis2D, coneAxis2D))) * sign(coneCutoff));
        coneCutoff = abs(coneCutoff);

        if (mirrorMode != MESH_MIRROR_NONE) {
          coneOrigin = asMirror(coneOrigin, mirrorMode);
          coneAxis = asMirror(coneAxis, mirrorMode);
        }

        coneOrigin = transApply(meshletTransform, coneOrigin);
        coneAxis = quatApply(meshletTransform.rot, coneAxis);

        if ((pass.flags & PASS_USES_MIRROR_PLANE_BIT) != 0u) {
          coneOrigin = planeMirror(pass.currMirrorPlane, coneOrigin);
          coneAxis = planeMirror(pass.currMirrorPlane, coneAxis);
        }

        if (!testConeFacing(coneOrigin, coneAxis, coneCutoff))
          viewMask = 0u;
      }

      // Cull against the mirror plane and frustum planes if necessary.
      if ((meshlet.flags & MESHLET_CULL_SPHERE_BIT) != 0u && viewMask != 0u) {
        vec3 sphereCenter = vec3(meshlet.sphereCenter);
        float sphereRadius = float(meshlet.sphereRadius);
        sphereRadius *= quatGetScale(meshletTransform.rot);

        if (mirrorMode != MESH_MIRROR_NONE)
          sphereCenter = asMirror(sphereCenter, mirrorMode);

        sphereCenter = transApply(meshletTransform, sphereCenter);

        if ((pass.flags & PASS_USES_MIRROR_PLANE_BIT) != 0u) {
          // We also only need to test against the mirror plane once since
          // it is not dependent on the exact view orientation
          if (!testPlaneSphere(pass.currMirrorPlane, sphereCenter, sphereRadius))
            viewMask = 0u;

          // Apply mirroring after culling to not negate the plane test
          sphereCenter = planeMirror(pass.currMirrorPlane, sphereCenter);
        }

        // If the render pass has a limited view range, cull meshlets against
        // it since the instance level culling isn't very accurate.
        if (pass.viewDistanceLimit > 0.0f) {
          float sphereRadiusSq = sphereRadius * sphereRadius;
          float maxDistanceSq = pass.viewDistanceLimit * pass.viewDistanceLimit;

          if (!testSphereDistance(sphereCenter, sphereRadiusSq, maxDistanceSq))
            viewMask = 0u;
        }

        if (viewMask != 0u) {
          // Test bounding sphere against the view frustum for each view. Ensure
          // that the view mask is uniform here so that memory loads and some
          // of the operation can be made more efficient.
          for (uint32_t i = 0; i < viewCount; i++) {
            // If necessary, apply view rotation to the sphere center
            // so that we can test it against the view frustum.
            vec3 sphereCenterView = sphereCenter;

            if ((pass.flags & PASS_IS_CUBE_MAP_BIT) != 0u)
              sphereCenterView = quatApply(passBuffer.cubeFaceRotations[i], sphereCenterView);

            // Don't trust compilers to deal with local arrays here
            bool frustumTest = testPlaneSphere(pass.frustum.planes[0], sphereCenterView, sphereRadius)
                            && testPlaneSphere(pass.frustum.planes[1], sphereCenterView, sphereRadius)
                            && testPlaneSphere(pass.frustum.planes[2], sphereCenterView, sphereRadius)
                            && testPlaneSphere(pass.frustum.planes[3], sphereCenterView, sphereRadius)
                            && testPlaneSphere(pass.frustum.planes[4], sphereCenterView, sphereRadius)
                            && testPlaneSphere(pass.frustum.planes[5], sphereCenterView, sphereRadius);

            if (!frustumTest)
              viewMask &= ~(1u << i);
          }
        }
      }
    }
  }

  // Emit task shader payload
  uint32_t outputCount = bitCount(viewMask);
  uint32_t outputIndex = tsAllocateOutputs(outputCount);

  tsPayloadInit(context.invocation, mesh, meshInstance, uint64_t(dataBuffer));

  if (tid < 2u)
    tsPayloadSetTransform(tid, tsNodeTransformsShared[tid]);

  tsPayloadAddMeshlet(tid, meshletOffset, outputIndex, viewMask);
  return tsGetOutputCount();
}

#endif // TS_INSTANCE_RENDER_H
