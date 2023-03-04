#pragma once

#include <array>
#include <string>

#include "../util/util_flags.h"
#include "../util/util_iface.h"
#include "../util/util_matrix.h"
#include "../util/util_small_vector.h"
#include "../util/util_types.h"
#include "../util/util_vector.h"

#include "gfx_descriptor_handle.h"
#include "gfx_format.h"
#include "gfx_types.h"

namespace as {

/**
 * \brief BVH flags
 */
enum class GfxRayTracingBvhFlag : uint32_t {
  /** Allows updating the BVH after the initial build. Should not
   *  be set for static geometry for optimal traversal performance. */
  eDynamic        = (1u << 0),
  eFlagEnum       = 0
};

using GfxRayTracingBvhFlags = Flags<GfxRayTracingBvhFlag>;


/**
 * \brief Axis-aligned bounding box
 */
struct GfxAabb {
  /** Minimum box coordinates */
  Vector3D min = Vector3D(0.0f);
  /** Maximum box coordinates */
  Vector3D max = Vector3D(0.0f);
};


/**
 * \brief Ray tracing geometry opacity
 *
 * Influences ray traversal behaviour.
 */
enum class GfxRayTracingOpacity : uint32_t {
  /** Geometry is opaque, and if hit by a ray, any ray
   *  intersection will be treated as a hit. */
  eOpaque     = 0,
  /** Geometry is not fully opaque, but whether an intersecion
   *  is a hit can be determined in no more than one step. This
   *  is most useful for AABBs that represent opaque geometry. */
  eProbeOnce  = 1,
  /** Geometry is not fully opaque, and intersections must be
   *  probed one by one. This is useful for masked meshes. */
  eProbeAny   = 2,
};


/**
 * \brief Ray tracing mesh properties
 */
struct GfxRayTracingMeshInfo {
  /** Vertex data format. Must support the \c GfxFormatFeature::eBvhGeometry
   *  feature. Vertex positions are assumed to be be tightly packed within the
   *  source buffer when building ray tracing BVHs. */
  GfxFormat vertexFormat;
  /** Index data format. If \c GfxFormat::eUnknown, the geometry is not indexed,
   *  otherwise this must be \c GfxFormat::eR16ui or \c GfxFormat::eR32ui. */
  GfxFormat indexFormat;
  /** Number of vertices in the buffer. If the geometry is indexed, this must
   *  be greater than the largest index value in the index buffer, otherwise,
   *  this must be at least as large as \c primitiveCount * 3. */
  uint32_t vertexCount;
  /** Number of triangle primitives in the mesh. Defines the number of
   *  indices or vertices that will be consumed from source buffers. */
  uint32_t primitiveCount;
};


/**
 * \brief Ray tracing AABB properties
 */
struct GfxRayTracingAabbInfo {
  /** Number of tightly packed AABBs in the source buffer. The data
   *  layout of an AABB matches that of the \c GfxAabb structure. */
  uint32_t boundingBoxCount;
};


/**
 * \brief Ray tracing geometry properties
 */
union GfxRayTracingGeometryInfo {
  GfxRayTracingMeshInfo mesh;
  GfxRayTracingAabbInfo aabb;
};


/**
 * \brief Ray tracing geometry type
 */
enum class GfxRayTracingGeometryType : uint16_t {
  /** Triangle geometry. Properties are defined by
   *  the \c GfxRayTracingMeshData structure. */
  eMesh = 0,
  /** Triangle geometry. Properties are defined by
   *  the \c GfxRayTracingAabbData structure. */
  eAabb = 1,
};


/**
 * \brief Ray tracing geometry flags
 */
enum class GfxRayTracingGeometryFlag : uint16_t {
  /** Allows specifying a transform matrix for meshes. May be
   *  useful when combining multiple meshes into one BVH. */
  eMeshTransform  = (1u << 0),
  eFlagEnum       = 0
};

using GfxRayTracingGeometryFlags = Flags<GfxRayTracingGeometryFlag>;


/**
 * \brief Ray tracing geometry properties
 *
 * Stores properties of a single geometry object within a geometry BVH.
 */
struct GfxRayTracingGeometry {
  /** Geometry type. Defines which member of \c data is used. */
  GfxRayTracingGeometryType type = GfxRayTracingGeometryType::eMesh;
  /** Geometry flags */
  GfxRayTracingGeometryFlags flags = 0;
  /** Geometry opacity. Defines how this geometry
   *  is treated during ray traversal. */
  GfxRayTracingOpacity opacity = GfxRayTracingOpacity::eOpaque;
  /** Geometry properties. */
  GfxRayTracingGeometryInfo data = { };
};


/**
 * \brief Ray tracing geometry info
 *
 * Stores properties of a geometry BVH, but without specifying
 * any data sources. Data is provided during build and update
 * operations.
 */
struct GfxRayTracingGeometryDesc {
  /** Debug name. */
  const char* debugName = nullptr;
  /** BVH flags */
  GfxRayTracingBvhFlags flags = 0;
  /** Geometry descriptions */
  small_vector<GfxRayTracingGeometry, 8> geometries;
};


/**
 * \brief Ray tracing instance flags
 */
enum class GfxRayTracingInstanceFlag : uint8_t {
  /** Overrides face culling to be disabled. Useful for two-sided
   *  materials while keeping face culling for other materials. */
  eDisableFaceCulling   = (1u << 0),
  /** Front face is counter-clockwise. Note that instance transforms
   *  that mirror geometry do not affect primitive winding. */
  eFrontFaceCcw         = (1u << 1),
  /** Overrides geometries to be opaque. */
  eForceOpaque          = (1u << 2),
  /** Overrides geometries to be non-opaque. */
  eForceNonOpaque       = (1u << 3),
  eFlagEnum             = 0
};


using GfxRayTracingInstanceFlags = Flags<GfxRayTracingInstanceFlag>;


/**
 * \brief Ray tracing instance data
 *
 * Describes an object instance in world space.
 * This structure may be located in GPU memory.
 */
struct GfxRayTracingInstanceData {
  /** Row-major transform matrix from object space into world space.
   *  Note that due to being row major, columns and rows are flipped. */
  Matrix4x3 transform = Matrix4x3::identity();
  /** Unique ID to identify the object instance. */
  uint24_t instanceId = uint24_t(0);
  /** 8-bit visibility mask. This instance will only be considered
   *  during traversal if this mask ANDed with the ray's instance
   *  mask is not zero. */
  uint8_t visibilityMask = 0xff;
  /** Reserved. This represents the shader binding table offset
   *  for ray tracing pipelines, but we do not expose those. */
  uint24_t reserved = uint24_t(0);
  /** Instance flags that override ray traversal properties. */
  GfxRayTracingInstanceFlags flags = 0;
  /** GPU address of the geometry BVH to instantiate */
  uint64_t geometryBvhAddress = 0;
};

static_assert(std::is_standard_layout_v<GfxRayTracingInstanceData>
  && sizeof(GfxRayTracingInstanceData) == 64);


/**
 * \brief Ray tracing geometry info
 *
 * Stores properties of a set of instances.
 */
struct GfxRayTracingInstance {
  /** Geometry opacity. Should match that of all
   *  geometries referenced by the given instances. */
  GfxRayTracingOpacity opacity = GfxRayTracingOpacity::eOpaque;
  /** Number of consecutive instances. */
  uint32_t instanceCount = 0;
};


/**
 * \brief Ray tracing instance info
 */
struct GfxRayTracingInstanceDesc {
  /** Debug name. */
  const char* debugName = nullptr;
  /** BVH flags */
  GfxRayTracingBvhFlags flags = 0;
  /** Instance descriptions */
  std::vector<GfxRayTracingInstance> instances;
};


/**
 * \brief Ray tracing BVH type
 */
enum class GfxRayTracingBvhType : uint32_t {
  /** Geometry BVH. Cannot be used directly for anything other
   *  than updates and copies. Can be part of an instance BVH. */
  eGeometry,
  /** Instance BVH. Can be used directly for ray traversal and
   *  therefore allows retrieving a descriptor. */
  eInstance,
};


/**
 * \brief Ray tracing BVH description
 *
 * Mostly exists for API consistency. Unlike most objects,
 * BVHs are not directly created from this description, but
 * instead this is inferred from the actual creation parameters.
 */
struct GfxRayTracingBvhDesc {
  /** Debug name */
  const char* debugName = nullptr;
  /** BVH type */
  GfxRayTracingBvhType type = GfxRayTracingBvhType::eGeometry;
  /** BVH flags */
  GfxRayTracingBvhFlags flags = 0;
  /** BVH size in bytes */
  uint64_t size = 0;
};


/**
 * \brief Ray tracing mesh data source
 */
struct GfxRayTracingMeshDataSource {
  /** GPU address of first vertex */
  uint64_t vertexData;
  /** GPU address of first index */
  uint64_t indexData;
  /** GPU address of transform matrix */
  uint64_t transformData;
};


/**
 * \brief Ray tracing AABB data source
 */
struct GfxRayTracingAabbDataSource {
  /** GPU address of where AABBs are stored */
  uint64_t boundingBoxData;
};


/**
 * \brief Ray tracing instance data source
 */
struct GfxRayTracingInstanceDataSource {
  /** GPU address of where instance data is stored */
  uint64_t instanceData;
};


/**
 * \brief Ray tracing BVH data source
 *
 * Defines where to read input data from
 * during BVH build or update operations.
 */
union GfxRayTracingBvhData {
  GfxRayTracingMeshDataSource mesh;
  GfxRayTracingAabbDataSource aabb;
  GfxRayTracingInstanceDataSource instances;
};


/**
 * \brief Ray tracing BVH build type
 */
enum class GfxRayTracingBvhBuildMode : uint32_t {
  /** Initial build */
  eBuild      = 0,
  /** Update. This requires that an initial
   *  built has been performed before. */
  eUpdate     = 1,
};


/**
 * \brief Ray tracing BVH interface
 */
class GfxRayTracingBvhIface {

public:

  GfxRayTracingBvhIface(
    const GfxRayTracingBvhDesc&         desc,
          uint64_t                      va)
  : m_desc(desc), m_va(va) {
    if (m_desc.debugName) {
      m_debugName = m_desc.debugName;
      m_desc.debugName = m_debugName.c_str();
    }
  }

  virtual ~GfxRayTracingBvhIface() { }

  /**
   * \brief Retrieves descriptor
   *
   * Only valid for instance BVHs. Will return
   * a null descriptor for geometry BVHs.
   * \returns BVH descriptor
   */
  virtual GfxDescriptor getDescriptor() const = 0;

  /**
   * \brief Queries description
   * \returns BVH description
   */
  GfxRayTracingBvhDesc getDesc() const {
    return m_desc;
  }

  /**
   * \brief Queries GPU address
   *
   * Useful for geometry BVHs, since the GPU
   * address must be passed to instance BVHs.
   * \returns GPU address
   */
  uint64_t getGpuAddress() const {
    return m_va;
  }

private:

  GfxRayTracingBvhDesc  m_desc;
  std::string           m_debugName;
  uint64_t              m_va;

};

/** See GfxRayTracingBvhIface. */
using GfxRayTracingBvh = IfaceRef<GfxRayTracingBvhIface>;

}
