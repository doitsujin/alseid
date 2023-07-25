#include <array>
#include <tuple>

#include "gltf_asset.h"

namespace as::gltf {

size_t gltfComputeDataSize(GltfComponentType type) {
  switch (type) {
    case GltfComponentType::eU8:
    case GltfComponentType::eS8:
      return 1;

    case GltfComponentType::eU16:
    case GltfComponentType::eS16:
      return 2;

    case GltfComponentType::eS32:
    case GltfComponentType::eU32:
    case GltfComponentType::eF32:
      return 4;
  }

  return 0;
}


size_t gltfComputeDataSize(const GltfDataType& type) {
  return gltfComputeDataSize(type.componentType) * type.rows * type.cols;
}


void GltfDataType::fromJson(const json& j) {
  static const std::array<std::tuple<const char*, uint16_t, uint16_t>, 7> s_types = {{
    std::make_tuple("SCALAR", uint16_t(1), uint16_t(1)),
    std::make_tuple("VEC2",   uint16_t(2), uint16_t(1)),
    std::make_tuple("VEC3",   uint16_t(3), uint16_t(1)),
    std::make_tuple("VEC4",   uint16_t(4), uint16_t(1)),
    std::make_tuple("MAT2",   uint16_t(2), uint16_t(2)),
    std::make_tuple("MAT3",   uint16_t(3), uint16_t(3)),
    std::make_tuple("MAT4",   uint16_t(4), uint16_t(4)),
  }};

  if (j.count("componentType"))
    j.at("componentType").get_to(componentType);

  if (j.count("normalized"))
    j.at("normalized").get_to(normalized);

  rows = 1;
  cols = 1;

  if (j.count("type")) {
    std::string type;
    j.at("type").get_to(type);

    for (const auto& e : s_types) {
      if (type == std::get<0>(e)) {
        rows = std::get<1>(e);
        cols = std::get<2>(e);
        break;
      }
    }
  }
}


void from_json(const json& j, GltfBuffer::Desc& desc) {
  desc = GltfBuffer::Desc();
  j.at("byteLength").get_to(desc.length);

  if (j.count("name"))  
    j.at("name").get_to(desc.name);

  if (j.count("uri"))  
    j.at("uri").get_to(desc.uri);
}


GltfBuffer::GltfBuffer() {
  
}


GltfBuffer::GltfBuffer(
  const Desc&                           desc)
: m_name(desc.name)
, m_size(desc.length) {

}


bool GltfBuffer::readFromFile(
  const IoFile&                       file,
        uint64_t                      offset,
        uint64_t                      length) {
  m_data.resize(length);

  IoStatus status = file->read(offset, length, m_data.data());
  return status == IoStatus::eSuccess;
}


bool GltfBuffer::readFromStream(
        RdBufferedStream&             stream,
        uint64_t                      length) {
  m_data.resize(length);
  return stream.read(m_data.data(), m_data.size());
}


bool GltfBuffer::readFromString(
        std::string_view              base64,
        uint64_t                      length) {
  m_data.resize(length);

  // Generate a look-up table on te fly. This isn't
  // exactly a great way to do this, but good enough.
  std::array<uint8_t, 256> lut = { };

  for (uint8_t i = 0; i < 26; i++) {
    lut[uint8_t('A') + i] = i;
    lut[uint8_t('a') + i] = i + 26;
  }

  for (uint8_t i = 0; i < 10; i++)
    lut[uint8_t('0') + i] = i + 52;

  lut[uint8_t('+')] = 62;
  lut[uint8_t('/')] = 63;

  // Decode 4 characters per iteration. We can ignore
  // padding since we know the desired output length.
  size_t size = base64.size();

  size_t i = 0;
  size_t o = 0;

  while (i < size && o < length) {
    uint32_t buffer = 0;

    for (size_t j = 0; j < 4; j++) {
      buffer <<= 6;

      if (i < size)
        buffer |= lut[uint8_t(base64[i++])];
    }

    if (o < length) m_data[o++] = uint8_t(buffer >> 16);
    if (o < length) m_data[o++] = uint8_t(buffer >> 8);
    if (o < length) m_data[o++] = uint8_t(buffer);
  }

  return i == size && o == length;
}




void from_json(const json& j, GltfBufferView::Desc& desc) {
  desc = GltfBufferView::Desc();
  j.at("buffer").get_to(desc.bufferIndex);
  j.at("byteLength").get_to(desc.byteLength);

  if (j.count("byteOffset"))
    j.at("byteOffset").get_to(desc.byteOffset);

  if (j.count("byteStride"))
    j.at("byteStride").get_to(desc.byteStride);
}


GltfBufferView::GltfBufferView(
        std::shared_ptr<GltfBuffer>   buffer,
  const Desc&                         desc)
: m_buffer  (std::move(buffer))
, m_offset  (desc.byteOffset)
, m_size    (desc.byteLength)
, m_stride  (desc.byteStride) {

}


GltfBufferView::~GltfBufferView() {

}




void from_json(const json& j, GltfAccessor::Desc& desc) {
  std::vector<double> minBounds;
  std::vector<double> maxBounds;

  desc = GltfAccessor::Desc();
  desc.bufferView = ~0u;
  desc.sparseIndexBufferView = ~0u;
  desc.sparseDataBufferView = ~0u;

  desc.dataType.fromJson(j);

  j.at("count").get_to(desc.elementCount);

  if (j.count("bufferView"))
    j.at("bufferView").get_to(desc.bufferView);

  if (j.count("byteOffset"))
    j.at("byteOffset").get_to(desc.byteOffset);

  if (j.count("min"))
    j.at("min").get_to(minBounds);

  if (j.count("max"))
    j.at("max").get_to(maxBounds);

  if (j.count("sparse")) {
    json sparse = j.at("sparse");
    json sparseIndices = sparse.at("indices");
    json sparseValues = sparse.at("values");

    sparse.at("count").get_to(desc.sparseCount);

    sparseIndices.at("bufferView").get_to(desc.sparseIndexBufferView);
    sparseIndices.at("componentType").get_to(desc.sparseIndexType);

    if (sparseIndices.count("byteOffset"))
      sparseIndices.at("byteOffset").get_to(desc.sparseIndexBufferOffset);

    sparseValues.at("bufferView").get_to(desc.sparseDataBufferView);

    if (sparseValues.count("byteOffset"))
      sparseValues.at("byteOffset").get_to(desc.sparseDataBufferOffset);
  }

  for (size_t i = 0; i < std::min(minBounds.size(), desc.min.values.size()); i++)
    desc.min.values[i] = minBounds[i];

  for (size_t i = 0; i < std::min(maxBounds.size(), desc.max.values.size()); i++)
    desc.max.values[i] = maxBounds[i];
}


GltfAccessor::GltfAccessor(
        std::shared_ptr<GltfBufferView> bufferView,
        std::shared_ptr<GltfBufferView> sparseIndexView,
        std::shared_ptr<GltfBufferView> sparseElementView,
  const Desc&                         desc)
: m_dataType            (desc.dataType)
, m_elementCount        (desc.elementCount)
, m_bufferView          (std::move(bufferView))
, m_bufferOffset        (desc.byteOffset)
, m_bufferStride        (gltfComputeDataSize(m_dataType))
, m_minBounds           (desc.min)
, m_maxBounds           (desc.max)
, m_sparseElementCount  (desc.sparseCount)
, m_sparseIndexView     (std::move(sparseIndexView))
, m_sparseIndexType     (desc.sparseIndexType)
, m_sparseIndexOffset   (desc.sparseIndexBufferOffset)
, m_sparseIndexStride   (gltfComputeDataSize(m_sparseIndexType))
, m_sparseElementView   (std::move(sparseElementView))
, m_sparseElementOffset (desc.sparseDataBufferOffset) {

}


GltfAccessor::~GltfAccessor() {

}


bool GltfAccessor::getElementData(
        size_t                        element,
        void*                         dst) const {
  if (m_sparseElementCount) {
    // If the accessor has a sparse component, run a
    // binary search on the sparse indices to see if
    // there is any data for the requested element
    size_t lo = 0;
    size_t hi = m_sparseElementCount - 1;

    while (lo < hi) {
      size_t pivot = (lo + hi) / 2;
      uint32_t index = 0;

      if (!getSparseIndex(pivot, index))
        return false;

      if (index < element) {
        lo = pivot + 1;
      } else if (index > element) {
        hi = pivot;
      } else {
        lo = pivot;
        hi = pivot;
      }
    }

    if (lo == hi) {
      uint32_t index = 0;

      if (!getSparseIndex(lo, index))
        return false;

      if (index == element)
        return getSparseData(lo, dst);
    }
  }

  // If no sparse data is present, read from the base
  // buffer view, or return zeroes if there is none.
  if (m_bufferView != nullptr) {
    return m_bufferView->getElementData(element,
      m_bufferStride, m_bufferOffset, m_bufferStride, dst);
  }

  std::memset(dst, 0, m_bufferStride);
  return true;
}


bool GltfAccessor::getSparseIndex(
        size_t                        index,
        uint32_t&                     result) const {
  result = 0;

  return m_sparseIndexView->getElementData(index, m_sparseIndexStride,
    m_sparseIndexOffset, m_sparseIndexStride, &result);
}


bool GltfAccessor::getSparseData(
        size_t                        index,
        void*                         dst) const {
  return m_sparseElementView->getElementData(index, m_bufferStride,
    m_sparseElementOffset, m_bufferStride, dst);
}




void from_json(const json& j, GltfMaterial::Desc& desc) {
  desc = GltfMaterial::Desc();

  if (j.count("name"))
    j.at("name").get_to(desc.name);
}


GltfMaterial::GltfMaterial(
  const Desc&                         desc)
: m_name(desc.name) {

}


GltfMaterial::~GltfMaterial() {

}




GltfMorphTarget::GltfMorphTarget(
  const std::vector<std::shared_ptr<GltfAccessor>>& accessors,
  const std::string&                  name,
  const json&                         j)
: m_name(name) {
  std::unordered_map<std::string, uint32_t> attributes;
  j.get_to(attributes);

  for (const auto& a : attributes) {
    std::shared_ptr<GltfAccessor> accessor = accessors.at(a.second);
    m_attributes.emplace(a.first, std::move(accessor));
  }
}


GltfMorphTarget::~GltfMorphTarget() {

}


void from_json(const json& j, GltfMeshPrimitive::Desc& desc) {
  desc = GltfMeshPrimitive::Desc();
  desc.topology = GltfPrimitiveTopology::eTriangleList;
  desc.indices = ~0u;
  desc.attributes = j.at("attributes");
  desc.material = ~0u;

  if (j.count("name"))
    j.at("name").get_to(desc.name);

  if (j.count("indices"))
    j.at("indices").get_to(desc.indices);

  if (j.count("material"))
    j.at("material").get_to(desc.material);

  if (j.count("mode"))
    j.at("mode").get_to(desc.topology);

  if (j.count("targets"))
    j.at("targets").get_to(desc.targets);
}



GltfMeshPrimitive::GltfMeshPrimitive(
  const std::vector<std::shared_ptr<GltfAccessor>>& accessors,
  const std::vector<std::string>&     targetNames,
        std::shared_ptr<GltfMaterial> material,
  const Desc&                         desc)
: m_name              (desc.name)
, m_topology          (desc.topology)
, m_material          (std::move(material))
, m_indices           (desc.indices != ~0u ? accessors.at(desc.indices) : nullptr) {
  std::unordered_map<std::string, uint32_t> attributes;
  desc.attributes.get_to(attributes);

  for (const auto& a : attributes) {
    std::shared_ptr<GltfAccessor> accessor = accessors.at(a.second);
    m_attributes.emplace(a.first, std::move(accessor));
  }

  for (size_t i = 0; i < desc.targets.size(); i++) {
    std::string name = i < targetNames.size() ? targetNames.at(i) : strcat("target_", i);
    m_targets.emplace_back(std::make_shared<GltfMorphTarget>(accessors, name, desc.targets.at(i)));
  }
}


GltfMeshPrimitive::~GltfMeshPrimitive() {

}




void from_json(const json& j, GltfMesh::Desc& desc) {
  desc = GltfMesh::Desc();
  desc.primitives = j.at("primitives");

  if (j.count("name"))
    j.at("name").get_to(desc.name);

  if (j.count("extras")) {
    auto extras = j.at("extras");

    if (extras.count("asMesh"))
      extras.at("asMesh").get_to(desc.asMesh);

    if (extras.count("asMinDistance"))
      extras.at("asMinDistance").get_to(desc.asMinDistance);

    if (extras.count("asMaxDistance"))
      extras.at("asMaxDistance").get_to(desc.asMaxDistance);

    if (extras.count("targetNames"))
      extras.at("targetNames").get_to(desc.targetNames);
  }
}


GltfMesh::GltfMesh(
  const std::vector<std::shared_ptr<GltfAccessor>>& accessors,
  const std::vector<std::shared_ptr<GltfMaterial>>& materials,
  const Desc&                         desc)
: m_name        (desc.name)
, m_parentName  (desc.asMesh)
, m_minDistance (desc.asMinDistance)
, m_maxDistance (desc.asMaxDistance) {
  std::vector<GltfMeshPrimitive::Desc> primitives;
  desc.primitives.get_to(primitives);

  m_primitives.reserve(primitives.size());

  for (const auto& primitive : primitives) {
    auto material = primitive.material < materials.size() - 1
      ? materials.at(primitive.material)
      : materials.back();

    m_primitives.emplace_back(std::make_shared<GltfMeshPrimitive>(
      accessors, desc.targetNames, material, primitive));
  }
}


GltfMesh::~GltfMesh() {

}




void from_json(const json& j, GltfNode::Desc& desc) {
  desc = GltfNode::Desc();
  desc.mesh = ~0u;
  desc.skin = ~0u;
  desc.matrix = Matrix4x4::identity();
  desc.rotation = Vector4D(0.0f, 0.0f, 0.0f, 1.0f);
  desc.scale = Vector3D(1.0f, 1.0f, 1.0f);
  desc.translation = Vector3D(0.0f, 0.0f, 0.0f);

  if (j.count("name"))
    j.at("name").get_to(desc.name);

  if (j.count("mesh"))
    j.at("mesh").get_to(desc.mesh);

  if (j.count("children"))
    j.at("children").get_to(desc.children);

  if (j.count("matrix")) {
    std::array<float, 16> values;
    j.at("matrix").get_to(values);

    desc.matrix = Matrix4x4(
      Vector4D(values[ 0], values[ 1], values[ 2], values[ 3]),
      Vector4D(values[ 4], values[ 5], values[ 6], values[ 7]),
      Vector4D(values[ 8], values[ 9], values[10], values[11]),
      Vector4D(values[12], values[13], values[14], values[15]));
  }

  if (j.count("rotation")) {
    std::array<float, 4> values;
    j.at("rotation").get_to(values);

    desc.rotation = Vector4D(
      values[0], values[1],
      values[2], values[3]);
  }

  if (j.count("translation")) {
    std::array<float, 3> values;
    j.at("translation").get_to(values);

    desc.translation = Vector3D(
      values[0], values[1], values[2]);
  }

  if (j.count("scale")) {
    std::array<float, 3> values;
    j.at("scale").get_to(values);

    desc.scale = Vector3D(
      values[0], values[1], values[2]);
  }

  if (j.count("skin"))
    j.at("skin").get_to(desc.skin);
}


GltfNode::GltfNode(
        std::shared_ptr<GltfMesh>     mesh,
  const Desc&                         desc)
: m_name              (desc.name)
, m_mesh              (std::move(mesh))
, m_childNodeIndices  (desc.children)
, m_skinIndex         (desc.skin)
, m_matrix            (desc.matrix)
, m_rotation          (desc.rotation)
, m_scale             (desc.scale)
, m_translation       (desc.translation) {

}


GltfNode::~GltfNode() {

}


void GltfNode::setChildNodes(
  const std::vector<std::shared_ptr<GltfNode>>& nodes) {
  m_childNodes.resize(m_childNodeIndices.size());

  for (size_t i = 0; i < m_childNodeIndices.size(); i++) {
    m_childNodes[i] = nodes.at(m_childNodeIndices[i]);
    m_childNodes[i]->m_parent = weak_from_this();
  }
}


void GltfNode::setSkin(
  const std::vector<std::shared_ptr<GltfSkin>>& skins) {
  if (m_skinIndex < skins.size())
    m_skin = skins.at(m_skinIndex);
}


QuatTransform GltfNode::computeTransform() const {
  QuatTransform result;

  if (m_matrix != Matrix4x4::identity()) {
    Vector3D scale(
      length(m_matrix.col<0>()),
      length(m_matrix.col<1>()),
      length(m_matrix.col<2>()));

    Matrix4x4 matrix = Matrix4x4(
      m_matrix.col<0>() / scale.at<0>(),
      m_matrix.col<1>() / scale.at<1>(),
      m_matrix.col<2>() / scale.at<2>(),
      m_matrix.col<3>());

    QuatTransform transform = computeTransformFromMatrix(matrix);

    float uniformScale = std::max(std::max(
      scale.at<0>(), scale.at<1>()), scale.at<2>());

    result = QuatTransform(
      transform.getRotation() * std::sqrt(uniformScale),
      transform.getTranslation());
  } else {
    float uniformScale = std::max(std::max(
      std::abs(m_scale.at<0>()),
      std::abs(m_scale.at<1>())),
      std::abs(m_scale.at<2>()));

    result = QuatTransform(
      Quaternion(m_rotation) * std::sqrt(uniformScale),
      Vector4D(m_translation, 0.0f));
  }

  auto parent = m_parent.lock();

  if (parent != nullptr)
    result = parent->computeTransform().chain(result);

  return result;
}




void from_json(const json& j, GltfSkin::Desc& desc) {
  desc = GltfSkin::Desc();
  desc.inverseBindMatrices = ~0u;
  desc.skeleton = ~0u;

  j.at("joints").get_to(desc.joints);

  if (j.count("name"))
    j.at("name").get_to(desc.name);

  if (j.count("skeleton"))
    j.at("skeleton").get_to(desc.skeleton);

  if (j.count("inverseBindMatrices"))
    j.at("inverseBindMatrices").get_to(desc.inverseBindMatrices);
}


GltfSkin::GltfSkin(
  const std::vector<std::shared_ptr<GltfAccessor>>& accessors,
  const std::vector<std::shared_ptr<GltfNode>>& nodes,
  const Desc&                         desc)
: m_name(desc.name) {
  if (desc.inverseBindMatrices < accessors.size())
    m_inverseBindMatrices = accessors.at(desc.inverseBindMatrices);

  if (desc.skeleton < nodes.size())
    m_skeleton = nodes.at(desc.skeleton);

  for (uint32_t joint : desc.joints)
    m_joints.push_back(nodes.at(joint));
}


GltfSkin::~GltfSkin() {

}




Gltf::Gltf(
  const Io&                           io,
  const std::filesystem::path&        path) {
  IoFile file = io->open(path, IoOpenMode::eRead);

  if (!file)
    throw Error("Failed to open GLTF file");

  if (!readGlb(file) && !readJson(file))
    throw Error("Failed to read GLTF file");

  // Parse JSON string
  json j = json::parse(m_jsonString);

  if (!parseBuffers(j, io, path.parent_path())
   || !parseBufferViews(j)
   || !parseAccessors(j)
   || !parseMaterials(j)
   || !parseMeshes(j)
   || !parseNodes(j)
   || !parseSkins(j))
    throw Error("Failed to parse GLTF file");
}


Gltf::~Gltf() {

}


bool Gltf::readGlb(
  const IoFile&                       file) {
  RdFileStream stream(file);
  RdStream reader(stream);

  // Read the file header and check if it is actual
  // a valid GLB container, exit early if not.
  GlbHeader header = { };

  if (!reader.read(header))
    return false;

  if (header.magic != FourCC('g', 'l', 'T', 'F'))
    return false;

  if (header.version != 2)
    throw Error("Unsupported GLB container version");

  // The JSON chunk is not optional and must occur at the start
  GlbChunk jsonChunk = { };

  if (!reader.read(jsonChunk))
    throw Error("Failed to read GLB chunk header");

  if (jsonChunk.chunkType != FourCC('J', 'S', 'O', 'N'))
    throw Error("First chunk of GLB file is not a JSON chunk");

  m_jsonString.resize(jsonChunk.chunkLength);

  if (!reader.read(m_jsonString.data(), m_jsonString.size()))
    throw Error("Failed to read GLB JSON chunk");

  // The embedded binary chunk is optional
  GlbChunk binChunk = { };

  if (!reader.read(binChunk))
    return true;

  if (binChunk.chunkType != FourCC('B', 'I', 'N', '\0'))
    return true;

  // If a binary chunk is present, it stores data for
  // the very first buffer in the buffers array
  auto buffer = std::make_shared<GltfBuffer>();
  
  if (!buffer->readFromStream(stream, binChunk.chunkLength))
    throw Error("Failed to read GLB BIN chunk");

  m_buffers.push_back(buffer);
  return true;
}


bool Gltf::readJson(
  const IoFile&                       file) {
  m_jsonString.resize(file->getSize());

  IoStatus status = file->read(0,
    m_jsonString.size(),
    m_jsonString.data());

  return status == IoStatus::eSuccess;
}


bool Gltf::parseBuffers(
  const json&                         j,
  const Io&                           io,
  const std::filesystem::path&        parentDirectory) {
  if (!j.count("buffers"))
    return true;

  std::vector<GltfBuffer::Desc> buffers;
  j.at("buffers").get_to(buffers);

  m_buffers.resize(buffers.size());

  for (size_t i = 0; i < buffers.size(); i++) {
    if (m_buffers[i] == nullptr)
      m_buffers[i] = std::make_shared<GltfBuffer>(buffers[i]);

    if (!buffers[i].uri.empty()) {
      static const std::array<std::string_view, 2> base64Prefixes = {{
        "data:application/octet-stream;base64,",
        "data:application/gltf-buffer;base64,",
      }};

      bool isEmbedded = false;

      for (auto prefix : base64Prefixes) {
        if (buffers[i].uri.starts_with(prefix)) {
          std::string_view string = buffers[i].uri;
          string.remove_prefix(prefix.size());
          m_buffers[i]->readFromString(string, m_buffers[i]->getSize());
          
          isEmbedded = true;
          break;
        }
      }

      if (!isEmbedded) {
        std::filesystem::path path = buffers[i].uri;

        if (path.is_relative())
          path = parentDirectory / path;

        IoFile file = io->open(path, IoOpenMode::eRead);

        if (!file)
          throw Error("Failed to open buffer file");

        if (!m_buffers[i]->readFromFile(file, 0, m_buffers[i]->getSize()))
          throw Error("Failed to read buffer file");
      }
    } else {
      m_buffers[i]->setSize(buffers[i].length);

      if (m_buffers[i]->getSize() > m_buffers[i]->getDataSize())
        throw Error("Embedded buffer data size mismatch");
    }
  }

  return true;
}


bool Gltf::parseBufferViews(
  const json&                         j) {
  if (!j.count("bufferViews"))
    return true;

  std::vector<GltfBufferView::Desc> bufferViews;
  j.at("bufferViews").get_to(bufferViews);

  m_bufferViews.reserve(bufferViews.size());

  for (const auto& view : bufferViews) {
    m_bufferViews.emplace_back(std::make_shared<GltfBufferView>(
      m_buffers.at(view.bufferIndex), view));
  }
  
  return true;
}


bool Gltf::parseAccessors(
  const json&                         j) {
  if (!j.count("accessors"))
    return true;

  std::vector<GltfAccessor::Desc> accessors;
  j.at("accessors").get_to(accessors);

  m_accessors.reserve(accessors.size());

  for (const auto& accessor : accessors) {
    std::shared_ptr<GltfBufferView> bufferView;
    std::shared_ptr<GltfBufferView> sparseIndexView;
    std::shared_ptr<GltfBufferView> sparseElementView;

    if (accessor.bufferView < ~0u)
      bufferView = m_bufferViews.at(accessor.bufferView);

    if (accessor.sparseCount) {
      sparseIndexView = m_bufferViews.at(accessor.sparseIndexBufferView);
      sparseElementView = m_bufferViews.at(accessor.sparseDataBufferView);
    }

    m_accessors.emplace_back(std::make_shared<GltfAccessor>(
      std::move(bufferView), std::move(sparseIndexView),
      std::move(sparseElementView), accessor));
  }

  return true;
}


bool Gltf::parseMaterials(
  const json&                         j) {
  std::vector<GltfMaterial::Desc> materials;

  if (j.count("materials"))
    j.at("materials").get_to(materials);

  // Add a default material for meshes
  // that do not define one
  materials.emplace_back().name = "default";

  for (const auto& material : materials)
    m_materials.emplace_back(std::make_shared<GltfMaterial>(material));

  return true;
}


bool Gltf::parseMeshes(
  const json&                         j) {
  if (!j.count("meshes"))
    return true;

  std::vector<GltfMesh::Desc> meshes;
  j.at("meshes").get_to(meshes);

  m_meshes.reserve(meshes.size());

  for (const auto& mesh : meshes)
    m_meshes.emplace_back(std::make_shared<GltfMesh>(m_accessors, m_materials, mesh));

  return true;
}


bool Gltf::parseNodes(
  const json&                         j) {
  if (!j.count("nodes"))
    return true;

  std::vector<GltfNode::Desc> nodes;
  j.at("nodes").get_to(nodes);

  m_nodes.reserve(nodes.size());

  for (const auto& node : nodes) {
    std::shared_ptr<GltfMesh> mesh;

    if (node.mesh != ~0u)
      mesh = m_meshes.at(node.mesh);

    m_nodes.emplace_back(std::make_shared<GltfNode>(std::move(mesh), node));
  }

  for (const auto& node : m_nodes)
    node->setChildNodes(m_nodes);

  return true;
}


bool Gltf::parseSkins(
  const json&                         j) {
  if (!j.count("skins"))
    return true;

  std::vector<GltfSkin::Desc> skins;
  j.at("skins").get_to(skins);

  m_skins.reserve(skins.size());

  for (const auto& skin : skins) {
    m_skins.emplace_back(std::make_shared<GltfSkin>(
      m_accessors, m_nodes, skin));
  }

  for (const auto& node : m_nodes)
    node->setSkin(m_skins);

  return true;
}

}
