// Let the backend decide on a task shader workgroup size. We
// need to declare the workgroup size early, or otherwise the
// gl_WorkGroupSize built-in will not return the correct value.
layout(local_size_x_id = SPEC_CONST_ID_TASK_SHADER_WORKGROUP_SIZE) in;

// Useful constants for some compile-time subgroup optimizations
bool IsSingleSubgroup = (gl_NumSubgroups == 1);

// Loads local mesh instance data, if provided by the mesh.
// Otherwise, returns a default instance object.
MeshInstance tsLoadMeshInstance(in GeometryRef geometry, in Mesh mesh, uint instanceId) {
  if (mesh.instanceCount == 0)
    return initMeshInstance();

  MeshInstanceRef instances = meshGetInstanceData(geometry, mesh);
  return instances.instances[instanceId];
}


// Reserves space for meshlet outputs per invocation. This must be
// called exactly once from uniform control flow. Returns the index
// of the allocated output slot within the payload. The return value
// is undefined for any invocation with a count of 0.
shared uint tsOutputIndexShared;

uint tsOutputIndexLocal = 0;

uint tsAllocateOutputs(uint count) {
  uint localIndex = 0;
  uint localCount = 0;

  if (subgroupAny(count > 1)) {
    // Having to do the double-reduction is kind of bad but
    // there's no way to broadcast data from the last lane
    localIndex = subgroupInclusiveAdd(count);
    localCount = subgroupAdd(count);
  } else {
    // When only allocating one output, take a fast path.
    // This can be a compile-time optimization if the task
    // shader only ever emits one workgroup per invocation.
    uvec4 mask = subgroupBallot(count != 0);

    localIndex = subgroupBallotBitCount(mask & gl_SubgroupLtMask);
    localCount = subgroupBallotBitCount(mask);
  }

  if (!IsSingleSubgroup) {
    // Initialize the shared counter here. This is fine since the
    // function is not intended to be called multiple times.
    if (gl_LocalInvocationIndex == 0)
      tsOutputIndexShared = 0;

    barrier();

    // Use the shared counter to compute indices for each subgroup
    uint first;

    if (subgroupElect())
      first = atomicAdd(tsOutputIndexShared, localCount);

    localIndex += subgroupBroadcastFirst(first);

    barrier();

    // Broadcast the total allocation count across the workgroup.
    tsOutputIndexLocal = tsOutputIndexShared;
  } else {
    // Adjust the local counter. This should end up in
    // a scalar register since all values are uniform.
    tsOutputIndexLocal = localCount;
  }

  return localIndex;
}


// Returns the total number of allocated outputs.
// Can be passed directly to EmitMeshTasksEXT.
uint tsGetOutputCount() {
  return tsOutputIndexLocal;
}


// Checks whether a mesh or LOD should be used based on distance.
// The view distance must be squared, but not the visilibity range.
bool tsTestDistanceRange(float distanceSquared, vec2 visibilityRange) {
  visibilityRange *= visibilityRange;

  return (distanceSquared >= visibilityRange.x)
      && (distanceSquared <  visibilityRange.y || visibilityRange.y == 0.0f);
}


// Selects the level of detail for the current mesh based on the current
// view distance. Must be called from uniform control flow. Note that the
// view distance must be uniform, and for any invalid LOD index, the
// maximum distance must be set to 0.
shared uint tsLodIndexShared;

uint tsSelectLodCooperative(in GeometryRef geometry, in Mesh mesh, float distanceSquared) {
  uint lodCount = uint(mesh.lodCount);

  // Test against the mesh visibility range, and return an invalid LOD
  // index if the mesh needs to be culled. Callers must handle this.
  vec2 meshRange = vec2(mesh.visibilityRange);

  if (!tsTestDistanceRange(distanceSquared, meshRange))
    return uint(lodCount);

  // Compute absolute buffer address of LOD definitions
  MeshLodRef lods = meshGetLodData(geometry, mesh);

  // Conservatively compute subgroup size in case we're not
  // running with full subgroups for whatever reason.
  uint invocationCount = subgroupBallotBitCount(subgroupBallot(true));

  // For the LODs themselves, we can now assume that LODs are tight and
  // ordered, i.e. that maxDistance of one LOD is equal to minDistance
  // of the next.
  uint lodIndex = 0;

  for (uint i = 0; i < lodCount; i += gl_WorkGroupSize.x) {
    uint lodId = gl_LocalInvocationIndex + i;
    bool lodTest = false;

    if (lodId < lodCount) {
      float maxDistance = float(lods.lods[lodId].maxDistance);

      lodTest = (distanceSquared < (maxDistance * maxDistance))
             || (maxDistance == 0.0f);
    }

    // Count LODs that passed within the current subgroup. Due to
    // LODs being ordered, the total number of passed tests is one
    // greater than the LOD index that we will use for rendering.
    uvec4 lodTestMask = subgroupBallot(lodTest);
    uint lodTestCount = subgroupBallotBitCount(lodTestMask);

    lodIndex += lodTestCount;

    // If any LOD test failed, we can stop since LODs will only
    // get more detailed.
    if (lodTestCount != invocationCount)
      break;
  }

  // Accumulate the number of failed LOD tests from all subgroups
  if (!IsSingleSubgroup) {
    if (gl_LocalInvocationIndex == 0)
      tsLodIndexShared = 0;

    barrier();

    if (subgroupElect() && (lodIndex != 0))
      atomicAdd(tsLodIndexShared, lodIndex);

    barrier();

    lodIndex = tsLodIndexShared;
    barrier();
  }

  if (lodIndex == 0)
    return lodCount;

  return lodIndex - 1;
}


// Tests sphere against a frustum plane. When culling vertices, just
// pass 0 as the radius. Returns true if the sphere is outside the
// half-space and should be culled.
bool tsCullSphere(vec3 center, float radius, vec4 plane) {
  return plane.w + dot(center, plane.xyz) < -radius;
}


// Tests cone visibility. Returns true if the cone faces away from
// the camera and should be culled. The cutoff is equivalent to
// cos(angle / 2).
bool tsCullCone(vec3 origin, vec3 axis, float cutoff) {
  // Input vectors may not be normalized. This is equivalent to:
  // dot(normalize(origin), normalize(axis)) > abs(cutoff)
  float squareCutoff = cutoff * cutoff;
  float squareScale  = dot(origin, origin) * dot(axis, axis);
  float squareDot    = dot(origin, axis);
        squareDot   *= abs(squareDot);

  return (squareDot > squareCutoff * squareScale);
}
