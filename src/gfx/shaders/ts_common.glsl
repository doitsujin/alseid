#ifndef TS_COMMON_H
#define TS_COMMON_H

// Let the backend decide on a task shader workgroup size. We
// need to declare the workgroup size early, or otherwise the
// gl_WorkGroupSize built-in will not return the correct value.
layout(local_size_x_id = SPEC_CONST_ID_TASK_SHADER_WORKGROUP_SIZE) in;


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
    localIndex = subgroupExclusiveAdd(count);
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

#endif // TS_COMMON_H
