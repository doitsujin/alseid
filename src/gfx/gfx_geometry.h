#pragma once

#include <list>
#include <vector>

#include "../util/util_small_vector.h"
#include "../util/util_stream.h"

#include "gfx_buffer.h"
#include "gfx_context.h"
#include "gfx_device.h"
#include "gfx_format.h"
#include "gfx_pipeline.h"
#include "gfx_ray_tracing.h"

namespace as {

/**
 * \brief Meshlet culling flags
 */
enum class GfxMeshletCullFlag : uint16_t {
  /** Frustum culling using a bounding sphere. */
  eCullSphere   = (1u << 0),
  /** Meshlet-level backface culling using a cone. */
  eCullCone     = (1u << 1),

  eFlagEnum     = 0
};

using GfxMeshletCullFlags = Flags<GfxMeshletCullFlag>;


/**
 * \brief Meshlet property flags
 */
enum class GfxMeshletFlag : uint16_t {
  /** Meshlet uses dual index buffers to fetch vertex
   *  data, one set for vertex position data and one
   *  for shading data. Does not affect morph targets. */
  eDualIndex        = (1u << 0),

  eFlagEnum         = 0
};

using GfxMeshletFlags = Flags<GfxMeshletFlag>;


/**
 * \brief Primitive data
 *
 * Stores three packed 8-bit indices
 * for a single triangle.
 */
using GfxMeshletPrimitive = Vector<uint8_t, 3>;

static_assert(sizeof(GfxMeshletPrimitive) == 3);


/**
 * \brief Dual index buffer data
 *
 * Stores two packed 8-bit indices for each output vertex. Dual
 * indexing can be used in order to reduce the amount of redundant
 * stored data within a meshlet.
 *
 * As an example, a cube with flat normals typically requires 24
 * vertices in total, with dual indexing this can be reduced to
 * 8 instances of positional data and 6 instances of shading data.
 *
 * Additionally, for render passes that do not require any shading
 * data, such as shadow rendering, this may significantly reduce
 * the number of vertices emitted by the mesh shader since only
 * positional data would be required for such passes.
 *
 * In some instances, dual indexing may require texture coordinates
 * to be stored alongside positional data in order to be effective.
 * This is especially true for alpha-tested geometry. This should
 * not be an issue if vertex data is laid out well, but may lead to
 * increased LDS pressure in the mesh shader.
 *
 * In order to use meshlet-based geometry for ray tracing, a temporary
 * index buffer must be generated on the fly since there is no way to
 * handle dual-indexing or 8-bit indices for BVH creation.
 */
struct GfxMeshletDualIndex {
  /** Index into the vertex data buffer, which should
   *  be used to compute vertex positions. */
  uint8_t vertexDataIndex;
  /** Index into the shading data buffer, which should
   *  be used to compute fragment shader inputs. */
  uint8_t shadingDataIndex;
};


/**
 * \brief Meshlet info
 *
 * Stores culling properties of the given meshlet as well as a
 * pointer to the actual meshlet data. This data is supposed to
 * be read by the task shader during rendering, and is laid out
 * tightly within the buffer in order to improve data locality.
 */
struct GfxMeshletInfo {
  /** Meshlet culling flags */
  GfxMeshletCullFlags flags;
  /** Bounding sphere center */
  Vector<float16_t, 3> sphereCenter;
  /** Bounding sphere radius */
  float16_t sphereRadius;
  /** Cone apex, in model space */
  Vector<float16_t, 3> coneOrigin;
  /** Cone axis. Z coordinate should be inferred,
   *  with the sign taken from the cone cutoff. */
  Vector<float16_t, 2> coneAxis;
  /** Cone cutoff (cos(angle/2)). If negative,
   *  the z coordinate of the axis is negative. */
  float16_t coneCutoff;
  /** Joint index. If the meshlet is largely only
   *  influenced by one joint, this can be used to
   *  perform culling within that joint's space. */
  uint16_t jointIndex;
  /** Meshlet data offset relative to the start
   *  of the buffer, always aligned to 16 bytes. */
  uint32_t dataOffset;
  /** Reserved for future use */
  uint32_t reserved;
};

static_assert(sizeof(GfxMeshletInfo) == 32);


/**
 * \brief Meshlet header
 *
 * Stores meshlet properties for rendering. This data is supposed to be
 * read by the mesh shader, but should not be needed by the task shader.
 *
 * Meshlet data immediately follows the header within the buffer to improve
 * data locality. All offsets are encoded as multiples of 16 bytes relative
 * to the header, which allows for a theoretical maximum of 1 MiB of data
 * per meshlet.
 */
struct GfxMeshletHeader {
  /** Meshlet property flags. */
  GfxMeshletFlags flags;
  /** Total number of vertices in the meshlet. Should generally not
   *  exceed 128. If dual index buffers are enabled, this is equal
   *  the number of index pairs in the buffer. */
  uint8_t vertexCount;
  /** Number of triangles in the meshlet. Should generally not exceed
   *  128. Equal to the number of primitive entries in the buffer. */
  uint8_t primitiveCount;
  /** Number of vertex data instances. If dual index buffers are
   *  enabled, this may be lower than the total vertex count, and
   *  the first index in the index pair can be used to access this
   *  data. */
  uint8_t vertexDataCount;
  /** Number of shading data instances. If dual index buffers are
   *  enabled, this may be lower than the total vertex count, and
   *  the second index in the pair can be used to access this data. */
  uint8_t shadingDataCount;
  /** Dual index buffer offset, relative to the meshlet header.
   *  Only relevant if dual index buffers are enabled. */
  uint16_t dualIndexOffset;
  /** Primitive data offset, relative to the meshlet header. Primitive
   *  data is tightly packed with three bytes per triangle. */
  uint16_t primitiveOffset;
  /** Vertex data offset relative to the meshlet header. */
  uint16_t vertexDataOffset;
  /** Shading data offset relative to the meshlet header. */
  uint16_t shadingDataOffset;
  /** Number of joint index and weight structures for each vertex,
   *  i.e. the maximum number of joint influences per vertex. */
  uint16_t jointCountPerVertex;
  /** Offset of the joint index and weight arrays within the buffer.
   *  This is laid out in such a way that joint data is stored in
   *  \c vertexDataCount consecutive elements for every possible
   *  joint. */
  uint16_t jointDataOffset;
  /** Morph target vertex data offset. Stores a tightly packed
   *  array of material-specific morph data structures that can
   *  be indexed using the vertex mask and offset from the morph
   *  target metadata structure. */
  uint16_t morphDataOffset;
  /** Morph target metadata offset. Morph data does not use dual
   *  index buffers, and instead needs to be loaded for each
   *  output vertex. */
  uint16_t morphTargetOffset;
  /** Bit mask of morph targets affecting this meshlet. The number
   *  of bits set here determines the number of morph target info
   *  structures in the buffer. */
  uint16_t morphTargetCount;
  /** Number of unique joints used by this meshlet. Per-vertex joint
   *  indices will index into the local joint index array, which
   *  immediately follows the meshlet header as an array of 16-bit
   *  indices, which in turn indexes into the mesh instance skin. */
  uint16_t jointCount;
  /** Dominant joint. If this is a valid joint index, all vertices
   *  within the meshlet must be transformed using this joint with
   *  a weight of 1. \c jointCount will be 0 in that case. */
  uint16_t jointIndex;
  uint32_t reserved1;
};

static_assert(sizeof(GfxMeshletHeader) == 32);


/**
 * \brief Joint index + weight pair
 */
struct GfxMeshletJointData {
  constexpr static uint32_t WeightBits = 11;
  constexpr static uint32_t WeightFactor = (1u << WeightBits) - 1;

  GfxMeshletJointData() = default;

  GfxMeshletJointData(
          uint16_t                      index,
          float                         weight)
  : jointWeightAndIndex((index << WeightBits) |
      clamp<uint16_t>(uint16_t(weight * float(WeightFactor)), 0, WeightFactor)) { }

  /** Joint weight in the lower 11 bits as a normalized
   *  unsigned integer, and local joint index in the
   *  upper 5 bits. */
  uint16_t jointWeightAndIndex;

  uint16_t getIndex() const {
    return uint16_t(jointWeightAndIndex >> WeightBits);
  }

  float getWeight() const {
    return float(jointWeightAndIndex & WeightFactor) / float(WeightFactor);
  }
};

static_assert(sizeof(GfxMeshletJointData) == 2);


/**
 * \brief Meshlet ray tracing info
 *
 * Stores all information necessary to build bottom-level
 * BVHs for meshlet-based geometry. The intention is that
 * all meshlets of an LOD are packed into one bottom-level
 * BVH.
 */
struct GfxMeshletRayTracingInfo {
  /** Offset of meshlet header within the data buffer,
   *  relative to the start of the actual GPU buffer. */
  uint32_t headerOffset;
  /** Offset of vertex data, relative to the meshlet header.
   *  The actual vertex layout of the can be queried from
   *  the mesh material. */
  uint32_t vertexOffset;
  /** Offset of index data relative to the meshlet header.
   *  Indices for BVH builds are stored as 16-bit. */
  uint32_t indexOffset;
  /** Number of vertices in the meshlet. */
  uint8_t vertexCount;
  /** Number of primitives in the meshlet. */
  uint8_t primitiveCount;
  /** Dominant joint. If this is a valid joint index, a single
   *  transform can be computed for the entire meshlet rather
   *  than having to rebuild the bottom-level BVH. */
  uint16_t jointIndex;
};

static_assert(sizeof(GfxMeshletRayTracingInfo) == 16);


/**
 * \brief Meshlet metadata
 *
 * CPU-side copy of meshlet info, which can be useful when creating
 * ray tracing acceleration structures for meshlet-based geometry.
 */
struct GfxMeshletMetadata {
  /** Meshlet culling info and buffer data offset */
  GfxMeshletInfo info = { };
  /** Meshlet data header */
  GfxMeshletHeader header = { };
  /** Meshlet ray tracing info */
  GfxMeshletRayTracingInfo rayTracing = { };
};


/**
 * \brief Morph target metadata structure
 */
struct GfxMeshletMorphTargetInfo {
  /** Morph target index. */
  uint16_t targetIndex;
  /** Index of the morph data structure for the first affected
   *  vertex in the meshlet morh data array. */
  uint16_t dataIndex;
  /** Bit mask of meshlet vertices affected by this morph target.
  *   Requires that the meshlet has at most 128 vertices. */
  std::array<uint32_t, 4> vertexMask;
};

static_assert(sizeof(GfxMeshletMorphTargetInfo) == 20);


/**
 * \brief Meshlet attribute stream
 *
 * Meshlet data is organized in two streams in order to
 * improve access locality during depth-only passes.
 */
enum class GfxMeshletAttributeStream : uint16_t {
  /** Stream that contains data relevant for primitive generation.
   *  This includes the vertex position as well as joint indices
   *  and weights, but may also include texture coordinates for
   *  alpha-tested geometry. */
  eVertexData   = 0,
  /** Stream that contains data relevant only for shading. This
   *  generally includes normals, texture coordinates etc. */
  eShadingData  = 1,
};


/**
 * \brief Meshlet vertex attribute semantic
 *
 * Assigns a specific meaning to a vertex attribute.
 */
enum class GfxMeshletAttributeSemantic : uint16_t {
  eNone         = 0,
  ePosition     = 1,
  eJointIndex   = 2,
  eJointWeight  = 3,
  eNormal       = 4,
  eTangent      = 5,
  eTexCoord     = 6,
  eColor        = 7,
};


/**
 * \brief Meshlet vertex attribute metadata
 *
 * Provides information about the data layout of meshlet buffer
 * streams. This data is solely intended for inspection purposes,
 * and to help generate data structures for relevant shaders.
 */
struct GfxMeshletAttributeMetadata {
  /** Human-readable name of the attribute. May correspond
   *  to struct member or variable names in a shader. */
  GfxSemanticName name = "";
  /** Exact format of the attribute. If not set, the
   *  size of the attribute cannot be inferred. */
  GfxFormat dataFormat = GfxFormat::eUnknown;
  /** The data stream that the attribute is stored in. */
  GfxMeshletAttributeStream stream = GfxMeshletAttributeStream::eVertexData;
  /** Semantic. Some attributes fulfill a specific role
   *  that may need to be known, e.g. the vertex position. */
  GfxMeshletAttributeSemantic semantic = GfxMeshletAttributeSemantic::eNone;
  /** Semantic index in case multiple attributes specify
   *  the same semantic. This can be useful to identify
   *  a specific set of texture coordinates. */
  uint16_t semanticIndex = 0;
  /** Offset of the attribute within the structure. */
  uint16_t dataOffset = 0;
  /** Whether or not this attribute is included in
   *  the morph target data structure. */
  bool morph = false;
  /** Offset of the attribute within the morph data
   *  structure, in case the attribute is morphed. */
  uint16_t morphOffset = 0;
};


/**
 * \brief Mesh LOD
 *
 * Stores a maximum view distance for the purpose of LOD
 * selection, as well as the range of meshlets within the
 * LOD. This data is intended to be used by the task shader,
 * but is also available to the CPU for regular draws.
 */
struct GfxMeshLod {
  /** Buffer index of where meshlet data for this LOD is stored.
   *  Within a mesh, LODs must be ordered in such a way that view
   *  distance and buffer index increase monotonically. The lowest
   *  LODs of the entire geometry should generally be in buffer 0. */
  uint16_t bufferIndex;
  /** Maximum view distance for this LOD. The LOD must not be used
   *  if the scaled view distance is greater than or equal to this
   *  value. */
  float16_t maxDistance;
  /** Index of first meshlet for this LOD within the given buffer.
   *  Meshlet metadata is packed tightly at the start of the buffer. */
  uint16_t meshletIndex;
  /** Number of meshlets for this LOD. */
  uint16_t meshletCount;
};

static_assert(sizeof(GfxMeshLod) == 8);


/**
 * \brief Mesh LOD metadata
 *
 * Stores info for a given LOD.
 */
struct GfxMeshLodMetadata {
  /** LOD data as it is stored in the GPU buffer. */
  GfxMeshLod info = { };
  /** Index of the first meshlet within this LOD within
   *  the CPU-side meshlet metadata buffer. */
  uint32_t firstMeshletIndex = 0;
};


/**
 * \brief Mesh info
 *
 * Stores a range of LODs for this mesh, as well as some other
 * information that may be useful to the task shader or compute
 * shaders interacting with meshlet data.
 *
 * Mesh metadata is tightly packed within the geometry buffer in order
 * to easily allow accessing this data through constant buffers, however
 * LOD and meshlet metadata are meant to be stored closely together for
 * each mesh in order to improve data locality within the task shader.
 */
struct GfxMeshInfo {
  /** Number of LODs used in this mesh. Should generally not exceed 32
   *  in order to facilitate efficient GPU-driven LOD selection. */
  uint8_t lodCount;
  /** Material index within the asset. This is not meant to index into
   *  a global material list, rather, the material to use can be
   *  determined on a per-instance basis and can be used to look up
   *  shaders and other properties. */
  uint8_t materialIndex;
  /** Maximum number of meshlets in any LOD. This info is necessary
   *  in order to dispatch the correct number of task shader workgroups.
   *  If 0, this mesh does not use meshlets and must be rendered using
   *  regular draws. */
  uint16_t maxMeshletCount;
  /** Number of instances of this mesh within the asset. If 0, then
   *  there will be exactly one instance with default parameters. */
  uint16_t instanceCount;
  /** Number of joint indices stored for each skinned instance. This
   *  is not directly related to the size of the joint index buffer,
   *  as instances may share the same buffer or use unique buffers. */
  uint16_t skinJoints;
  /** Offset to first set of instance data for this mesh, relative
   *  to the start of the geometry buffer, in bytes. Always aligned
   *  to 16 bytes. Not defined if the instance count is 0. */
  uint32_t instanceDataOffset;
  /** Offset to first LOD description, relative to the start of
   *  the geometry buffer, in bytes. Always aligned to 16 bytes. */
  uint32_t lodInfoOffset;
  /** Offset of skin data relative to the start of the geometry
   *  buffer, in bytes. Always aligned to 16 bytes. */
  uint32_t skinDataOffset;
  /** Minimum view distance of any LOD within the mesh. If the view
   *  distance is lower, the mesh must be culled entirely. Using this
   *  can be useful to switch between meshes with local instancing at
   *  higher LODs, and single-mesh versions of the same geometry at
   *  lower LODs to reduce the meshlet count. */
  float16_t minDistance;
  /** Maximum view distance for this mesh. Must be equal to the
   *  maximum distance of any LOD. The mesh must be culled entirely
   *  if the current view distance is greater than or equal to this. */
  float16_t maxDistance;
};

static_assert(sizeof(GfxMeshInfo) == 24);


/**
 * \brief Mesh instance flags
 *
 * Any local mirroring takes place in mesh space, before
 * any other transforms are applied. Mesh shaders must
 * change the primitive winding order as necessary.
 */
enum class GfxMirroring : uint16_t {
  eMirrorNone = 0x0,
  eMirrorX    = 0x1,
  eMirrorY    = 0x2,
  eMirrorZ    = 0x3,
};


/**
 * \brief Mesh instance data
 *
 * Stores properties of a single mesh instance in a compact format.
 */
struct GfxMeshInstance {
  // Transform quaternion
  Vector<float, 4> transform;
  // Translation vector
  Vector<float, 3> translate;
  // Index into the skin data buffer, which refers
  // to the joint with a relative joint index of 0.
  uint16_t jointIndex;
  // Extra data for things like the mirroring mode.
  uint16_t extra;

  static uint16_t packExtraData(
          GfxMirroring                  mirror) {
    return uint16_t(mirror);
  }
};

static_assert(sizeof(GfxMeshInstance) == 32);


/**
 * \brief Mesh instance metadata
 *
 * Assigns a name and an instance index
 * to a given mesh instance.
 */
struct GfxMeshInstanceMetadata {
  /** Human-readable instance name. Must be unique
   *  within the asset, not just within the mesh. */
  GfxSemanticName name = "";
  /** Mesh instance metadata */
  GfxMeshInstance info = { };
  /** Mesh index within the asset. */
  uint32_t meshIndex = 0;
  /** Instance index within the mesh. */
  uint32_t instanceIndex = 0;
};


/**
 * \brief Mesh metadata
 *
 * Stores information about a single mesh within the
 * asset, including both metadata and information that
 * is relevant for rendering.
 */
struct GfxMeshMetadata {
  /** Human-readable mesh name. Does not need to be unique
   *  globally, but should be unique within the asset. */
  GfxSemanticName name = "";
  /** Mesh properties that are also available to the GPU. */
  GfxMeshInfo info = { };
  /** Mesh index within the asset. This is only
   *  useful when looking up a mesh by name. */
  uint32_t meshIndex = 0;
  /** Index into the CPU-side LOD metadata array. */
  uint32_t lodMetadataIndex = 0;
  /** Index into the CPU-side instance data array */
  uint32_t instanceDataIndex = 0;
};


/**
 * \brief Mesh material metadata
 */
struct GfxMeshMaterialMetadata {
  /** Human-readable material name. Does not need to be
   *  unique globally, but should be unique within the
   *  asset. Can be used to help link mesh materials to
   *  actual application-provided material properties. */
  GfxSemanticName name = "";
  /** Material index. Only useful when looking up materials
   *  by name. */
  uint32_t materialIndex = 0;
  /** Index of first attribute description within the list
   *  of vertex attributes. This may be useful to generate
   *  structs for use within shaders. */
  uint32_t attributeIndex = 0;
  /** Number of vertex attribute descriptions. */
  uint32_t attributeCount = 0;
  /** Vertex data stride, in bytes. */
  uint32_t vertexDataStride = 0;
  /** Shading data stride, in bytes. */
  uint32_t shadingDataStride = 0;
  /** Morph target data stride, in bytes */
  uint32_t morphDataStride = 0;
};


/**
 * \brief Joint data
 *
 * Stores T-pose joint position in object space, as well as
 * the ID of the parent joint. Used to compute absolute joint
 * transforms for animation purposes.
 */
struct GfxJoint {
  /** Default position of the joint */
  Vector3D position;
  /** Radius of the bounding sphere around the joint's position in
   *  model space. This can be used to compute bounding volumes for
   *  skinned meshes at runtime. If the relative joint transform has
   *  a translation component, its vector length must also be added
   *  to the radius as well in order to account for low weights. */
  float16_t radius;
  /** Parent joint. Joints must be ordered in such a way that
   *  any joint precedes all of its children in the list, and
   *  that parent indices are ordered in ascending order. */
  uint16_t parent;
};


/**
 * \brief Joint metadata
 *
 * Stores the T-pose joint position, as well as a
 * name and an index to identify the joint by.
 */
struct GfxJointMetadata {
  /** Name to identify the joint by */
  GfxSemanticName name = "";
  /** Joint position, in object space. */
  GfxJoint info = { };
  /** Joint index. Useful after looking up the joint
   *  by name, in order to set up local transforms. */
  uint32_t jointIndex = 0;
};


/**
 * \brief Morph target metadata
 *
 * Stores the name of a morph target for lookup
 * purposes.
 */
struct GfxMorphTargetMetadata {
  /** Name to identify the morph target by */
  GfxSemanticName name = "";
  /** Morph target index. Useful after looking up the
   *  target by name, in order to set up weights. */
  uint32_t morphTargetIndex = 0;
};


/**
 * \brief Animation buffer header
 *
 * Stores the data layout of the animation buffer. The
 * animation buffer stores flat arrays of all data with
 * indices into those arrays provided by animation groups.
 */
struct GfxAnimationInfo {
  /** Number of animation groups in the buffer */
  uint32_t groupCount;
  /** Offset of keyframe data within the buffer,
   *  in bytes, relative to the animation header. */
  uint32_t keyframeDataOffset;
  /** Offset of joint transform data within the buffer */
  uint32_t jointDataOffset;
  /** Offset of morph target weights within the buffer */
  uint32_t weightDataOffset;
};

static_assert(sizeof(GfxAnimationInfo) == 16);


/**
 * \brief Animation group flags
 */
enum class GfxAnimationGroupFlag : uint32_t {
  /** Use Slerp for quaternion interpolation rather than
   *  normalized linear interpolation. Recommended only
   *  when angles between quaternions are very large. */
  eSlerp    = (1u << 0),

  eFlagEnum = 0
};

using GfxAnimationGroupFlags = Flags<GfxAnimationGroupFlag>;


/**
 * \brief Animation group metadata
 *
 * An animation group points to key frame data, both time
 * stamps and actual transforms, and can handle up to 32
 * joints and morph targets at once.
 *
 * Having multiple animation groups per animation is useful
 * if more than 32 joints are affected by an animation, or
 * if some joints are animated with a different set of time
 * stamps.
 *
 * Since interpolation between keyframes is generally linear, it
 * is useful to have a large number of equally spaced keyframes
 * for each animation in order to reach a high level of quality.
 * This is especially true since lookup can be performed in
 * O(log n) time within the animation shader.
 */
struct GfxAnimationGroup {
  /** Animation group flags. Determines interpolation modes. */
  GfxAnimationGroupFlags flags;
  /** Animation duration in an arbitrary unit. All animation
   *  groups within an animation must set this to the same value. */
  float duration;
  /** Index of the first keyframe node for this group. */
  uint32_t keyframeIndex;
  /** Number of top-level keyframe nodes in this group. */
  uint32_t keyframeCount;
  /** Index of the first morph target weight for the first keyframe
   *  within this animation group. For each keyframe, an array of
   *  \c morphTargetCount weights is stored at this location. */
  uint32_t morphTargetWeightIndex;
  /** Number of morph targets within the animation group. */
  uint32_t morphTargetCount;
  /** Index of the first joint transform for the first keyframe
   *  within this animation group. For each keyframe, an array of
   *  \c jointCount transforms is stored at this location. */
  uint32_t jointTransformIndex;
  /** Number of joint transforms provided by each keyframe.
   *  At most 32 joints are supported per animation group. */
  uint32_t jointCount;
  /** Fixed-size joint index buffer. Used to map the joints affected
   *  by this animation group back to absolute model joint indices. */
  std::array<uint16_t, 32> jointIndices;
  /** Fixed-size morph target index buffer. */
  std::array<uint8_t, 32> morphTargetIndices;
};

static_assert(sizeof(GfxAnimationGroup) == 128);


/**
 * \brief Animation keyframe data
 *
 * Keyframe data is stored as a broad tree in order to facilitate
 * fast lookup on the GPU. Each layer can have up to 32 child nodes,
 * which means that animations with up to 993 keyframes only need
 * two lookup iterations.
 */
struct GfxAnimationKeyframe {
  /** Timestamp represented by this keyframe. If this is not a leaf
   *  node, this must be equal to the timestamp of the first child. */
  float timestamp;
  /** Keyframe data index. For leaf nodes, this is the keyframe index
   *  for the joint transform and morph target weight indices, which
   *  must then be multiplied by the respective count. Otherwise, this
   *  is an index into the keyframe array of where the first child node
   *  of this node is stored, relative to the animation group's root node. */
  uint24_t nextIndex;
  /** Child node count. For leaf nodes, this is always 0, otherwise this
   *  stores the number of child nodes and must be between 2 and 32. */
  uint8_t nextCount;
};

static_assert(sizeof(GfxAnimationKeyframe) == 8);


/**
 * \brief Animation joint transform
 *
 * Stores a padded joint transform.
 */
struct GfxAnimationJoint {
  /** Transform quaternion */
  Vector<float, 4> transform;
  /** Translation vector */
  Vector<float, 3> translate;
  /** Reserved for future use */
  uint32_t reserved;
};

static_assert(sizeof(GfxAnimationJoint) == 32);


/**
 * \brief Animation metadata
 *
 * Stores the name of an animation as well as the range
 * of internal animation groups needed to apply it.
 */
struct GfxAnimationMetadata {
  /** Unique name of the animation */
  GfxSemanticName name = "";
  /** Animation index within the CPU array. Potentially
   *  useful after looking up the animation by name. */
  uint32_t animationIndex = 0;
  /** First animation group index. */
  uint32_t groupIndex = 0;
  /** Number of animation groups. Needed to dispatch the
   *  correct number of workgroups of the animation shader. */
  uint32_t groupCount = 0;
  /** Total duration of the animation in an arbitrary unit,
   *  usually seconds. Can be used to implement looping
   *  animations by wrapping the time stamp around. */
  float duration = 0.0f;
};


/**
 * \brief Geometry info
 *
 * Stores culling info for the mesh as well as general
 * properties that may be useful to the task shader.
 *
 * Geometry info is immediately followed by mesh metadata, so
 * that this data can be used within a single constant buffer.
 */
struct GfxGeometryInfo {
  /** Axis-aligned bounding box. Must account for all possible morph
   *  target animations. For skinned meshes, this bounding box must
   *  be expanded by the individual joint bounding volumes for each
   *  instance. */
  GfxAabb<float16_t> aabb;
  /** Data buffer count. For large assets, it is recommended to split off
   *  more detailed LODs into separate buffers, so that geometry data can
   *  be streamed in and out on demand. The metadata buffer also serves
   *  as the first data buffer, and should contain the lowest LODs for all
   *  meshes. For any given data buffer that is resident in video memory,
   *  all buffers with a lower index must also be resident, which means that
   *  data buffers should roughly be ordered by the view distance range of
   *  the LODs they contain. */
  uint8_t bufferCount;
  /** Number of materials referenced by the object. */
  uint8_t materialCount;
  /** Number of meshes within this object. */
  uint16_t meshCount;
  /** Number of joints within the skeleton. */
  uint16_t jointCount;
  /** Number of morph targets across all meshes. */
  uint16_t morphTargetCount;
  /** Offset to data buffer pointers within the metadata buffer. The
   *  application must overwrite these as it loads data buffers into memory.
   *  Since the first data buffer is part of the metadata buffer, pointers
   *  will only be stored for buffers with a non-zero index. */
  uint32_t bufferPointerOffset;
  /** Offset to joint data within the data buffer. Points to a
   *  tightly packed array of position vectors in object space. */
  uint32_t jointDataOffset;
  /** Offset to the first data buffer, in bytes. Aligned to 16
   *  bytes. Points to an array of meshlet info structures. */
  uint32_t meshletDataOffset;
  /** Offset to the animation data buffer, in bytes. Aligned to 16 bytes. */
  uint32_t animationDataOffset;
  /** Reserved for future use. */
  uint32_t reserved;
};

static_assert(sizeof(GfxGeometryInfo) == 40);


/**
 * \brief Geometry description
 *
 * Stores basic info for a geometry asset, as well as
 * additional metadata to inspect the asset in detail.
 */
struct GfxGeometry {
  /** Geometry info. Provides coarse culling info as well
   *  as the general layout of the GPU data buffers. */
  GfxGeometryInfo info = { };
  /** List of mesh LOD descriptions. */
  std::vector<GfxMeshLodMetadata> lods;
  /** List of mesh descriptions. */
  std::vector<GfxMeshMetadata> meshes;
  /** List of mesh instances */
  std::vector<GfxMeshInstanceMetadata> instances;
  /** List of meshlet vertex data offsets within their respective
   *  data buffers. Useful when creating ray tracing BVHs. */
  std::vector<GfxMeshletRayTracingInfo> meshlets;
  /** List of material descriptions. */
  std::vector<GfxMeshMaterialMetadata> materials;
  /** List of vertex attribute descriptions. */
  std::vector<GfxMeshletAttributeMetadata> attributes;
  /** List of joints */
  std::vector<GfxJointMetadata> joints;
  /** List of morph targets */
  std::vector<GfxMorphTargetMetadata> morphTargets;
  /** List of animations */
  std::vector<GfxAnimationMetadata> animations;

  /**
   * \brief Looks up a specific mesh LOD
   *
   * \param [in] mesh Mesh to look at
   * \param [in] lod LOD within the mesh
   * \returns Pointer to the given LOD, or \c nullptr
   *    if the LOD is invalid.
   */
  const GfxMeshLodMetadata* getLod(
    const GfxMeshMetadata*              mesh,
          uint32_t                      lod) const;

  /**
   * \brief Looks up meshlet within an LOD
   *
   * \param [in] mesh Mesh to look at
   * \param [in] lod Mesh LOD to look at
   * \param [in] meshlet Meshlet index within the LOD
   * \returns Pointer to meshlet info, or \c nullptr
   *    if the given meshlet index is out of bounds.
   */
  const GfxMeshletRayTracingInfo* getMeshlet(
    const GfxMeshMetadata*              mesh,
    const GfxMeshLodMetadata*           lod,
          uint32_t                      meshlet) const;

  /**
   * \brief Looks up joint by index
   *
   * \param [in] joint Joint index
   * \returns Pointer to given joint, or \c nullptr
   *    if the joint index is out of bounds.
   */
  const GfxJointMetadata* getJoint(
          uint32_t                      joint) const;

  /**
   * \brief Looks up mesh metadata by name
   *
   * \param [in] name Mesh name
   * \returns Mesh object with that name, or \c nullptr
   *    if no mesh with the given name exists.
   */
  const GfxMeshMetadata* findMesh(
    const char*                         name) const;

  /**
   * \brief Looks up mesh instance by name
   *
   * This is the preferred way of obtaining info about
   * geometry, e.g. for the purpose of selectively not
   * rendering certain meshes, since mesh instancing
   * may be arbitrary and depends on export settings.
   * \param [in] name Instance name
   * \returns Instance object with that name, or \c nullptr
   *    if no mesh instance has the given name.
   */
  const GfxMeshInstanceMetadata* findInstance(
    const char*                         name) const;

  /**
   * \brief Looks up material metadata by name
   *
   * \param [in] name Material name
   * \returns Material object with that name, or \c nullptr
   *    if no material with the given name exists.
   */
  const GfxMeshMaterialMetadata* findMaterial(
    const char*                         name) const;

  /**
   * \brief Looks up attribute metadata by name
   *
   * \param [in] material Material to scan
   * \param [in] name Name of the attribute
   * \returns Attribute metadata, or \c nullptr
   */
  const GfxMeshletAttributeMetadata* findAttribute(
    const GfxMeshMaterialMetadata*      material,
    const char*                         name) const;

  /**
   * \brief Looks up attribute metadata by semantic
   *
   * Can for example be used to locate the vertex position.
   * \param [in] material Material to scan
   * \param [in] semantic Attribute semantic
   * \param [in] index Semantic index
   * \returns Attribute metadata, or \c nullptr
   */
  const GfxMeshletAttributeMetadata* findAttribute(
    const GfxMeshMaterialMetadata*      material,
          GfxMeshletAttributeSemantic   semantic,
          uint16_t                      index) const;

  /**
   * \brief Looks up joint by name
   *
   * \param [in] name Name of the joint
   * \returns Pointer to joint metadata, or \c nullptr
   *    if no joint with that name can be found.
   */
  const GfxJointMetadata* findJoint(
    const char*                         name) const;

  /**
   * \brief Looks up morph target by name
   *
   * \param [in] name Name of the morph target
   * \returns Pointer to morph target metadata, or
   *    \c nullptr if no morph target with that name
   *    can be found.
   */
  const GfxMorphTargetMetadata* findMorphTarget(
    const char*                         name) const;

  /**
   * \brief Serializes geometry metadata to a stream
   *
   * \param [in] output Stream to write to
   * \returns \c true on success
   */
  bool serialize(
          WrBufferedStream&             output);

  /**
   * \brief Reads serialized geometry metadata
   *
   * \param [in] in Stream to read from
   * \returns \c true on success
   */
  bool deserialize(
          RdMemoryView                  input);

};

}
