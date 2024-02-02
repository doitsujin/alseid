#ifndef AS_GEOMETRY_H
#define AS_GEOMETRY_H

#include "./scene/animation/as_animation.glsl"

// Axis-aligned bounding box
struct Aabb {
  f32vec3   lo;  // Vertex with minimum component values
  f32vec3   hi;  // Vertex with maximum component values
};


// 16-bit Axis-aligned bounding box
struct Aabb16 {
  f16vec3   lo;  // Vertex with minimum component values
  f16vec3   hi;  // Vertex with maximum component values
};


// Computes the central point of an axis-aligned bounding box.
vec3 aabbComputeCenter(in Aabb aabb) {
  return mix(aabb.lo, aabb.hi, 0.5f);
}


// Computes position of a given vertex of an axis-aligned bounding box
// Vertices are returned in no particular order, but only the least
// significant three bits of the vertex index are considered.
vec3 aabbComputeVertex(in Aabb aabb, uint vertex) {
  return mix(aabb.lo, aabb.hi,
    notEqual(uvec3(vertex) & uvec3(0x1, 0x2, 0x4), uvec3(0u)));
}


// Geometry metadata
//
// Stores culling information for the asset as a whole, as well as
// the overall buffer layout, with offsets being relative to the
// start of the geometry metadata structure itself.
struct Geometry {
  Aabb16    aabb;
  uint8_t   bufferCount;
  uint8_t   materialCount;
  uint16_t  meshCount;
  uint16_t  jointCount;
  uint16_t  morphTargetCount;
  uint32_t  bufferPointerOffset;
  uint32_t  jointDataOffset;
  uint32_t  meshletDataOffset;
  uint32_t  animationDataOffset;
  uint32_t  reserved;
};


// Mesh metadata
//
// Describes the mesh within the geometric asset being rendered.
// Offsets are relative to the start of the geometry metadata
// structure, not the mesh metadata.
struct Mesh {
  uint8_t   lodCount;
  uint8_t   materialIndex;
  uint16_t  maxMeshletCount;
  uint16_t  instanceCount;
  uint16_t  skinJoints;
  uint32_t  instanceDataOffset;
  uint32_t  lodInfoOffset;
  uint32_t  skinDataOffset;
  f16vec2   visibilityRange;
};


// Buffer reference type for geometry data
layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer GeometryRef {
  Geometry  geometry;
  Mesh      meshes[];
};


// Mesh instance data
//
// Stores a transform quaternion, a translation vector,
// as well as joint and morph target weight offsets for
// animation purposes.
struct MeshInstance {
  f32vec4   transform;
  f32vec3   translate;
  uint16_t  jointIndex;
  uint16_t  extra;
};


MeshInstance initMeshInstance() {
  MeshInstance result;
  result.transform = f32vec4(0.0f.xxx, 1.0f);
  result.translate = f32vec3(0.0f);
  result.jointIndex = uint16_t(0);
  result.extra = uint16_t(0);
  return result;
}


// Buffer reference type for mesh instance data
layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer MeshInstanceRef {
  MeshInstance instances[];
};


MeshInstanceRef meshGetInstanceData(in GeometryRef geometry, in Mesh mesh) {
  uint64_t address = uint64_t(geometry) + mesh.instanceDataOffset;
  return MeshInstanceRef(address);
}


// Mirror modes for mesh instances
#define MESH_MIRROR_NONE  (0x0)
#define MESH_MIRROR_X     (0x1)
#define MESH_MIRROR_Y     (0x2)
#define MESH_MIRROR_Z     (0x3)

// Extracts mirroring info from mesh
// instance extra data field
uint asGetMirrorMode(uint extra) {
  return extra & 0x3;
}


// Helper function to mirror a vector
// based on the mirroring mode
vec3 asMirror(vec3 vector, uint mode) {
  return mix(vector, -vector, bvec3(
    mode == MESH_MIRROR_X,
    mode == MESH_MIRROR_Y,
    mode == MESH_MIRROR_Z));
}


// Helper function to check whether primitive flipping
// needs to be enabled based on two mirroring modes.
// Returns true if only one mirror mode is not NONE.
bool asNeedsFaceFlip(uint mirrorA, uint mirrorB) {
  return (mirrorA == MESH_MIRROR_NONE)
      != (mirrorB == MESH_MIRROR_NONE);
}


// Buffer reference type for skinning data
layout(buffer_reference, buffer_reference_align = 4, scalar)
readonly buffer MeshSkinningRef {
  uint16_t joints[];
};


MeshSkinningRef meshGetSkinningData(in GeometryRef geometry, in Mesh mesh) {
  if (mesh.skinDataOffset == 0)
    return MeshSkinningRef(uint64_t(0));

  uint64_t address = uint64_t(geometry) + mesh.skinDataOffset;
  return MeshSkinningRef(address);
}


// Mesh LOD description
//
// Stores data relevant for LOD selection, as well as the
// meshlet and vertex data ranges relevant for rendering.
struct MeshLod {
  uint16_t  bufferIndex;
  float16_t maxDistance;
  uint16_t  meshletIndex;
  uint16_t  meshletCount;
};


// Buffer reference type for LOD data
layout(buffer_reference, buffer_reference_align = 8, scalar)
readonly buffer MeshLodRef {
  MeshLod   lods[];
};


MeshLodRef meshGetLodData(in GeometryRef geometry, in Mesh mesh) {
  uint64_t address = uint64_t(geometry) + mesh.lodInfoOffset;
  return MeshLodRef(address);
}


// Meshlet culling info
//
// Stores meshlet metadata that is useful for the task shader,
// or for compute shaders performing meshlet-level culling.
struct MeshletMetadata {
  uint16_t  flags;
  f16vec3   sphereCenter;
  float16_t sphereRadius;
  f16vec3   coneOrigin;
  f16vec2   coneAxis;
  float16_t coneCutoff;
  uint16_t  jointIndex;
  uint32_t  dataOffset;
  uint32_t  primitiveGroupCount;
};

#define MESHLET_CULL_SPHERE_BIT         (1u << 0)
#define MESHLET_CULL_CONE_BIT           (1u << 1)


// Buffer reference type for meshlet metadata
layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer MeshletMetadataRef {
  MeshletMetadata meshlets[];
};


// Meshlet primitive group
struct MeshletPrimitiveGroup {
  uint8_t vertexCount;
  uint8_t primitiveCount;
};


// Meshlet metadata
//
// Stores vertex data as well as morph target data
// relevant for rendering the meshlet in question.
#define MESHLET_LOCAL_JOINT_COUNT         (16u)
#define MESHLET_LOCAL_MORPH_TARGET_COUNT  (16u)
#define MESHLET_GROUP_SIZE                (32u)

struct Meshlet {
  uint16_t  groupVertexOffset;
  uint16_t  groupPrimitiveOffset;
  uint16_t  vertexDataOffset;
  uint16_t  shadingDataOffset;
  uint8_t   vertexDataCount;
  uint8_t   shadingDataCount;
  uint8_t   jointCount;
  uint8_t   jointCountPerVertex;
  uint16_t  jointDataOffset;
  uint16_t  jointIndex;
  uint16_t  morphTargetCount;
  uint16_t  morphTargetOffset;
  uint16_t  morphDataOffset;
  uint16_t  skinIndexOffset;
};


// Buffer reference type for meshlet data, immediately followed by a
// tightly packed array of group properties. The group count itself
// must be passed to the mesh shader through other means.
layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer MeshletRef {
  Meshlet               header;
  MeshletPrimitiveGroup groups[];
};

MeshletRef meshletGetHeader(
  in    MeshletMetadataRef            dataBuffer,
  in    MeshletMetadata               meshlet) {
  uint64_t address = uint64_t(dataBuffer) + meshlet.dataOffset;
  return MeshletRef(address);
}

uint64_t meshletComputeAddress(uint64_t baseAddress, uint16_t offset) {
  // Offsets within a meshlet are stored as multiples of 16 bytes
  return baseAddress + (uint32_t(offset) * 16u);
}


// Joint influence structure
struct JointInfluence {
  uint32_t  index;
  float     weight;
};

JointInfluence meshletDecodeJointInfluence(uint32_t data) {
  return JointInfluence(bitfieldExtract(data, 12, 4),
    float(bitfieldExtract(data, 0, 12)) / 4095.0f);
}


// Buffer reference type for primitive data.
layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer MeshletPrimitiveDataRef {
  uint16_t primitives[];
};


// Helper function to decode primitive data
u8vec3 meshletDecodePrimitive(uint32_t prim) {
  return u8vec3(
    bitfieldExtract(prim,  0, 5),
    bitfieldExtract(prim,  5, 5),
    bitfieldExtract(prim, 10, 5));
}


// Buffer reference type for vertex indices.
layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer MeshletVertexIndexRef {
  u8vec2 vertices[];
};


// Buffer reference type for skin indices.
layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer MeshletSkinIndexRef {
  uint16_t indices[];
};


// Per-vertex morph target metadata
struct MeshletMorphTargetVertexInfo {
  uint16_t morphTargetMask;
  uint16_t morphDataIndex;
};

layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer MeshletMorphTargetVertexInfoRef {
  MeshletMorphTargetVertexInfo vertexInfos[];
};


// Joint metadata
struct Joint {
  f32vec3   position;
  float16_t radius;
  uint16_t  parent;
};


// Buffer reference type for joints
layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer JointRef {
  Joint joints[];
};

JointRef geometryGetJointData(in GeometryRef geometryAddress, in Geometry geometry) {
  uint64_t address = uint64_t(geometryAddress) + geometry.jointDataOffset;
  return JointRef(address);
}


// Data buffer pointers stored in the geometry buffer. Data buffers
// will always have 
layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer GeometryBufferPointerRef {
  MeshletMetadataRef buffers[];
};


GeometryBufferPointerRef geometryGetBuffers(in GeometryRef geometryAddress, in Geometry geometry) {
  uint64_t address = uint64_t(geometryAddress) + geometry.bufferPointerOffset;
  return GeometryBufferPointerRef(address);
}


MeshletMetadataRef geometryGetEmbeddedBuffer(in GeometryRef geometryAddress, in Geometry geometry) {
  uint64_t address = uint64_t(geometryAddress) + geometry.meshletDataOffset;
  return MeshletMetadataRef(address);
}


AnimationBufferRef geometryGetAnimationBuffer(in GeometryRef geometryAddress, in Geometry geometry) {
  uint64_t address = uint64_t(geometryAddress) + geometry.animationDataOffset;
  return AnimationBufferRef(address);
}

#endif // AS_GEOMETRY_H
