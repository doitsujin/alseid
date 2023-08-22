#version 460

#extension GL_GOOGLE_include_directive : enable

#include "as_include_head.glsl"

#include "pass.glsl"

#define TS_MAIN tsMain

struct TsTransforms {
  Transform meshToModel;
  Transform modelToView;
};


uint tsSelectMeshLod(in Mesh mesh, in TsTransforms transforms) {
  // Compute center of bounding box with view space
  vec3 center = mix(
    vec3(geometry.metadata.aabb.lo),
    vec3(geometry.metadata.aabb.hi), 0.5f);

  center = transApply(transforms.modelToView, center);

  // Use squared distances for cooperative test
  float distanceSquared = dot(center, center);
  return tsSelectLodCooperative(push.geometryBuffer, mesh, distanceSquared);
}


bool tsCullMeshlet(in MeshInstance instance, in MeshletMetadata meshlet, in TsTransforms transforms, in MeshSkinningRef skin) {
  if ((meshlet.flags & (MESHLET_CULL_SPHERE_BIT | MESHLET_CULL_CONE_BIT)) == 0)
    return false;

  // Compute transform from mesh space to view space
  Transform transform = transforms.meshToModel;

  if (meshlet.jointIndex < geometry.metadata.jointCount) {
    Transform joint = tsLoadJoint(skin, meshlet.jointIndex + instance.jointIndex);
    transform = transChain(joint, transform);
  }

  transform = transChain(transforms.modelToView, transform);

  // Everything here currently ignores the joint index, which
  // will become important once animations are supported.
  if ((meshlet.flags & MESHLET_CULL_SPHERE_BIT) != 0) {
    vec3 sphereCenter = transApply(transform, vec3(meshlet.sphereCenter));
    float sphereRadius = float(meshlet.sphereRadius) * quatGetScale(transform.rot);

    bool cull = false;

    for (uint i = 0; i < NUM_FRUSTUM_PLANES; i++)
      cull = cull || tsCullSphere(sphereCenter, sphereRadius, scene.viewFrustum[i]);

    if (cull)
      return true;
  }

  if ((meshlet.flags & MESHLET_CULL_CONE_BIT) != 0) {
    // Reconstruct actual cone parameters
    float coneCutoff = meshlet.coneCutoff;

    vec3 coneAxis;
    coneAxis.xy = meshlet.coneAxis;
    coneAxis.z = sqrt(max(1.0f - dot(coneAxis.xy, coneAxis.xy), 0.0f)) * sign(coneCutoff);

    vec3 coneOrigin = vec3(meshlet.coneOrigin);

    // Transform cone into view space
    coneOrigin = transApply(transform, coneOrigin);
    coneAxis = quatApply(transform.rot, coneAxis);

    if (tsCullCone(coneOrigin, coneAxis, coneCutoff))
      return true;
  }

  return false;
}


shared uint bufferMaskShared;


uint tsMain() {
  Geometry metadata = geometry.metadata;

  GeometryBufferPointerRef geometryBuffers = geometryGetBuffers(
    push.geometryBuffer, metadata);

  // First buffer is always resident
  uint bufferMask = 0x1;

  if (gl_LocalInvocationIndex < metadata.bufferCount) {
    if (uint64_t(geometryBuffers.buffers[gl_LocalInvocationIndex]) != uint64_t(0))
      bufferMask |= 1u << gl_LocalInvocationIndex;
  }

  bufferMask = subgroupOr(bufferMask);

  if (!IsSingleSubgroup) {
    if (gl_LocalInvocationIndex == 0)
      bufferMaskShared = 0u;

    barrier();

    if (subgroupElect() && bufferMask != 0)
      atomicOr(bufferMaskShared, bufferMask);

    barrier();
    bufferMask = bufferMaskShared;
  }

  Mesh mesh = geometry.meshes[push.meshIndex];

  MeshInstance instance = tsLoadMeshInstance(
    push.geometryBuffer, mesh, gl_WorkGroupID.y);

  TsTransforms transforms;

  // Compute mesh space to model space transform
  transforms.meshToModel = Transform(
    vec4(instance.transform),
    vec3(instance.translate));

  // Compute model space to view space transform
  transforms.modelToView = transChain(
    scene.viewTransform, model.modelTransform);

  MeshLodRef lods = meshGetLodData(push.geometryBuffer, mesh);

  // Select LOD based on view distance, and exit early
  // if no LOD is suitable.
  uint lodIndex = tsSelectMeshLod(mesh, transforms);

  if (lodIndex >= mesh.lodCount)
    return 0;

  // Load selected LOD and extract the meshlet range.
  MeshLod lod = meshGetLodData(push.geometryBuffer, mesh).lods[lodIndex];

  MeshletMetadataRef dataBuffer;

  if (lod.bufferIndex == 0)
    dataBuffer = geometryGetEmbeddedBuffer(push.geometryBuffer, metadata);
  else
    dataBuffer = geometryBuffers.buffers[lod.bufferIndex - 1];

  if (uint64_t(dataBuffer) == uint64_t(0u))
    return 0;

  uint localIndex = gl_GlobalInvocationID.x;
  uint meshletIndex = lod.meshletIndex + localIndex;
  uint meshletCount = lod.meshletCount;

  // If this workgroup does not contribute any meshlets, exit early.
  if (gl_WorkGroupSize.x * gl_WorkGroupID.x >= meshletCount)
    return 0;

  // Cull individual meshlets against the view frustum
  MeshSkinningRef skinningData = meshGetSkinningData(push.geometryBuffer, mesh);
  MeshletMetadata meshlet = dataBuffer.meshlets[meshletIndex];

  bool cullMeshlet = true;
  if (localIndex < meshletCount)
    cullMeshlet = tsCullMeshlet(instance, meshlet, transforms, skinningData);

  // For each visible meshlet, write the absolute address to the payload
  uint outputIndex = tsAllocateOutputs(cullMeshlet ? 0 : 1);

  if (!cullMeshlet) {
    payload.meshlets[outputIndex] = uint64_t(meshletGetHeader(dataBuffer, meshlet));
    payload.meshletIndices[outputIndex] = meshletIndex;
  }

  if (gl_LocalInvocationIndex == 0) {
    payload.skinningData = uint64_t(skinningData);
    payload.instance = instance;
    payload.modelViewTransform = transforms.modelToView;
  }

  return tsGetOutputCount();
}

#include "as_include_tail.glsl"
