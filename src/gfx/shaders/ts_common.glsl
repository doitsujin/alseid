#ifndef TS_COMMON_H
#define TS_COMMON_H

// Let the backend decide on a task shader workgroup size. We
// need to declare the workgroup size early, or otherwise the
// gl_WorkGroupSize built-in will not return the correct value.
layout(local_size_x_id = SPEC_CONST_ID_TASK_SHADER_WORKGROUP_SIZE) in;


// Computes the number of workgroups to spawn based
// on a thread-local, per-meshlet workgroup count.
shared uint32_t tsOutputCountShared;

uint32_t tsComputeOutputCount(uint32_t localCount) {
  uint32_t result = subgroupAdd(localCount);

  if (!IsSingleSubgroup) {
    if (gl_LocalInvocationIndex == 0u)
      tsOutputCountShared = 0u;

    barrier();

    if (subgroupElect())
      atomicAdd(tsOutputCountShared, result);

    barrier();

    result = tsOutputCountShared;
  }

  return result;
}

#endif // TS_COMMON_H
