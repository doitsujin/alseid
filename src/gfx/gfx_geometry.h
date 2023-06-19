#pragma once

#include <optional>
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
 *
 * Meshlet culling can only be enabled for largely static,
 * geometry, or in case of bounding volumes, if the bounding
 * volume accounts for all possible animations, as culling
 * primitives are not transformed.
 */
enum class GfxMeshletCullFlag : uint16_t {
  /** Frustum culling using a bounding sphere. */
  eSphere   = (1u << 0),
  /** Back-face culling using a cone. */
  eCone     = (1u << 1),
  eFlagEnum = 0
};

using GfxMeshletCullFlags = Flags<GfxMeshletCullFlag>;


/**
 * \brief Primitive data
 *
 * Stores three packed 8-bit indices for a single
 * triangle.
 */
struct GfxMeshletPrimitive {
  /** Vertex indices for the primitive */
  Vector<uint8_t, 3> indices;
};

static_assert(sizeof(GfxMeshletPrimitive) == 3);


/**
 * \brief Meshlet data
 *
 * Stores draw parameters and basic culling info for
 * a meshlet, using a GPU-friendly data format.
 */
struct GfxMeshlet {
  /** Meshlet culling flags */
  GfxMeshletCullFlags cullFlags;
  /** Bounding sphere origin */
  Vector<float16_t, 3> sphereOrigin;
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
  /** Number of primitives */
  uint16_t primitiveCount;
  /** First index in the primitive buffer */
  uint32_t firstPrimitive;
  /** First vertex in the vertex buffer */
  uint32_t firstVertex;
};

static_assert(sizeof(GfxMeshlet) == 32);


/**
 * \brief Mesh LOD data
 *
 * Stores draw parameters and a meshlet range
 * for a given LOD of a mesh.
 */
struct GfxMeshLod {
  /** Maximum view distance for this LOD. Can be ignored
   *  or scaled to match the app's needs. A value of 0
   *  indicates infinity. */
  float maxDistance = 0.0f;
  /** Number of indices */
  uint32_t indexCount = 0;
  /** First index in the index buffer */
  uint32_t firstIndex = 0;
  /** First vertex in the vertex buffer */
  uint32_t firstVertex = 0;
  /** Index of first meshlet for this LOD. */
  uint32_t meshletIndex = 0;
  /** Meshlet count. If 0, meshlets are disabled
   *  and regular draws must be used instead. */
  uint32_t meshletCount = 0;
};


/**
 * \brief Mesh data
 *
 * Stores a list of LODs for a mesh of a specific material.
 * This structure is not intended to be used by the GPU.
 */
struct GfxMesh {
  /** Material index. Materials are provided per instance, this
   *  is not intended to index into a global material list. */
  uint16_t materialIndex = 0;
  /** Vertex stream index. This describes which buffer regions
   *  to pull vertex data, primitive data and index data from. */
  uint16_t streamIndex = 0;
  /** Index of the first LOD used for this mesh, relative to
   *  the start of the LOD list within the metadata buffer. */
  uint16_t lodIndex = 0;
  /** Number of LODs in the mesh. All LODs for the mesh must
   *  be stored consecutively, with the maximum distance in
   *  descending order. */
  uint16_t lodCount = 0;
  /** Maximum number of meshlets for any given LOD.
   *  Can be used to determine the number of required
   *  task shader invocations for meshlet rendering. */
  uint32_t maxMeshletCount = 0;
};


/**
 * \brief Mesh buffer stream description
 *
 * Stores data layout info for a given vertex stream.
 * This structure is not intended to be used by the GPU.
 */
struct GfxMeshDataStream {
  /** Index buffer format */
  GfxFormat indexFormat = GfxFormat::eUnknown;
  /** Index buffer offset, in bytes */
  uint32_t indexDataOffset = 0;
  /** Index data size, in bytes */
  uint32_t indexDataSize = 0;
  /** Packed primitive data offset, in bytes.
   *  This is used for meshlet rendering. */
  uint32_t primitiveDataOffset = 0;
  /** Packed primitive data size, in bytes.
   *  If 0, meshlet rendering is disabled. */
  uint32_t primitiveDataSize = 0;
  /** Vertex position format */
  GfxFormat positionFormat = GfxFormat::eUnknown;
  /** Offset of vertex positions in buffer.
   *  Vertex positions are tightly packed. */
  uint32_t vertexPositionOffset = 0;
  /** Offset of structured vertex data in buffer. */
  uint32_t vertexDataOffset = 0;
  /** Structured vertex data stride. Defines
   *  the size of the vertex data structure. */
  uint32_t vertexDataStride = 0;
  /** Total number of vertices in the stream */
  uint32_t vertexCount = 0;
};


/**
 * \brief Geometry flags
 */
enum class GfxGeometryFlag : uint32_t {
  /** Enables frustum culling based on the AABB */
  eAabb           = (1u << 0),

  eFlagEnum       = 0
};

using GfxGeometryFlags = Flags<GfxGeometryFlag>;


/**
 * \brief Geometry description
 *
 * Stores basic info for a geometry asset and
 * outlines the layout of the data buffer.
 */
struct GfxGeometry {
  /** Geometry flags */
  GfxGeometryFlags flags = 0;
  /** Number of materials referenced by the object. */
  uint32_t materialCount = 0;
  /** Axis-aligned bounding box. If culling is enabled,
   *  this must account for any potential animation. */
  GfxAabb aabb = { };
  /** Total meshlet count stored in the data buffer. */
  uint32_t meshletCount = 0;
  /** Meshlet metadata offset within the data buffer. */
  uint32_t meshletOffset = 0;
  /** List of data stream descriptions */
  std::vector<GfxMeshDataStream> streams;
  /** List of mesh descriptions */
  std::vector<GfxMesh> meshes;
  /** List of LOD descriptions */
  std::vector<GfxMeshLod> lods;

  /**
   * \brief Serializes mesh buffer description
   *
   * \param [in] output Stream to write to
   * \returns \c true on success
   */
  bool serialize(
          WrBufferedStream&             output);

  /**
   * \brief Deserializes mesh buffer description
   *
   * \param [in] input Stream to read from
   * \returns Decoded mesh buffer description
   */
  static std::optional<GfxGeometry> deserialize(
          RdMemoryView                  input);

};

}
