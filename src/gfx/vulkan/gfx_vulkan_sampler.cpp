#include "gfx_vulkan_sampler.h"
#include "gfx_vulkan_utils.h"

namespace as {

GfxVulkanSampler::GfxVulkanSampler(
        std::shared_ptr<GfxVulkanDevice> device,
  const GfxSamplerDesc&               desc)
: GfxSamplerIface(desc)
, m_device(std::move(device)) {
  auto& vk = m_device->vk();

  VkSamplerCreateInfo info = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
  info.magFilter = getVkFilter(desc.magFilter);
  info.minFilter = getVkFilter(desc.minFilter);
  info.mipmapMode = getVkMipmapMode(desc.mipFilter);
  info.addressModeU = getVkAddressMode(desc.addressModeU);
  info.addressModeV = getVkAddressMode(desc.addressModeV);
  info.addressModeW = getVkAddressMode(desc.addressModeW);
  info.mipLodBias = desc.lodBias;
  info.anisotropyEnable = desc.anisotropy > 1;
  info.maxAnisotropy = float(desc.anisotropy);
  info.compareEnable = desc.type == GfxSamplerType::eDepthCompare;
  info.compareOp = getVkCompareOp(desc.compareOp);
  info.minLod = desc.minLod;
  info.maxLod = desc.maxLod;
  info.borderColor = getVkBorderColor(desc.borderColor);

  VkResult vr = vk.vkCreateSampler(vk.device, &info, nullptr, &m_sampler);

  if (vr)
    throw VulkanError("Vulkan: Failed to create sampler", vr);

  m_device->setDebugName(m_sampler, desc.debugName);
}


GfxVulkanSampler::~GfxVulkanSampler() {
  auto& vk = m_device->vk();

  vk.vkDestroySampler(vk.device, m_sampler, nullptr);
}


GfxDescriptor GfxVulkanSampler::getDescriptor() const {
  GfxVulkanDescriptor descriptor = { };
  descriptor.image.sampler = m_sampler;

  return exportVkDescriptor(descriptor);
}

}
