#include "../gfx.h"

#include "gfx_vulkan_device.h"
#include "gfx_vulkan_ray_tracing.h"
#include "gfx_vulkan_utils.h"

namespace as {

GfxVulkanRayTracingBvhInfo::GfxVulkanRayTracingBvhInfo(
  const GfxVulkanDevice&              device,
  const GfxRayTracingGeometryDesc&    desc)
: info { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR } {
  info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
  info.flags = getVkAccelerationStructureFlags(desc.flags);
  info.geometryCount = desc.geometries.size();

  geometries.reserve(info.geometryCount);

  for (uint32_t i = 0; i < info.geometryCount; i++) {
    const auto& geometryDesc = desc.geometries[i];

    VkAccelerationStructureGeometryKHR geometry = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
    geometry.flags = getVkGeometryFlags(desc.geometries[i].opacity);

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo = { };

    switch (geometryDesc.type) {
      case GfxRayTracingGeometryType::eMesh: {
        geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;

        geometry.geometry.triangles = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR };
        geometry.geometry.triangles.vertexFormat = device.getVkFormat(geometryDesc.data.mesh.vertexFormat);
        geometry.geometry.triangles.vertexStride = geometryDesc.data.mesh.vertexStride;
        geometry.geometry.triangles.vertexData.deviceAddress = geometryDesc.data.mesh.vertexOffset;
        geometry.geometry.triangles.maxVertex = geometryDesc.data.mesh.vertexCount - 1;
        geometry.geometry.triangles.indexType = getVkIndexType(geometryDesc.data.mesh.indexFormat);

        if (geometryDesc.flags & GfxRayTracingGeometryFlag::eMeshTransform) {
          // As per spect, vkGetAccelerationStructureBuildSizes only checks whether
          // this is null, the pointer does not have to be valid at this time.
          geometry.geometry.triangles.transformData.hostAddress = reinterpret_cast<const void*>(16);
        }

        rangeInfo.primitiveCount = geometry.geometry.triangles.indexType == VK_INDEX_TYPE_NONE_KHR
          ? geometryDesc.data.mesh.vertexCount / 3
          : geometryDesc.data.mesh.indexCount / 3;
      } break;

      case GfxRayTracingGeometryType::eAabb: {
        geometry.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;
        
        geometry.geometry.aabbs = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR };
        geometry.geometry.aabbs.stride = sizeof(GfxAabb<float>);

        rangeInfo.primitiveCount = geometryDesc.data.aabb.boundingBoxCount;
      } break;
    }

    geometries.push_back(geometry);
    rangeInfos.push_back(rangeInfo);
  }

  fixupPointers();
}


GfxVulkanRayTracingBvhInfo::GfxVulkanRayTracingBvhInfo(
  const GfxVulkanDevice&              device,
  const GfxRayTracingInstanceDesc&    desc)
: info { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR } {
  info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
  info.flags = getVkAccelerationStructureFlags(desc.flags);
  info.geometryCount = desc.instances.size();

  geometries.reserve(info.geometryCount);

  for (uint32_t i = 0; i < info.geometryCount; i++) {
    VkAccelerationStructureGeometryKHR geometry = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
    geometry.flags = getVkGeometryFlags(desc.instances[i].opacity);
    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;

    geometry.geometry.instances = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR };
    geometry.geometry.instances.arrayOfPointers = VK_FALSE;

    geometries.push_back(geometry);

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo = { };
    rangeInfo.primitiveCount = desc.instances[i].instanceCount;

    rangeInfos.push_back(rangeInfo);
  }

  fixupPointers();
}


GfxVulkanRayTracingBvhInfo::GfxVulkanRayTracingBvhInfo(GfxVulkanRayTracingBvhInfo&& other)
: info        (other.info)
, geometries  (std::move(other.geometries))
, rangeInfos  (std::move(other.rangeInfos)) {
  fixupPointers();
}


GfxVulkanRayTracingBvhInfo::GfxVulkanRayTracingBvhInfo(const GfxVulkanRayTracingBvhInfo& other)
: info        (other.info)
, geometries  (other.geometries)
, rangeInfos  (other.rangeInfos) {
  fixupPointers();
}


GfxVulkanRayTracingBvhInfo& GfxVulkanRayTracingBvhInfo::operator = (GfxVulkanRayTracingBvhInfo&& other) {
  info = other.info;
  geometries = std::move(other.geometries);
  rangeInfos = std::move(other.rangeInfos);

  fixupPointers();
  return *this;
}


GfxVulkanRayTracingBvhInfo& GfxVulkanRayTracingBvhInfo::operator = (const GfxVulkanRayTracingBvhInfo&& other) {
  info = other.info;
  geometries = other.geometries;
  rangeInfos = other.rangeInfos;

  fixupPointers();
  return *this;
}


void GfxVulkanRayTracingBvhInfo::fixupPointers() {
  info.pGeometries = info.geometryCount ? geometries.data() : nullptr;
}




GfxVulkanRayTracingBvh::GfxVulkanRayTracingBvh(
        GfxVulkanDevice&              device,
  const GfxRayTracingBvhDesc&         desc,
        GfxVulkanRayTracingBvhInfo&&  info,
  const GfxVulkanRayTracingBvhSize&   size,
        VkBuffer                      buffer,
        VkAccelerationStructureKHR    rtas,
        VkDeviceAddress               va,
        GfxVulkanMemorySlice&&        memory)
: GfxRayTracingBvhIface(desc, va)
, m_device  (device)
, m_info    (std::move(info))
, m_size    (size)
, m_memory  (std::move(memory))
, m_buffer  (buffer)
, m_rtas    (rtas) {
  m_device.setDebugName(m_buffer, desc.debugName);
  m_device.setDebugName(m_rtas, desc.debugName);
}


GfxVulkanRayTracingBvh::~GfxVulkanRayTracingBvh() {
  auto& vk = m_device.vk();

  vk.vkDestroyAccelerationStructureKHR(vk.device, m_rtas, nullptr);
  vk.vkDestroyBuffer(vk.device, m_buffer, nullptr);
}


uint64_t GfxVulkanRayTracingBvh::getScratchSize(
      GfxRayTracingBvhBuildMode     mode) const {
  return mode == GfxRayTracingBvhBuildMode::eBuild
    ? m_size.scratchSizeForBuild
    : m_size.scratchSizeForUpdate;
}


GfxVulkanRayTracingBvhInfo GfxVulkanRayTracingBvh::getBuildInfo(
      GfxRayTracingBvhBuildMode     mode,
const GfxRayTracingBvhData*         data,
      uint64_t                      scratch) const {
  GfxVulkanRayTracingBvhInfo info = m_info;
  info.info.mode = getVkBuildMode(mode);
  info.info.scratchData.deviceAddress = scratch;
  info.info.dstAccelerationStructure = m_rtas;

  if (mode == GfxRayTracingBvhBuildMode::eUpdate)
    info.info.srcAccelerationStructure = m_rtas;

  for (uint32_t i = 0; i < info.info.geometryCount; i++) {
    auto& geometry = info.geometries[i];
    auto& rangeInfo = info.rangeInfos[i];

    switch (geometry.geometryType) {
      case VK_GEOMETRY_TYPE_TRIANGLES_KHR: {
        bool hasTransformMatrix = geometry.geometry.triangles.transformData.hostAddress != nullptr;
        geometry.geometry.triangles.vertexData.deviceAddress += data[i].mesh.vertexData;

        if (geometry.geometry.triangles.indexType != VK_INDEX_TYPE_NONE_KHR)
          geometry.geometry.triangles.indexData.deviceAddress = data[i].mesh.indexData;

        if (hasTransformMatrix) {
          geometry.geometry.triangles.transformData = VkDeviceOrHostAddressConstKHR();
          geometry.geometry.triangles.transformData.deviceAddress = data[i].mesh.transformData;
        }

        rangeInfo.primitiveOffset = data[i].mesh.firstIndex * getVkIndexSize(geometry.geometry.triangles.indexType);
        rangeInfo.firstVertex = data[i].mesh.firstVertex;
      } break;

      case VK_GEOMETRY_TYPE_AABBS_KHR: {
        geometry.geometry.aabbs.data.deviceAddress = data[i].aabb.boundingBoxData;
      } break;

      case VK_GEOMETRY_TYPE_INSTANCES_KHR: {
        geometry.geometry.instances.arrayOfPointers = VK_FALSE;
        geometry.geometry.instances.data.deviceAddress = data[i].instances.instanceData;
      } break;

      default:
        dbg_unreachable("Invalid geometry type");
    }
  }

  return info;
}


GfxDescriptor GfxVulkanRayTracingBvh::getDescriptor() const {
  GfxVulkanDescriptor descriptor;
  descriptor.bvh = m_rtas;

  return exportVkDescriptor(descriptor);
}

}
