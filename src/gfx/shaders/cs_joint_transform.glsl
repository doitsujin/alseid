// Joint transform compute shader.
//
// Given a set of joint metadata and relative transforms, this computes
// absolute transforms that can be used directly as input for rendering.
//
//
// The interface is defined as follows:
//
// Context object that is passed through to all functions. This should
// store information required to fetch data from the correct places,
// e.g. a buffer reference to the geometry's joint metadata.
//
//    struct CsContext {
//      GeometryRef geometry;           /* mandatory */
//      ...
//    };
//
//
// Initializes context object.
//
//    CsContext csGetContext();
//
//
// Loads the relative transform of the joint with the given index
// from the input buffer.
//
//    Transform csLoadInputJoint(in CsContext context, uint index);
//
//
// Loads the final transform of the joint with the given index from the
// output buffer. Outputs are cached internally in shared memory, and
// this will only be called if the given joint is not cached.
// Note that the output buffer MUST be declared as workgroupcoherent.
//
//    Transform csLoadOutputJoint(in CsContext context, uint index);
//
//
// Stores final transform of the joint with the given index. The output
// buffer may be the same as the input buffer.
//
//    void csStoreOutputJoint(in CsContext context, uint index, in Transform transform);
//
//
// Stores axis-aligned bounding box of the instance in model space. This
// is computed by combining the static bounding box of the geometry with
// the transformed model space joint bounding volumes. Will only be called
// by the first thread in the workgroup.
//
//    void csStoreBoundingVolume(in CsContext context, in Aabb aabb);


// One workgroup per instance. We generally do not expect a large
// degree of parallelism here since joints are often deeply nested.
layout(local_size_x_id = SPEC_CONST_ID_MIN_SUBGROUP_SIZE) in;

layout(constant_id = SPEC_CONST_ID_MIN_SUBGROUP_SIZE) const uint CsJointCacheSize = 128;

// Useful constants for some compile-time subgroup optimizations
bool IsSingleSubgroup = (gl_NumSubgroups == 1);


// Helper function to compute the axis-aligned bounding box for
// the geometry instance by reducing the coordinate vectors.
Aabb csAabbShared;

Aabb csAccumulateAabb(Aabb aabb) {
  aabb.lo = subgroupMin(aabb.lo);
  aabb.hi = subgroupMax(aabb.hi);

  if (!IsSingleSubgroup) {
    // We shouldn't ever hit this code path in practice, but drivers
    // may be weird and not launch the shader with full subgroups.
    if (gl_SubgroupID == 0) {
      if (subgroupElect())
        csAabbShared = aabb;
    }

    barrier();

    // Ignore performance and just do something that works without
    // requiring weird features like atomic floats.
    for (uint i = 1; i < gl_NumSubgroups; i++) {
      if (gl_SubgroupID == i) {
        if (subgroupElect()) {
          csAabbShared.lo = min(aabb.lo, csAabbShared.lo);
          csAabbShared.hi = max(aabb.hi, csAabbShared.hi);
        }
      }

      barrier();
    }

    aabb = csAabbShared;
  }

  return aabb;
}


// Computes transform for a single joint. This is equivalent to
// translating a vertex into joint space, applying the relative
// transform, and then translating back into model space.
// Do not assume normalization here. This is slower, but this
// shader is not expected to be ALU bound in the first place.
Transform csComputeJointTransform(in Transform jointTransform, vec3 jointPosition) {
  Transform localTransform;
  localTransform.rot = jointTransform.rot;
  localTransform.pos = jointTransform.pos + quatApply(jointTransform.rot, -jointPosition) + jointPosition;
  return localTransform;
}


// Computes the absolute transform for a joint by chaining
// it with the absolute transform of its parent joint.
Transform csComputeAbsoluteTransform(in Transform parentTransform, in Transform jointTransform) {
  return transChain(parentTransform, jointTransform);
}


// Computes absolute transforms for the jointCount joints defined in
// the joints buffer, using relative transforms from the input buffer,
// and writes the final transforms to the result buffer. Must be called
// from uniform control flow within the workgroup.
shared Transform csTransformJointOutputCache[CsJointCacheSize];

shared uint csTransformJointCountShared;

void csTransformJoints(in CsContext context) {
  Geometry geometry = context.geometry.geometry;
  JointRef jointBuffer = geometryGetJointData(context.geometry, geometry);

  Aabb aabb = Aabb(
    vec3(geometry.aabb.lo),
    vec3(geometry.aabb.hi));

  uint firstJoint = 0;
  uint firstCachedOutput = 0;

  while (firstJoint < geometry.jointCount) {
    Joint joint;

    // Check whether the current joint index is valid, and if it is,
    // whether the joint has a dependency on the current iteration.
    uint jointIndex = firstJoint + gl_LocalInvocationIndex;
    bool canCompute = jointIndex < geometry.jointCount;

    if (canCompute) {
      joint = jointBuffer.joints[jointIndex];

      canCompute = (joint.parent >= geometry.jointCount)
                || (joint.parent <  firstJoint);
    }

    // Count joints to process in this iteration
    uint count = subgroupBallotBitCount(subgroupBallot(canCompute));

    if (!IsSingleSubgroup) {
      if (gl_LocalInvocationIndex == 0)
        csTransformJointCountShared = 0;

      barrier();

      if (subgroupElect())
        atomicAdd(csTransformJointCountShared, count);

      barrier();

      // Don't need a barrier after the read since
      // we'll be doing another one later anyway
      count = csTransformJointCountShared;
    }

    Transform jointTransform;

    if (canCompute) {
      // Load relative transform from input buffer.
      Transform relativeTransform = csLoadInputJoint(context, jointIndex);

      // Compute transform for the current joint.
      jointTransform = csComputeJointTransform(relativeTransform, joint.position);

      // If necessary, load parent transform from output buffer and use
      // it to compute the absolute transform for the current joint.
      if (joint.parent < firstJoint) {
        Transform parentTransform;

        if (joint.parent < firstCachedOutput)
          parentTransform = csLoadOutputJoint(context, joint.parent);
        else
          parentTransform = csTransformJointOutputCache[joint.parent - firstCachedOutput];

        jointTransform = csComputeAbsoluteTransform(parentTransform, jointTransform);
      }

      // Adjust the bounding box as necessary.
      vec3 absolutePosition = joint.position + jointTransform.pos;

      float radius = quatGetScale(jointTransform.rot) * float(joint.radius)
                   + length(relativeTransform.pos);

      aabb.lo = min(aabb.lo, absolutePosition - radius);
      aabb.hi = max(aabb.hi, absolutePosition + radius);
    }

    if (firstJoint + count < geometry.jointCount) {
      // Insert barrier between potentially reading from LDS and writing
      // to it again in order to prevent a write-after-read hazard here.
      barrier();

      if (canCompute) {
        // Cache output transform in LDS so that the next iteration can access
        // the data without having to read memory. We should only hit the memory
        // path if the number of joints we can compute in parallel is larger
        // than the workgroup size, which should be extremely rare.
        csTransformJointOutputCache[gl_LocalInvocationIndex] = jointTransform;
      }
    }

    // Store final result in output buffer.
    if (canCompute)
      csStoreOutputJoint(context, jointIndex, jointTransform);

    // If there are any joints left to process, make sure writes
    // to the output buffer are visible to the entire workgroup.
    if (firstJoint + count < geometry.jointCount) {
      controlBarrier(gl_ScopeWorkgroup, gl_ScopeWorkgroup,
        gl_StorageSemanticsShared | gl_StorageSemanticsBuffer,
        gl_SemanticsAcquireRelease);
    }

    // Advance to next iteration
    firstCachedOutput = firstJoint;
    firstJoint += count;
  }

  // Broadcast AABB to whole workgroup
  aabb = csAccumulateAabb(aabb);

  if (gl_LocalInvocationIndex == 0)
    csStoreBoundingVolume(context, aabb);
}


// Entry point for this compute shader
void csTransformJointsMain() {
  CsContext context = csGetContext();
  csTransformJoints(context);
}
