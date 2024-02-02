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

uint tsMain() {
  uint32_t tid = gl_LocalInvocationIndex;

  TsContext context = tsGetInstanceContext();

  // Visibility mask for the meshlet emitted by the current invocation.
  // Initialize to 0, since the invocation may be inactive.
  MsMeshletPayload payload = MsMeshletPayload(0u, 0u, 0u);

  if (context.invocation.isValid) {
    // Load scene header
    SceneHeader scene = SceneHeaderIn(context.sceneVa).header;
    SceneNodeTransformBufferIn nodeTransforms = SceneNodeTransformBufferIn(context.sceneVa + scene.nodeTransformOffset);

    // Load instance node
    InstanceNodeBufferIn instanceNodes = InstanceNodeBufferIn(context.instanceVa);
    InstanceNode instanceNode = instanceNodes.nodes[context.invocation.instanceIndex];

    uint32_t nodeTransformIndex = nodeComputeTransformIndices(instanceNode.nodeIndex, scene.nodeCount, context.frameId).x;
    Transform nodeTransform = nodeTransforms.nodeTransforms[nodeTransformIndex].absoluteTransform;

    // Load draw parameters
    InstanceHeader instanceInfo = InstanceDataBufferIn(instanceNode.propertyBuffer).header;
    InstanceDraw draw = instanceLoadDraw(instanceNode.propertyBuffer, context.invocation.drawIndex);

    // Load mesh properties
    GeometryRef geometry = GeometryRef(instanceInfo.geometryVa);
    Geometry geometryMetadata = geometry.geometry;

    // Locate meshlet data buffer for the selected LOD
    MeshletMetadataRef dataBuffer = MeshletMetadataRef(context.invocation.meshletBuffer);

    // Load mesh instance and skinning-related properties
    Mesh mesh = geometry.meshes[uint32_t(draw.meshIndex)];

    uint32_t meshInstanceIndex = uint32_t(draw.meshInstanceIndex) + context.invocation.localMeshInstance;
    MeshInstance meshInstance = tsLoadMeshInstance(geometry, mesh, meshInstanceIndex);

    InstanceDataBufferIn instanceData = InstanceDataBufferIn(instanceNode.propertyBuffer);
    MeshSkinningRef meshSkinning = meshGetSkinningData(geometry, mesh);

    // Compute visibility mask for a given meshlet. For render passes that
    // may emit multiple meshlets per invocation, e.g. when rendering cube
    // maps or shadow maps, each set bit corresponds to a sub-pass.
    MeshletMetadata meshlet = dataBuffer.meshlets[context.invocation.lodMeshletIndex];
    payload.offset = meshlet.dataOffset;
    payload.groups = meshlet.primitiveGroupCount;

    // Load render pass info and default to rendering all views,
    // since meshlets may have all culling options disabled.
    PassInfoBufferIn passBuffer = PassInfoBufferIn(context.passInfoVa);
    uint32_t passFlags = passBuffer.passes[context.invocation.passIndex].flags;

    uint32_t viewCount = (passFlags & RENDER_PASS_IS_CUBE_MAP_BIT) != 0u ? 6u : 1u;
    payload.viewMask = (1u << viewCount) - 1u;

    if (meshlet.flags != 0u) {
      // Compute transform from model space to view space
      Transform meshletTransform = Transform(meshInstance.transform, meshInstance.translate);

      if (meshlet.jointIndex < 0xffffu) {
        uint32_t jointIndex = meshSkinning.joints[meshInstance.jointIndex + meshlet.jointIndex];

        if (jointIndex < instanceData.header.jointCount) {
          Transform jointTransform = instanceLoadJoint(instanceNode.propertyBuffer, 0, jointIndex);
          meshletTransform = transChain(jointTransform, meshletTransform);
        }
      }

      meshletTransform = transChain(nodeTransform, meshletTransform);
      meshletTransform = transChainNorm(passBuffer.passes[context.invocation.passIndex].currTransform.transform, meshletTransform);

      // Check mirror mode, which has to be applied in mesh instance space
      uint32_t mirrorMode = asGetMirrorMode(meshInstance.extra);

      vec4 passMirrorPlane = vec4(0.0f);

      if ((passFlags & RENDER_PASS_USES_MIRROR_PLANE_BIT) != 0u)
        passMirrorPlane = passBuffer.passes[context.invocation.passIndex].currMirrorPlane;

      // Perform cone culling if that is enabled for the meshlet. Since the
      // angles don't change with a view rotation, we only need to do this once.
      vec2 coneAxis = meshlet.coneAxis;
      float coneCutoff = meshlet.coneCutoff;

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

        if ((passFlags & RENDER_PASS_USES_MIRROR_PLANE_BIT) != 0u) {
          coneOrigin = planeMirror(passMirrorPlane, coneOrigin);
          coneAxis = planeMirror(passMirrorPlane, coneAxis);
        }

        if (!testConeFacing(coneOrigin, coneAxis, coneCutoff))
          payload.viewMask = 0u;
      }

      // Cull against the mirror plane and frustum planes if necessary.
      if ((meshlet.flags & MESHLET_CULL_SPHERE_BIT) != 0u && payload.viewMask != 0u) {
        float passViewDistanceLimit = passBuffer.passes[context.invocation.passIndex].viewDistanceLimit;

        vec3 sphereCenter = vec3(meshlet.sphereCenter);
        float sphereRadius = float(meshlet.sphereRadius);
        sphereRadius *= quatGetScale(meshletTransform.rot);

        if (mirrorMode != MESH_MIRROR_NONE)
          sphereCenter = asMirror(sphereCenter, mirrorMode);

        sphereCenter = transApply(meshletTransform, sphereCenter);

        if ((passFlags & RENDER_PASS_USES_MIRROR_PLANE_BIT) != 0u) {
          // We also only need to test against the mirror plane once since
          // it is not dependent on the exact view orientation
          if (!testPlaneSphere(passMirrorPlane, sphereCenter, sphereRadius))
            payload.viewMask = 0u;

          // Apply mirroring after culling to not negate the plane test
          sphereCenter = planeMirror(passMirrorPlane, sphereCenter);
        }

        // If the render pass has a limited view range, cull meshlets against
        // it since the instance level culling isn't very accurate.
        if (passViewDistanceLimit > 0.0f) {
          float sphereRadiusSq = sphereRadius * sphereRadius;
          float maxDistanceSq = passViewDistanceLimit * passViewDistanceLimit;

          if (!testSphereDistance(sphereCenter, sphereRadiusSq, maxDistanceSq))
            payload.viewMask = 0u;
        }

        if (payload.viewMask != 0u) {
          // Test bounding sphere against the view frustum for each view. Ensure
          // that the view mask is uniform here so that memory loads and some
          // of the operation can be made more efficient.
          ViewFrustum frustum = passBuffer.passes[context.invocation.passIndex].frustum;

          for (uint32_t i = 0; i < viewCount; i++) {
            // If necessary, apply view rotation to the sphere center
            // so that we can test it against the view frustum.
            vec3 sphereCenterView = sphereCenter;

            if ((passFlags & RENDER_PASS_IS_CUBE_MAP_BIT) != 0u)
              sphereCenterView = quatApply(passBuffer.cubeFaceRotations[i], sphereCenterView);

            // Don't trust compilers to deal with local arrays here
            bool frustumTest = testPlaneSphere(frustum.planes[0], sphereCenterView, sphereRadius)
                            && testPlaneSphere(frustum.planes[1], sphereCenterView, sphereRadius)
                            && testPlaneSphere(frustum.planes[2], sphereCenterView, sphereRadius)
                            && testPlaneSphere(frustum.planes[3], sphereCenterView, sphereRadius)
                            && testPlaneSphere(frustum.planes[4], sphereCenterView, sphereRadius)
                            && testPlaneSphere(frustum.planes[5], sphereCenterView, sphereRadius);

            if (!frustumTest)
              payload.viewMask &= ~(1u << i);
          }
        }
      }
    }
  }

  // Compute total number of workgroups to emit for each meshlet
  uint32_t meshletWorkgroups = msPayloadComputeWorkgroupCount(payload);
  uint32_t outputCount = tsComputeOutputCount(meshletWorkgroups);

  // Emit task shader payload. It is important that every active
  // thread writes to its local section in the output payload.
  if (outputCount != 0u) {
    tsPayloadInit(context.invocation);
    tsPayloadAddMeshlet(tid, payload);
  }

  return outputCount;
}

#endif // TS_INSTANCE_RENDER_H
