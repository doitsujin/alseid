#include <algorithm>
#include <queue>
#include <unordered_map>

#include "gfx.h"
#include "gfx_geometry.h"

namespace as {

const GfxMeshLodMetadata* GfxGeometry::getLod(
  const GfxMeshMetadata*              mesh,
        uint32_t                      lod) const {
  if (lod >= mesh->info.lodCount)
    return nullptr;

  uint absoluteIndex = mesh->lodMetadataIndex + lod;

  if (absoluteIndex >= lods.size())
    return nullptr;

  return &lods[absoluteIndex];
}


uint32_t GfxGeometry::getMeshletVertexDataOffset(
  const GfxMeshMetadata*              mesh,
  const GfxMeshLodMetadata*           lod,
        uint32_t                      meshlet) const {
  if (meshlet >= lod->info.meshletCount)
    return 0;

  uint absoluteIndex = lod->firstMeshletIndex + meshlet;

  if (absoluteIndex >= meshletOffsets.size())
    return 0;

  return meshletOffsets[absoluteIndex];
}


const GfxJointMetadata* GfxGeometry::getJoint(
        uint32_t                      joint) const {
  if (joint >= joints.size())
    return nullptr;

  return &joints[joint];
}


const GfxMeshMetadata* GfxGeometry::findMesh(
  const char*                         name) const {
  for (const auto& mesh : meshes) {
    if (mesh.name == name)
      return &mesh;
  }

  return nullptr;
}


const GfxMeshInstanceMetadata* GfxGeometry::findInstance(
  const char*                         name) const {
  for (const auto& instance : instances) {
    if (instance.name == name)
      return &instance;
  }

  return nullptr;
}


const GfxMeshMaterialMetadata* GfxGeometry::findMaterial(
  const char*                         name) const {
  for (const auto& material : materials) {
    if (material.name == name)
      return &material;
  }

  return nullptr;
}


const GfxMeshletAttributeMetadata* GfxGeometry::findAttribute(
  const GfxMeshMaterialMetadata*      material,
  const char*                         name) const {
  for (uint32_t i = 0; i < material->attributeCount; i++) {
    const auto* attribute = &attributes[i + material->attributeIndex];

    if (attribute->name == name)
      return attribute;
  }

  return nullptr;
}


const GfxMeshletAttributeMetadata* GfxGeometry::findAttribute(
  const GfxMeshMaterialMetadata*      material,
        GfxMeshletAttributeSemantic   semantic,
        uint16_t                      index) const {
  for (uint32_t i = 0; i < material->attributeCount; i++) {
    const auto* attribute = &attributes[i + material->attributeIndex];

    if (attribute->semantic == semantic
     && attribute->semanticIndex == index)
      return attribute;
  }

  return nullptr;
}


const GfxJointMetadata* GfxGeometry::findJoint(
  const char*                         name) const {
  for (const auto& joint : joints) {
    if (joint.name == name)
      return &joint;
  }

  return nullptr;
}


const GfxMorphTargetMetadata* GfxGeometry::findMorphTarget(
  const char*                         name) const {
  for (const auto& morphTarget : morphTargets) {
    if (morphTarget.name == name)
      return &morphTarget;
  }

  return nullptr;
}


bool GfxGeometry::serialize(
        WrBufferedStream&             output) {
  WrStream stream(output);
  bool success = true;

  // Version number, currently always 0.
  uint16_t version = 0;

  // Write out header
  success &= stream.write(version)
          && stream.write(info);

  // Write out meshes. The mesh count can be inferred from the
  // geometry metadata structure and is not explicitly stored.
  for (const auto& mesh : meshes) {
    success &= stream.write(mesh.name)
            && stream.write(mesh.info)
            && stream.write(uint16_t(mesh.lodMetadataIndex))
            && stream.write(uint16_t(mesh.instanceDataIndex));
  }

  // Write out LOD array. We need to also store the total LOD count
  // here if we don't want this to get annoying during deserialization.
  success &= stream.write(uint16_t(lods.size()));

  for (const auto& lod : lods) {
    success &= stream.write(lod.info)
            && stream.write(lod.firstMeshletIndex);
  }

  // Write out instance data array. Same applies here, we need to
  // explicitly store the length of the array.
  success &= stream.write(uint16_t(instances.size()));

  for (const auto& instance : instances) {
    success &= stream.write(instance.name)
            && stream.write(instance.info)
            && stream.write(uint16_t(instance.meshIndex))
            && stream.write(uint16_t(instance.instanceIndex));
  }

  // Write out meshlet metadata in the order it occurs in the CPU array.
  success &= stream.write(uint32_t(meshletOffsets.size()));

  for (uint32_t meshlet : meshletOffsets)
    success &= stream.write(meshlet);

  // Write out material metadata. The material count can be
  // inferred from the overall geometry metadata structure.
  for (const auto& material : materials) {
    success &= stream.write(material.name)
            && stream.write(uint16_t(material.attributeIndex))
            && stream.write(uint16_t(material.attributeCount))
            && stream.write(uint16_t(material.vertexDataStride))
            && stream.write(uint16_t(material.shadingDataStride))
            && stream.write(uint16_t(material.morphDataStride));
  }

  // Write out attributes, as well as the count.
  success &= stream.write(uint16_t(attributes.size()));

  for (const auto& attribute : attributes) {
    success &= stream.write(attribute.name)
            && stream.write(uint16_t(attribute.dataFormat))
            && stream.write(uint16_t(attribute.stream))
            && stream.write(uint16_t(attribute.semantic))
            && stream.write(uint16_t(attribute.semanticIndex))
            && stream.write(uint16_t(attribute.dataOffset))
            && stream.write(uint8_t(attribute.morph))
            && stream.write(uint16_t(attribute.morphOffset));
  }

  // Write out joints. The number of joints can be
  // inferred from the geometry metadata structure.
  for (const auto& joint : joints) {
    success &= stream.write(joint.name)
            && stream.write(joint.info);
  }

  // Write out morph target names. The number of morph
  // targets is stored in the geometry metadata structure.
  for (const auto& morphTarget : morphTargets)
    success &= stream.write(morphTarget.name);

  // Write out animation metadata.
  success &= stream.write(uint16_t(animations.size()));

  for (const auto& animation : animations) {
    success &= stream.write(animation.name)
            && stream.write(uint16_t(animation.groupIndex))
            && stream.write(uint16_t(animation.groupCount))
            && stream.write(animation.duration);
  }

  return success;
}


bool GfxGeometry::deserialize(
        RdMemoryView                  input) {
  RdStream reader(input);

  // Decode version number and error out if it is not supported
  uint16_t version = 0;

  if (!reader.read(version) || version != 0)
    return false;

  // Read geometry metadata
  if (!reader.read(info))
    return false;

  // Read mesh metadata
  meshes.resize(info.meshCount);
  uint32_t meshIndex = 0;

  for (auto& mesh : meshes) {
    if (!reader.read(mesh.name)
     || !reader.read(mesh.info)
     || !reader.readAs<uint16_t>(mesh.lodMetadataIndex)
     || !reader.readAs<uint16_t>(mesh.instanceDataIndex))
      return false;

    mesh.meshIndex = meshIndex++;
  }

  // Read LOD count and metadata
  size_t lodCount = 0;

  if (!reader.readAs<uint16_t>(lodCount))
    return false;

  lods.resize(lodCount);

  for (auto& lod : lods) {
    if (!reader.read(lod.info)
     || !reader.read(lod.firstMeshletIndex))
      return false;
  }

  // Read mesh instance data
  size_t instanceCount = 0;

  if (!reader.readAs<uint16_t>(instanceCount))
    return false;

  instances.resize(instanceCount);

  for (auto& instance : instances) {
    if (!reader.read(instance.name)
     || !reader.read(instance.info)
     || !reader.readAs<uint16_t>(instance.meshIndex)
     || !reader.readAs<uint16_t>(instance.instanceIndex))
      return false;
  }

  // Read meshlet metadata
  size_t meshletCount = 0;

  if (!reader.readAs<uint32_t>(meshletCount))
    return false;

  meshletOffsets.resize(meshletCount);

  for (auto& meshlet : meshletOffsets) {
    if (!reader.read(meshlet))
      return false;
  }

  // Read material metadata
  materials.resize(info.materialCount);
  uint32_t materialIndex = 0;

  for (auto& material : materials) {
    if (!reader.read(material.name)
     || !reader.readAs<uint16_t>(material.attributeIndex)
     || !reader.readAs<uint16_t>(material.attributeCount)
     || !reader.readAs<uint16_t>(material.vertexDataStride)
     || !reader.readAs<uint16_t>(material.shadingDataStride)
     || !reader.readAs<uint16_t>(material.morphDataStride))
      return false;

    material.materialIndex = materialIndex++;
  }

  // Read material attributes
  size_t attributeCount = 0;

  if (!reader.readAs<uint16_t>(attributeCount))
    return false;

  attributes.resize(attributeCount);

  for (auto& attribute : attributes) {
    if (!reader.read(attribute.name)
     || !reader.readAs<uint16_t>(attribute.dataFormat)
     || !reader.readAs<uint16_t>(attribute.stream)
     || !reader.readAs<uint16_t>(attribute.semantic)
     || !reader.readAs<uint16_t>(attribute.semanticIndex)
     || !reader.readAs<uint16_t>(attribute.dataOffset)
     || !reader.readAs<uint8_t>(attribute.morph)
     || !reader.readAs<uint16_t>(attribute.morphOffset))
      return false;
  }

  // Read joint metadata
  joints.resize(info.jointCount);
  uint32_t jointIndex = 0;

  for (auto& joint : joints) {
    if (!reader.read(joint.name)
     || !reader.read(joint.info))
      return false;

    joint.jointIndex = jointIndex++;
  }

  // Read morph target metadata
  morphTargets.resize(info.morphTargetCount);
  uint32_t morphTargetIndex = 0;

  for (auto& morphTarget : morphTargets) {
    if (!reader.read(morphTarget.name))
      return false;

    morphTarget.morphTargetIndex = morphTargetIndex++;
  }

  // Read animation metadata
  uint32_t animationCount = 0u;

  if (!reader.readAs<uint16_t>(animationCount))
    return false;

  animations.resize(animationCount);
  uint32_t animationIndex = 0;

  for (auto& animation : animations) {
    if (!reader.read(animation.name)
     || !reader.readAs<uint16_t>(animation.groupIndex)
     || !reader.readAs<uint16_t>(animation.groupCount)
     || !reader.read(animation.duration))
      return false;

    animation.animationIndex = animationIndex++;
  }

  return true;
}

}
