#pragma once

#include <unordered_map>
#include <unordered_set>

// not sure how to include this, the CMake project does
// not appear to set include directories we can use
#include "../../subprojects/meshoptimizer/src/meshoptimizer.h"

#include "../../src/alloc/alloc_linear.h"

#include "../../src/gfx/gfx.h"
#include "../../src/gfx/gfx_geometry.h"

#include "../../src/job/job.h"

#include "gltf_asset.h"

namespace as::gltf {

/**
 * \brief GLTF vertex data
 *
 * Normalized data will be converted to float, and
 * signed integers are sign-extended. The data layout
 * is defined by the mesh primitive.
 */
union GltfVertex {
  float     f32[64];
  uint32_t  u32[64];
  int32_t   i32[64];
};


/**
 * \brief GLTF vertex attribute
 *
 * Provides information on where vertex data is
 * stored within an unpacked vertex structure.
 */
struct GltfVertexAttribute {
  std::string       name;
  GltfDataType      type;
  uint8_t           offset;
  uint8_t           components;
};


/**
 * \brief GLTF vertex layout
 *
 * Stores offsets for each attribute of where
 * decoded vertex data is stored.
 */
struct GltfVertexLayout {
  std::vector<GltfVertexAttribute> attributes;

  /**
   * \brief Looks up attribute by name
   *
   * Slow since this is a linear search.
   * \param [in] name Attribute name
   * \returns Pointer to attribute info, or
   *    \c nullptr if no such attribute exists.
   */
  const GltfVertexAttribute* findAttribute(
    const char*                         name) const {
    for (const auto& a : attributes) {
      if (a.name == name)
        return &a;
    }

    return nullptr;
  }
};


/**
 * \brief Vertex data reader
 *
 * Convenience class to access vertex and index
 * data of a GLTF mesh primitive.
 */
class GltfVertexDataReader {

public:

  GltfVertexDataReader(
          std::shared_ptr<GltfMeshPrimitive>  primitive);

  ~GltfVertexDataReader();

  /**
   * \brief Queries vertex layout
   * \returns Vertex layout
   */
  GltfVertexLayout getLayout() const {
    return m_layout;
  }

  /**
   * \brief Counts number of primitives
   *
   * Depends on the primitive topology of the underlying
   * mesh, as well as number of indices stored in the
   * index buffer.
   * \returns Primitive count
   */
  uint32_t countPrimitives() const;

  /**
   * \brief Counts number of indices
   *
   * If the mesh uses a primitive topology other than point,
   * line list or triangle list, this may be different from
   * the number of indices stored in the original index buffer.
   * This is proportional to the primitive count.
   * \returns Resolved index count
   */
  uint32_t countIndices() const;

  /**
   * \brief Counts number of vertices
   *
   * Returns the number of vertices stored in the mesh.
   * \returns Vertex count
   */
  uint32_t countVertices() const;

  /**
   * \brief Queries resolved primitive topology
   *
   * Returns the corresponding list topology of the original
   * topology, e.g. \c eTriangleList for \c eTriangleStrip.
   * \returns Resolved primitive topology
   */
  GltfPrimitiveTopology getTopology() const;

  /**
   * \brief Generates index buffer
   *
   * Any non-list topology will be implicitly converted to
   * the corresponding list topology so that we can use it
   * with existing tools. Indices will be converted to
   * 32-bit unsigned integers.
   * \param [out] dst Index data. Must be large enough to
   *    hold at least \c countIndices indices.
   */
  void readIndices(
          uint32_t*                     dst) const;

  /**
   * \brief Generates vertex buffer
   *
   * Reads each vertex into a \c GltfVertex structure,
   * performing data conversion on the fly as necessary.
   * \param [out] dst Vertex data. Must be large enough to
   *    hold at least \c countVertices vertices.
   */
  void readVertices(
          GltfVertex*                   dst) const;

  /**
   * \brief Reads set of vertices from a morph target
   *
   * If any resulting vertex is entirely zero after
   * format conversion, it should be ignored.
   * \param [in] target Morph target
   * \param [in] vertexCount Number of vertices to read
   * \param [in] vertexIndices Vertex indices to read
   * \param [out] dst Vertex data
   */
  void readMorphedVertices(
    const std::shared_ptr<GltfMorphTarget>& target,
          uint32_t                      vertexCount,
    const uint32_t*                     vertexIndices,
          GltfVertex*                   dst) const;

private:

  std::shared_ptr<GltfMeshPrimitive>  m_primitive;
  GltfVertexLayout                    m_layout;

  uint32_t readIndex(
    const std::shared_ptr<GltfAccessor>& accessor,
          uint32_t                      index) const;

  void readVertexRange(
    const GltfVertexAttribute&          attribute,
    const std::shared_ptr<GltfAccessor>& accessor,
          uint32_t                      vFirst,
          uint32_t                      vLast,
    const uint32_t*                     indices,
          GltfVertex*                   dst) const;

};


/**
 * \brief Vertex attribute description
 */
struct GltfPackedVertexAttributeDesc {
  std::string name;
  GfxFormat format = GfxFormat::eUnknown;
  GfxMeshletAttributeStream stream = GfxMeshletAttributeStream::eVertexData;
  bool morph = false;
};


/**
 * \brief Vertex layout description
 *
 * Stores buffer strides and metadata for individual
 * vertex attributes.
 */
struct GltfPackedVertexLayoutDesc {
  std::string name;
  std::vector<GltfPackedVertexAttributeDesc> attributes;
};

void from_json(const json& j, GltfPackedVertexLayoutDesc& desc);


/**
 * \brief Packed vertex stream enum
 */
enum class GltfPackedVertexStream : uint32_t {
  eVertexData   = 0,
  eShadingData  = 1,
  eMorphData    = 2,
};


/**
 * \brief Packed vertex layout
 *
 * Describes vertex attributes as they are stored in the
 * packed meshlet vertex buffer. Directly corresponds to
 * material metadata in the geometry structure.
 *
 * Provides convenience methods to pack imported vertices.
 */
class GltfPackedVertexLayout {

public:

  /**
   * \brief Initializes packed vertex layout
   *
   * Automatically computes strides and offsets.
   * \param [in] desc Vertex layout description
   */
  GltfPackedVertexLayout(
    const GltfPackedVertexLayoutDesc&   desc);

  /**
   * \brief Retrieves material metadata
   * \returns Material metadata structure
   */
  GfxMeshMaterialMetadata getMetadata() const {
    return m_metadata;
  }

  /**
   * \brief Retrieves attributes
   * \returns Attribute iterator pair
   */
  auto getAttributes() const {
    return std::make_pair(
      m_attributes.begin(),
      m_attributes.end());
  }

  /**
   * \brief Finds attribute by name
   *
   * \param [in] name Attribute name
   * \returns Pointer to attribute, if any
   */
  const GfxMeshletAttributeMetadata* findAttribute(
    const char*                         name) const {
    for (const auto& attribute : m_attributes) {
      if (attribute.name == name)
        return &attribute;
    }

    return nullptr;
  }

  /**
   * \brief Computes data stride for given stream
   *
   * \param [in] streamType Output stream type
   * \returns Data stride, in bytes
   */
  uint32_t getStreamDataStride(
          GltfPackedVertexStream        streamType) const;

  /**
   * \brief Packs vertex data using the given layout
   *
   * The output layout will match that of the desired stream. When
   * the morph data stream is selected, only morphed attributes will
   * be written to the output stream.
   *
   * Note that while input data can be accessed via an index buffer,
   * output data will always be tightly packed so it can be furher
   * processed for meshlet building more easily.
   * \param [in] inputLayout Input data layout
   * \param [in] vertexCount Number of input vertices
   * \param [in] vertexData Input vertex data
   * \param [in] indexData Optional index buffer
   * \param [in] outputType Output stream type
   * \param [in] outputStride Output data stride
   * \param [out] data Packed vertex data
   */
  void processVertices(
    const GltfVertexLayout&             inputLayout,
          uint32_t                      vertexCount,
    const GltfVertex*                   vertexData,
    const uint32_t*                     indexData,
          GltfPackedVertexStream        outputType,
          size_t                        outputStride,
          void*                         data) const;

private:

  GfxMeshMaterialMetadata                   m_metadata;
  std::vector<GfxMeshletAttributeMetadata>  m_attributes;

  uint32_t computeDataLayout(
          GltfPackedVertexStream        streamType);

  static bool testAttributeStream(
    const GfxMeshletAttributeMetadata&  attribute,
          GltfPackedVertexStream        streamType);

  static std::pair<uint32_t, uint32_t> computeFormatSize(
          GfxFormat                     format);

  static std::pair<GfxMeshletAttributeSemantic, uint32_t> parseSemantic(
    const char*                         name);

};


/**
 * \brief Packed vertex layout map
 *
 * Convenience class to access vertex layout objects
 * by their material name.
 */
class GltfPackedVertexLayoutMap {

public:

  GltfPackedVertexLayoutMap();

  ~GltfPackedVertexLayoutMap();

  /**
   * \brief Creates new vertex layout
   *
   * \param [in] desc Vertex layout description
   * \returns Pointer to vertex layout if successful.
   *    If a layout with the same name already exists,
   *    it will be replaced.
   */
  std::shared_ptr<GltfPackedVertexLayout> emplace(
    const GltfPackedVertexLayoutDesc&   desc);

  /**
   * \brief Looks up vertex layout by name
   *
   * \param [in] name Vertex layout name
   * \returns Pointer to vertex layout object, or
   *    \c nullptr if no layout with that name exists.
   */
  std::shared_ptr<GltfPackedVertexLayout> find(
    const char*                         name) const;

private:

  std::unordered_map<std::string, std::shared_ptr<GltfPackedVertexLayout>> m_map;

};


/**
 * \brief Shared axis-aligned bounding box
 *
 * Convenience class to let multiple threads work
 * on computing geometry AABBs concurrently.
 */
class GltfSharedAabb {

public:

  /**
   * \brief Retrieves axis-aligned bounding box
   * \returns Final bounding box
   */
  GfxAabb<float> getAabb() const {
    return GfxAabb<float>(
      m_lo.get<0, 1, 2>(),
      m_hi.get<0, 1, 2>());
  }

  /**
   * \brief Accumulates bounds
   *
   * Expands the bounding box with the
   * given parameters as necessary.
   * \param [in] lo Minimum vertex coordinates
   * \param [in] hi Maximum vertex coordinates
   */
  void accumulate(Vector4D lo, Vector4D hi) {
    std::lock_guard lock(m_mutex);

    if (m_initialized) {
      m_lo = min(m_lo, lo);
      m_hi = max(m_hi, hi);
    } else {
      m_initialized = true;
      m_lo = lo;
      m_hi = hi;
    }
  }

private:

  std::mutex      m_mutex;
  bool            m_initialized = false;
  Vector4D        m_lo = Vector4D(0.0f);
  Vector4D        m_hi = Vector4D(0.0f);

};


/**
 * \brief Morph target lookup table
 */
using GltfMorphTargetMap = std::unordered_map<std::string, uint32_t>;


/**
 * \brief Joint info lookup table
 */
using GltfJointMap = std::unordered_map<std::shared_ptr<GltfNode>, uint32_t>;


/**
 * \brief Meshlet builder
 *
 * Provides metadata and buffer storage for a single
 * converted meshlet.
 */
class GltfMeshletBuilder : public std::enable_shared_from_this<GltfMeshletBuilder> {

public:

  GltfMeshletBuilder(
          std::shared_ptr<GltfMeshPrimitive> primitive,
          GltfVertexLayout                inputLayout,
          std::shared_ptr<GltfPackedVertexLayout> packedLayout,
          std::shared_ptr<GltfMorphTargetMap> morphTargetMap,
    const meshopt_Meshlet&                meshlet);

  ~GltfMeshletBuilder();

  /**
   * \brief Queries meshlet metadata
   * \returns Meshlet metadata
   */
  GfxMeshletMetadata getMetadata() const {
    return m_metadata;
  }

  /**
   * \brief Retrieves meshlet buffer
   * \returns Meshlet buffer
   */
  RdMemoryView getBuffer() const {
    return RdMemoryView(m_buffer);
  }

  /**
   * \brief Computes joint bounding volumes
   *
   * For each joint that affects the meshlet, computes the maximum
   * distance to any vertex within the meshlet in model space. The
   * idea is that rotations will not affect the distance regardless
   * of the weight applied, so every vertex will always be within
   * the bounding sphere of at least one joint.
   *
   * Instance bounding volumes can then be applied by computing a
   * superset of joint bounding volumes using the transformed joint
   * positions in model space as a pivot point, and adjusting the
   * sphere for scaling and translation as necessary.
   * \param [in] joints Joint metadata array
   * \param [in] skins Joint remapping buffer
   * \param [in] instance Mesh instance
   */
  void computeJointBoundingVolumes(
          std::vector<GfxJointMetadata>& joints,
    const std::vector<uint16_t>&        skins,
    const GfxMeshInstance&              instance) const;

  /**
   * \brief Builds meshlet
   *
   * \param [in] primitiveIndices Meshlet index buffer
   * \param [in] vertexIndices Indices into vertex data
   * \param [in] vertexData Pointer to vertex data
   */
  void buildMeshlet(
    const uint8_t*                      primitiveIndices,
    const uint32_t*                     vertexIndices,
    const GltfVertex*                   vertexData);

private:

  std::shared_ptr<GltfMeshPrimitive>      m_primitive;
  std::shared_ptr<GltfPackedVertexLayout> m_packedLayout;
  std::shared_ptr<GltfMorphTargetMap>     m_morphTargetMap;

  GltfVertexLayout                        m_inputLayout;

  meshopt_Meshlet                         m_meshlet;

  GfxMeshletMetadata                      m_metadata;
  std::vector<char>                       m_buffer;

  std::vector<uint16_t>                   m_localJoints;

  std::vector<GltfVertex> loadVertices(
    const uint32_t*                     indices,
    const GltfVertex*                   vertices);

  std::vector<char> packVertices(
          GltfPackedVertexStream        stream,
    const GltfVertex*                   vertices,
    const GfxMeshletJointData*          joints);

  void computeMeshletBounds(
    const GltfVertex*                   vertices,
    const uint8_t*                      indices);

  std::vector<std::pair<uint8_t, uint8_t>> computeDualIndexBuffer(
          std::vector<char>&            vertexData,
          std::vector<char>&            shadingData);

  uint32_t deduplicateData(
          std::vector<char>&            data,
          uint32_t                      stride,
          uint32_t&                     count,
          uint32_t                      index);

  bool processJoints(
          GltfVertex*                   vertices,
          std::vector<GfxMeshletJointData>& jointBuffer);

  void renormalizeWeights(
          std::vector<std::pair<float, uint16_t>>& weights) const;

  void processMorphTargets(
          std::vector<GfxMeshletMorphTargetInfo>& morphTargets,
          std::vector<char>&            morphBuffer,
    const uint32_t*                     vertexIndices);

  void buildMeshletBuffer(
    const uint8_t*                      primitiveIndices,
    const char*                         vertexData,
    const char*                         shadingData,
    const std::pair<uint8_t, uint8_t>*  dualIndexData,
    const std::vector<GfxMeshletMorphTargetInfo>& morphTargets,
    const std::vector<char>&            morphBuffer);

  static uint16_t allocateStorage(
          uint16_t&                       allocator,
          size_t                          amount);

};


/**
 * \brief Mesh primitive converter
 *
 * Provides functionality to convert a single GLTF mesh
 * primitive to a series of meshlets.
 *
 * This will automatically perform certain optimizations
 * such as dual-indexing or the use of local joint indices
 * if beneficial.
 */
class GltfMeshPrimitiveConverter : public std::enable_shared_from_this<GltfMeshPrimitiveConverter> {
public:

  GltfMeshPrimitiveConverter(
          std::shared_ptr<GltfPackedVertexLayout> layout,
          std::shared_ptr<GltfMeshPrimitive> primitive,
          std::shared_ptr<GltfMorphTargetMap> morphTargetMap);

  ~GltfMeshPrimitiveConverter();

  /**
   * \brief Counts number of meshlets
   *
   * Only valid after the conversion job has completed.
   * \returns Meshlet count
   */
  uint32_t getMeshletCount() const {
    return uint32_t(m_meshlets.size());
  }

  /**
   * \brief Retrieves meshlet builder
   *
   * \param [in] index Meshlet index
   * \returns Meshlet builder object
   */
  std::shared_ptr<GltfMeshletBuilder> getMeshlet(
          uint32_t                      index) const {
    return index < getMeshletCount()
      ? m_meshlets[index]
      : nullptr;
  }

  /**
   * \brief Dispatches conversion job
   *
   * The first job spawned will use meshoptimizer to convert the
   * mesh into a series of meshlets, afterwards each meshlet will
   * be processed in parallel.
   * \param [in] jobs Job manager instance
   * \returns Job to synchronize with
   */
  Job dispatchConvert(
    const Jobs&                         jobs);

  /**
   * \brief Dispatches job to compute AABB
   *
   * Depending on whether morph targets are present, this will
   * compute the AABB either based on vertex data or meshlet
   * bounding spheres.
   * \param [in] jobs Job manager instance
   * \param [in] dependency Job to wait for
   * \param [in] aabb Shared AABB instance
   * \param [in] transform Instance transform
   * \returns Job to synchronize with
   */
  Job dispatchComputeAabb(
    const Jobs&                         jobs,
    const Job&                          dependency,
          std::shared_ptr<GltfSharedAabb> aabb,
          QuatTransform                 transform) const;

  /**
   * \brief Computes joint bounding volumes
   *
   * \param [in] joints Joint metadata array
   * \param [in] skins Joint remapping buffer
   * \param [in] instance Mesh instance
   */
  void computeJointBoundingVolumes(
          std::vector<GfxJointMetadata>& joints,
    const std::vector<uint16_t>&        skins,
    const GfxMeshInstance&              instance) const;

private:

  std::shared_ptr<GltfPackedVertexLayout> m_layout;
  std::shared_ptr<GltfMeshPrimitive>      m_primitive;
  std::shared_ptr<GltfMorphTargetMap>     m_morphTargetMap;

  GltfVertexLayout                        m_inputLayout;

  std::vector<uint32_t>                   m_sourceIndexBuffer;
  std::vector<GltfVertex>                 m_sourceVertexBuffer;

  std::vector<meshopt_Meshlet>            m_meshletMetadata;
  std::vector<uint8_t>                    m_meshletIndexBuffer;
  std::vector<uint32_t>                   m_meshletVertexIndices;

  std::vector<std::shared_ptr<GltfMeshletBuilder>>  m_meshlets;

  void readPrimitiveData();

  void generateMeshlets();

  void buildMeshlet(
          uint32_t                      meshlet);

  void cleanup();

};


/**
 * \brief Mesh LOD converter
 *
 * Stores LOD metadata, as well as one or more GLTF mesh primitive
 * objects that comprise the LOD. All primitives within a GLTF mesh
 * that use the same material should be grouped together for better
 * rendering efficiency.
 */
class GltfMeshLodConverter : public std::enable_shared_from_this<GltfMeshLodConverter> {

public:

  GltfMeshLodConverter(
          std::shared_ptr<GltfMesh>     mesh,
          std::shared_ptr<GltfPackedVertexLayout> layout);

  ~GltfMeshLodConverter();

  /**
   * \brief Counts number of meshlets
   *
   * Only valid after the conversion job has completed.
   * \returns Meshlet count
   */
  uint32_t getMeshletCount() const {
    return uint32_t(m_meshlets.size());
  }

  /**
   * \brief Retrieves meshlet builder
   *
   * \param [in] index Meshlet index
   * \returns Meshlet builder object
   */
  std::shared_ptr<GltfMeshletBuilder> getMeshlet(
          uint32_t                      index) const {
    return index < getMeshletCount()
      ? m_meshlets[index]
      : nullptr;
  }

  /**
   * \brief Checks whether this LOD is ordered before another
   *
   * Checks if the effective maximum view distance is greater
   * than that of another. Note that a view distance of 0 is
   * treated as infinity.
   */
  bool isOrderedBefore(const GltfMeshLodConverter& other) const {
    return (other.m_maxDistance != 0.0f) &&
      (m_maxDistance > other.m_maxDistance || m_maxDistance == 0.0f);
  }

  /**
   * \brief Checks whether the given mesh can be used with this LOD
   *
   * \param [in] mesh Mesh to check
   * \returns \c true if the meshes is part of the same LOD
   */
  bool isSameLod(
    const std::shared_ptr<GltfMesh>&    mesh) const {
    // TODO check max distance instead
    return m_mesh == mesh;
  }

  /**
   * \brief Queries approximate data size
   *
   * This is intended to be used \e only to assign LODs to geometry
   * buffers, and cannot be used for allocation purposes since this
   * will not account for data alignment.
   * \returns Approximate data buffer size
   */
  size_t getDataSize() const {
    return m_dataSize;
  }

  /**
   * \brief Queries mesh LOD metadata
   *
   * Only valid after conversion has completed. The meshlet
   * index and buffer index properties are both set to zero.
   * \returns LOD metadata
   */
  GfxMeshLodMetadata getMetadata() const;

  /**
   * \brief Adds a primitive
   *
   * \param [in] primitive Primitive to add
   * \param [in] morphTargetMap Morph target map
   */
  void addPrimitive(
          std::shared_ptr<GltfMeshPrimitive> primitive,
          std::shared_ptr<GltfMorphTargetMap> morphTargetMap);

  /**
   * \brief Dispatches conversion job
   *
   * Dispatches conversion jobs for all primitives, as
   * well as a final job that processes the result.
   * \param [in] jobs Job manager instance
   * \returns Job to synchronize with
   */
  Job dispatchConvert(
    const Jobs&                         jobs);

  /**
   * \brief Dispatches jobs to compute bounding box
   *
   * Dispatches jobs for each individual primitive.
   * \param [in] jobs Job manager instance
   * \param [in] dependency Job to wait for
   * \param [in] aabb Shared AABB instance
   * \param [in] transform Instance transform
   * \returns Job to synchronize with
   */
  Job dispatchComputeAabb(
    const Jobs&                         jobs,
    const Job&                          dependency,
          std::shared_ptr<GltfSharedAabb> aabb,
          QuatTransform                 transform) const;

  /**
   * \brief Computes joint bounding volumes
   *
   * \param [in] joints Joint metadata array
   * \param [in] skins Joint remapping buffer
   * \param [in] instance Mesh instance
   */
  void computeJointBoundingVolumes(
          std::vector<GfxJointMetadata>& joints,
    const std::vector<uint16_t>&        skins,
    const GfxMeshInstance&              instance) const;

private:

  float   m_maxDistance = 0.0f;
  size_t  m_dataSize    = 0;

  std::shared_ptr<GltfMesh>                                 m_mesh;
  std::shared_ptr<GltfPackedVertexLayout>                   m_layout;

  std::vector<std::shared_ptr<GltfMeshPrimitiveConverter>>  m_primitives;
  std::vector<std::shared_ptr<GltfMeshletBuilder>>          m_meshlets;

  void accumulateMeshlets();

};



/**
 * \brief Mesh converter
 *
 * Stores mesh metadata, as well as a list of LOD converters and
 * instance metadata. This will produce one \c GfxMesh object, so
 * a GLTF mesh with multiple primitives using different materials
 * will need to create multiple mesh converters.
 */
class GltfMeshConverter : public std::enable_shared_from_this<GltfMeshConverter> {

public:

  GltfMeshConverter(
          std::shared_ptr<GltfMesh>     mesh,
          std::shared_ptr<GltfMaterial> material,
          std::shared_ptr<GltfPackedVertexLayout> layout);

  ~GltfMeshConverter();

  /**
   * \brief Queries material
   * \returns Material
   */
  std::shared_ptr<GltfMaterial> getMaterial() const {
    return m_material;
  }

  /**
   * \brief Queries mesh metadata
   * \returns Mesh metadata
   */
  GfxMeshMetadata getMetadata() const;

  /**
   * \brief Queries instance metadata
   *
   * \param [in] index Instance index
   * \returns Instance metadata
   */
  GfxMeshInstanceMetadata getInstanceMetadata(
          uint32_t                      index) const {
    return m_instances.at(index);
  }

  /**
   * \brief Retrieves LOD converter
   *
   * \param [in] index LOD index
   * \returns LOD converter
   */
  std::shared_ptr<GltfMeshLodConverter> getLodConverter(
          uint32_t                      index) const {
    return m_lods.at(index);
  }

  /**
   * \brief Queries size of joint index array
   * \returns Number of joint indices
   */
  uint32_t getJointIndexArraySize() const {
    return m_jointIndices.size();
  }

  /**
   * \brief Retrieves a joint index
   *
   * \param [in] index Index into joint index array
   * \returns The remapped joint index
   */
  size_t getJointIndex(
          uint32_t                      index) const {
    return m_jointIndices.at(index);
  }

  /**
   * \brief Checks whether mesh and material match
   *
   * The converter may accept different mesh objects if they
   * have a tag assigning them as an LOD to the current mesh.
   * \param [in] mesh Mesh to check
   * \param [in] layout Vertex layout to check
   * \returns \c true if the converter was created
   *    for the same combination of arguments.
   */
  bool isSameMeshMaterial(
    const std::shared_ptr<GltfMesh>&    mesh,
    const std::shared_ptr<GltfPackedVertexLayout>& layout) const;

  /**
   * \brief Adds mesh primitive
   *
   * LOD data will be taken from the mesh object. Primitives
   * from the same mesh will be assigned to the same LOD.
   * \param [in] mesh Containing mesh
   * \param [in] primitive Mesh primitive
   * \param [in] morphTargetMap Morph target map
   */
  void addPrimitive(
    const std::shared_ptr<GltfMesh>&    mesh,
          std::shared_ptr<GltfMeshPrimitive> primitive,
          std::shared_ptr<GltfMorphTargetMap> morphTargetMap);

  /**
   * \brief Adds instance node
   *
   * Takes transform and skinning info from the node. Note that
   * meshes without instance nodes will be treated as if there
   * was a single instance with no transform or skinning.
   * \param [in] node Instance node 
   */
  void addInstance(
    const std::shared_ptr<GltfNode>&    node);

  /**
   * \brief Generates joint index array for skins
   *
   * Produces one set of joint indices per unique skin.
   * \param [in] jointMap Joint lookup table
   */
  void applySkins(
    const GltfJointMap&                 jointMap);

  /**
   * \brief Dispatches conversion job
   *
   * Performs meshlet conversion for all LODs and processes
   * instance data.
   * \param [in] jobs Job manager instance
   * \returns Job to synchronize with
   */
  Job dispatchConvert(
    const Jobs&                         jobs);

  /**
   * \brief Dispatches jobs to compute bounding box
   *
   * Dispatches jobs for each individual LOD and instance.
   * \param [in] jobs Job manager instance
   * \param [in] dependency Job to wait for
   * \param [in] aabb Shared AABB instance
   * \returns Job to synchronize with
   */
  Job dispatchComputeAabb(
    const Jobs&                         jobs,
    const Job&                          dependency,
          std::shared_ptr<GltfSharedAabb> aabb) const;

  /**
   * \brief Computes joint bounding volumes
   *
   * Computes the bounding sphere for all joints
   * that affect any instances of this mesh.
   * \param [in] joints Joint metadata array
   */
  void computeJointBoundingVolumes(
          std::vector<GfxJointMetadata>& joints) const;

private:

  std::shared_ptr<GltfMesh>                           m_mesh;
  std::shared_ptr<GltfMaterial>                       m_material;
  std::shared_ptr<GltfPackedVertexLayout>             m_layout;

  std::vector<std::shared_ptr<GltfMeshLodConverter>>  m_lods;
  std::vector<std::shared_ptr<GltfNode>>              m_nodes;

  std::vector<GfxMeshInstanceMetadata>                m_instances;
  std::vector<uint16_t>                               m_jointIndices;

  std::unordered_map<
    std::shared_ptr<GltfSkin>, uint16_t>              m_skinOffsets;

  uint16_t                                            m_jointCountPerSkin = 0;

  void accumulateLods();

  void processInstances();

};


/**
 * \brief Animation interpolator
 *
 * Provides functionality to read and interpolate
 * animation data at given time stamps.
 */
class GltfAnimationInterpolator {

public:

  GltfAnimationInterpolator(
          std::shared_ptr<GltfAnimationSampler> sampler,
          uint32_t                      index);

  ~GltfAnimationInterpolator();

  /**
   * \brief Reads sampler data
   * \returns Sampler data
   */
  void readData();

  /**
   * \brief Interpolates scalar keyframe data
   *
   * Reads source data as if it was scalar.
   * Useful for morph target weights.
   * \returns Interpolated key frame data
   */
  float interpolateScalar(
          float                         timestamp) const;

  /**
   * \brief Interpolates 3D vector
   *
   * Useful for joint scaling and translation.
   * \returns Interpolated key frame data
   */
  Vector3D interpolateVec3(
          float                         timestamp) const;

  /**
   * \brief Interpolates quaternion
   *
   * Uses slerp to interpolate between keyframes.
   * \returns Interpolated key frame data
   */
  Quat interpolateQuaternion(
          float                         timestamp) const;

private:

  std::shared_ptr<GltfAnimationSampler> m_sampler;

  std::vector<float>                    m_timestamps;
  std::vector<Vector4D>                 m_keyframes;

  uint32_t                              m_index = 0u;
  uint32_t                              m_count = 1u;

  uint32_t findKeyframe(
          float                         timestamp) const;

  float getKeyframePair(
          float                         timestamp,
          Vector4D&                     a,
          Vector4D&                     b) const;

};


/**
 * \brief Animation converter
 *
 * Produces keyframe data for a single GLTF animation.
 */
class GltfAnimationConverter : public std::enable_shared_from_this<GltfAnimationConverter> {
  constexpr static size_t NodesPerLayer = 32;
public:

  GltfAnimationConverter(
    const GltfJointMap&                 jointMap,
    const GltfMorphTargetMap&           morphTargetMap,
          std::shared_ptr<GltfAnimation> animation);

  ~GltfAnimationConverter();

  /**
   * \brief Retrieves animation metadata
   * \returns Animation metadata
   */
  GfxAnimationMetadata getMetadata() const;

  /**
   * \brief Pushes data to a set of builder arrays
   *
   * \param [out] groups Animation group array
   * \param [out] keyframes Keyframe array
   * \param [out] joints Joint transform array
   * \param [out] weights Morph target weight array
   */
  void pushArrays(
          std::vector<GfxAnimationGroup>& groups,
          std::vector<GfxAnimationKeyframe>& keyframes,
          std::vector<GfxAnimationJoint>& joints,
          std::vector<float>&           weights) const;

  /**
   * \brief Dispatches animation conversion job
   *
   * Creates animation groups and dispatches
   * jobs to build buffers for each of them.
   * \param [in] jobs Job manager instance
   * \returns Job to synchronize with
   */
  Job dispatchConvert(
    const Jobs&                         jobs);

private:

  struct JointInfo {
    std::shared_ptr<GltfAnimationInterpolator> translation;
    std::shared_ptr<GltfAnimationInterpolator> rotation;
    std::shared_ptr<GltfAnimationInterpolator> scale;
    QuatTransform inverseBind = QuatTransform::identity();
    uint32_t index = 0;
  };

  std::shared_ptr<GltfAnimation>    m_animation;

  std::unordered_set<std::shared_ptr<GltfAccessor>>         m_inputAccessors;
  std::unordered_map<std::shared_ptr<GltfNode>, JointInfo>  m_joints;
  std::unordered_map<uint32_t,
    std::shared_ptr<GltfAnimationInterpolator>>             m_morphTargets;

  std::vector<GfxAnimationGroup>    m_animationGroups;

  uint32_t                          m_keyframeTopLevelNodes = 0;

  std::vector<GfxAnimationKeyframe> m_keyframeArray;
  std::vector<GfxAnimationJoint>    m_jointArray;
  std::vector<float>                m_weightArray;

  void loadInterpolatorData();

  void buildKeyframeTree();

  void buildAnimationGroups();

};


/**
 * \brief GLTF asset converter
 *
 * Processes an entire GLTF asset using externally provided
 * vertex formats.
 */
class GltfConverter : public std::enable_shared_from_this<GltfConverter> {

public:

  GltfConverter(
          Jobs                          jobs,
          std::shared_ptr<Gltf>         asset,
          std::shared_ptr<GltfPackedVertexLayoutMap> layouts);

  ~GltfConverter();

  /**
   * \brief Retrieves geometry object
   *
   * Only valid after the conversion job has completed.
   * \returns Final geometry object
   */
  std::shared_ptr<GfxGeometry> getGeometry() const {
    return m_geometry;
  }

  /**
   * \brief Retrieves buffer view
   *
   * The buffer count can be queried from the geometry
   * object after conversion has completed.
   * \param [in] index Buffer index
   * \returns Buffer view
   */
  RdMemoryView getBuffer(
          uint32_t                      index) const {
    return RdMemoryView(m_buffers.at(index));
  }

  /**
   * \brief Retrieves animation buffer
   *
   * Must always be stored last in a file.
   * \returns Animation buffer view
   */
  RdMemoryView getAnimationBuffer() const {
    return RdMemoryView(m_animationBuffer);
  }

  /**
   * \brief Dispatches job to convert the asset
   *
   * Processes all meshes and builds all buffers as
   * well as the geometry object.
   * \returns Job to synchronize with
   */
  Job dispatchConvert();

private:

  Jobs                                        m_jobs;

  std::shared_ptr<Gltf>                       m_asset;
  std::shared_ptr<GltfPackedVertexLayoutMap>  m_layouts;

  std::shared_ptr<GfxGeometry>                m_geometry;
  std::vector<std::vector<char>>              m_buffers;

  std::unordered_map<
    std::shared_ptr<GltfMaterial>, uint32_t>  m_materialIndices;

  GltfJointMap                                m_jointMap;

  std::shared_ptr<GltfMorphTargetMap>         m_morphTargetMap;

  std::vector<std::shared_ptr<GltfMeshConverter>> m_meshConverters;
  std::vector<GfxJointMetadata>               m_jointMetadata;

  std::shared_ptr<GltfSharedAabb>             m_aabb;

  std::vector<std::shared_ptr<GltfAnimationConverter>> m_animationConverters;
  std::vector<char>                           m_animationBuffer;

  void buildGeometry();

  void buildBuffers(
    const std::vector<GfxMeshletMetadata>& meshlets);

  void buildAnimationBuffer();

  void computeJointBoundingVolumes();

  std::shared_ptr<GltfPackedVertexLayout> getMaterialLayout(
    const std::shared_ptr<GltfMaterial>& material);

  uint32_t getMaterialIndex(
    const std::shared_ptr<GltfMaterial>& material);

  std::shared_ptr<GltfMeshConverter> getMeshConverter(
    const std::shared_ptr<GltfMesh>&    mesh,
    const std::shared_ptr<GltfMaterial>& material);

  void addMesh(
    const std::shared_ptr<GltfMesh>&    mesh);

  void addMeshInstance(
          std::shared_ptr<GltfNode>     node);

  void addSkin(
    const std::shared_ptr<GltfNode>&    node,
          std::shared_ptr<GltfSkin>     skin);

  void addAnimation(
          std::shared_ptr<GltfAnimation> animation);

  void computeJointIndices();

  void writeBufferData(
          uint32_t                      buffer,
          uint32_t                      offset,
    const void*                         data,
          size_t                        size);

  void writeAnimationData(
          uint32_t                      offset,
    const void*                         data,
          size_t                        size);

  static uint32_t allocateStorage(
          uint32_t&                     allocator,
          size_t                        amount);

};

}
