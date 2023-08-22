#define NUM_FRUSTUM_PLANES (6)

// Scene constant buffer
layout(set = 0, binding = 0, scalar)
uniform Scene {
  Projection  projection;
  Transform   viewTransform;
  float       dummy;
  vec4        viewFrustum[NUM_FRUSTUM_PLANES];
} scene;


#ifdef STAGE_TASK
// Uniform buffer with geometry information
layout(set = 1, binding = 0, scalar)
uniform GeometryBuffer {
  Geometry    metadata;
  Mesh        meshes[256];
} geometry;
#endif


// Per-instance data buffer
layout(set = 2, binding = 0, scalar)
uniform Instance {
  Transform   modelTransform;
} model;

// Per-instance joint transforms
#ifndef STAGE_FRAG
struct JointTransform {
  vec4 rot;
  vec4 pos;
};

layout(set = 2, binding = 1, scalar)
readonly buffer Joints {
  JointTransform  joints[];
};

// Morph target weights
layout(set = 2, binding = 2, scalar)
readonly buffer Weights {
  float  weights[];
};
#endif

// Push constant data - provides a
// pointer to the geometry buffer
layout(push_constant)
uniform PushData {
  GeometryRef geometryBuffer;
  uint32_t    meshIndex;
} push;


// Interface between task and mesh shader
#ifndef STAGE_FRAG

struct TaskPayload {
  uint64_t skinningData;
  MeshInstance instance;
  Transform modelViewTransform;
  uint64_t meshlets[MAX_TASK_SHADER_WORKGROUP_SIZE];
  uint meshletIndices[MAX_TASK_SHADER_WORKGROUP_SIZE];
};

taskPayloadSharedEXT TaskPayload payload;

struct MsContext {
  uint64_t        meshlet;
  uint            cullMode;
  bool            faceFlip;
  uint64_t        skinData;
  uint            skinOffset;
};


Transform msLoadJointTransform(in MsContext context, uint index) {
  return Transform(
    vec4(joints[index].rot),
    vec3(joints[index].pos));
}


Transform tsLoadJoint(in MeshSkinningRef skin, uint index) {
  if (uint64_t(skin) == uint64_t(0))
    index = 0xffff;

  if (index != 0xffff)
    index = skin.joints[index];

  if (index != 0xffff) {
    return Transform(
      vec4(joints[index].rot),
      vec3(joints[index].pos));
  } else {
    return Transform(
      vec4(0.0f, 0.0f, 0.0f, 1.0f),
      vec3(0.0f, 0.0f, 0.0f));
  }
}

#endif


#ifdef STAGE_MESH

MsContext msGetInstanceContext() {
  MsContext context;
  context.meshlet = payload.meshlets[gl_WorkGroupID.x];
  context.cullMode = FACE_CULL_MODE_CW;
  context.faceFlip = false;
  context.skinData = payload.skinningData;
  context.skinOffset = payload.instance.jointIndex;
  return context;
}

#endif
