#include "../../util/util_small_vector.h"

#include "gfx_vulkan_descriptor_array.h"
#include "gfx_vulkan_utils.h"

namespace as {

GfxVulkanDescriptorArray::GfxVulkanDescriptorArray(
        GfxVulkanDevice&              device,
  const GfxDescriptorArrayDesc&       desc)
: GfxDescriptorArrayIface(desc)
, m_device  (device)
, m_type    (getVkDescriptorType(desc.bindingType))
, m_size    (desc.descriptorCount) {
  auto& vk = m_device.vk();

  VkDescriptorPoolSize poolSize;
  poolSize.type = m_type;
  poolSize.descriptorCount = m_size;

  VkDescriptorPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
  poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
  poolInfo.maxSets = 1;
  poolInfo.poolSizeCount = 1;
  poolInfo.pPoolSizes = &poolSize;

  VkResult vr = vk.vkCreateDescriptorPool(vk.device, &poolInfo, nullptr, &m_pool);

  if (vr)
    throw VulkanError("Vulkan: Failed to create descriptor pool", vr);

  VkDescriptorSetLayout setLayout = m_device.getPipelineManager().getDescriptorArrayLayout(desc.bindingType)->getSetLayout();

  VkDescriptorSetVariableDescriptorCountAllocateInfo countInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO };
  countInfo.descriptorSetCount = 1;
  countInfo.pDescriptorCounts = &m_size;

  VkDescriptorSetAllocateInfo setInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, &countInfo };
  setInfo.descriptorPool = m_pool;
  setInfo.descriptorSetCount = 1;
  setInfo.pSetLayouts = &setLayout;

  vr = vk.vkAllocateDescriptorSets(vk.device, &setInfo, &m_set);

  if (vr) {
    vk.vkDestroyDescriptorPool(vk.device, m_pool, nullptr);
    throw VulkanError("Vulkan: Failed to allocate descriptor array", vr);
  }

  m_device.setDebugName(m_set, desc.debugName);

  // Null descriptors do not support samplers, so create a dummy sampler
  if (m_type == VK_DESCRIPTOR_TYPE_SAMPLER) {
    VkSamplerCreateInfo info = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    info.magFilter = VK_FILTER_LINEAR;
    info.minFilter = VK_FILTER_LINEAR;
    info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.minLod = -std::numeric_limits<float>::max();
    info.maxLod = std::numeric_limits<float>::max();

    vr = vk.vkCreateSampler(vk.device, &info, nullptr, &m_sampler);

    if (vr)
      throw VulkanError("Vulkan: Failed to create dummy sampler", vr);

    m_device.setDebugName(m_sampler, "null");
  }

  // Explicitly initialize the descriptor set with null
  // descriptors so that accessing unwritten descriptors
  // does not cause a GPU crash.
  std::vector<GfxDescriptor> descriptors(m_size);
  setDescriptors(0, descriptors.size(), descriptors.data());
}


GfxVulkanDescriptorArray::~GfxVulkanDescriptorArray() {
  auto& vk = m_device.vk();

  vk.vkDestroyDescriptorPool(vk.device, m_pool, nullptr);
  vk.vkDestroySampler(vk.device, m_sampler, nullptr);
}


void GfxVulkanDescriptorArray::setDescriptors(
        uint32_t                      index,
        uint32_t                      count,
  const GfxDescriptor*                descriptors) {
  auto& vk = m_device.vk();

  VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
  write.dstSet = m_set;
  write.dstBinding = 0;
  write.dstArrayElement = index;
  write.descriptorCount = count;
  write.descriptorType = m_type;

  switch (m_type) {
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: {
      small_vector<VkDescriptorBufferInfo, 64> bufferInfos(count);

      for (uint32_t i = 0; i < count; i++)
        bufferInfos[i] = importVkDescriptor(descriptors[i]).buffer;

      write.pBufferInfo = bufferInfos.data();
      vk.vkUpdateDescriptorSets(vk.device, 1, &write, 0, nullptr);
    } break;

    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: {
      small_vector<VkBufferView, 64> bufferViews(count);

      for (uint32_t i = 0; i < count; i++)
        bufferViews[i] = importVkDescriptor(descriptors[i]).bufferView;

      write.pTexelBufferView = bufferViews.data();
      vk.vkUpdateDescriptorSets(vk.device, 1, &write, 0, nullptr);
    } break;

    case VK_DESCRIPTOR_TYPE_SAMPLER: {
      small_vector<VkDescriptorImageInfo, 64> samplerInfos(count);

      for (uint32_t i = 0; i < count; i++) {
        samplerInfos[i] = importVkDescriptor(descriptors[i]).image;

        if (!samplerInfos[i].sampler)
          samplerInfos[i].sampler = m_sampler;
      }

      write.pImageInfo = samplerInfos.data();
      vk.vkUpdateDescriptorSets(vk.device, 1, &write, 0, nullptr);
    } break;

    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE: {
      small_vector<VkDescriptorImageInfo, 64> imageInfos(count);

      for (uint32_t i = 0; i < count; i++)
        imageInfos[i] = importVkDescriptor(descriptors[i]).image;

      write.pImageInfo = imageInfos.data();
      vk.vkUpdateDescriptorSets(vk.device, 1, &write, 0, nullptr);
    } break;

    case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR: {
      small_vector<VkAccelerationStructureKHR, 64> accelerationStructures(count);

      for (uint32_t i = 0; i < count; i++)
        accelerationStructures[i] = importVkDescriptor(descriptors[i]).bvh;

      VkWriteDescriptorSetAccelerationStructureKHR writeRtas = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
      writeRtas.accelerationStructureCount = accelerationStructures.size();
      writeRtas.pAccelerationStructures = accelerationStructures.data();

      write.pNext = &writeRtas;
      vk.vkUpdateDescriptorSets(vk.device, 1, &write, 0, nullptr);
    } break;

    default:
      return;
  }
}

}
