#include "gfx_vulkan_format.h"

namespace as {

GfxVulkanFormatMap::GfxVulkanFormatMap() {
  map(GfxFormat::eUnknown, VK_FORMAT_UNDEFINED);
  map(GfxFormat::eR4G4B4A4un, VK_FORMAT_A4B4G4R4_UNORM_PACK16);
  map(GfxFormat::eR8un, VK_FORMAT_R8_UNORM);
  map(GfxFormat::eR8sn, VK_FORMAT_R8_SNORM);
  map(GfxFormat::eR8ui, VK_FORMAT_R8_UINT);
  map(GfxFormat::eR8si, VK_FORMAT_R8_SINT);
  map(GfxFormat::eR8G8un, VK_FORMAT_R8G8_UNORM);
  map(GfxFormat::eR8G8sn, VK_FORMAT_R8G8_SNORM);
  map(GfxFormat::eR8G8ui, VK_FORMAT_R8G8_UINT);
  map(GfxFormat::eR8G8si, VK_FORMAT_R8G8_SINT);
  map(GfxFormat::eR8G8B8A8un, VK_FORMAT_R8G8B8A8_UNORM);
  map(GfxFormat::eR8G8B8A8sn, VK_FORMAT_R8G8B8A8_SNORM);
  map(GfxFormat::eR8G8B8A8ui, VK_FORMAT_R8G8B8A8_UINT);
  map(GfxFormat::eR8G8B8A8si, VK_FORMAT_R8G8B8A8_SINT);
  map(GfxFormat::eR8G8B8A8srgb, VK_FORMAT_R8G8B8A8_SRGB);
  map(GfxFormat::eB8G8R8A8un, VK_FORMAT_B8G8R8A8_UNORM);
  map(GfxFormat::eB8G8R8A8sn, VK_FORMAT_B8G8R8A8_SNORM);
  map(GfxFormat::eB8G8R8A8ui, VK_FORMAT_B8G8R8A8_UINT);
  map(GfxFormat::eB8G8R8A8si, VK_FORMAT_B8G8R8A8_SINT);
  map(GfxFormat::eB8G8R8A8srgb, VK_FORMAT_B8G8R8A8_SRGB);
  map(GfxFormat::eR9G9B9E5f, VK_FORMAT_E5B9G9R9_UFLOAT_PACK32);
  map(GfxFormat::eR10G10B10A2un, VK_FORMAT_A2B10G10R10_UNORM_PACK32);
  map(GfxFormat::eR10G10B10A2ui, VK_FORMAT_A2B10G10R10_UINT_PACK32);
  map(GfxFormat::eB10G10R10A2un, VK_FORMAT_A2R10G10B10_UNORM_PACK32);
  map(GfxFormat::eB10G10R10A2ui, VK_FORMAT_A2R10G10B10_UINT_PACK32);
  map(GfxFormat::eR11G11B10f, VK_FORMAT_B10G11R11_UFLOAT_PACK32);
  map(GfxFormat::eR16un, VK_FORMAT_R16_UNORM);
  map(GfxFormat::eR16sn, VK_FORMAT_R16_SNORM);
  map(GfxFormat::eR16ui, VK_FORMAT_R16_UINT);
  map(GfxFormat::eR16si, VK_FORMAT_R16_SINT);
  map(GfxFormat::eR16f, VK_FORMAT_R16_SFLOAT);
  map(GfxFormat::eR16G16un, VK_FORMAT_R16G16_UNORM);
  map(GfxFormat::eR16G16sn, VK_FORMAT_R16G16_SNORM);
  map(GfxFormat::eR16G16ui, VK_FORMAT_R16G16_UINT);
  map(GfxFormat::eR16G16si, VK_FORMAT_R16G16_SINT);
  map(GfxFormat::eR16G16f, VK_FORMAT_R16G16_SFLOAT);
  map(GfxFormat::eR16G16B16A16un, VK_FORMAT_R16G16B16A16_UNORM);
  map(GfxFormat::eR16G16B16A16sn, VK_FORMAT_R16G16B16A16_SNORM);
  map(GfxFormat::eR16G16B16A16ui, VK_FORMAT_R16G16B16A16_UINT);
  map(GfxFormat::eR16G16B16A16si, VK_FORMAT_R16G16B16A16_SINT);
  map(GfxFormat::eR16G16B16A16f, VK_FORMAT_R16G16B16A16_SFLOAT);
  map(GfxFormat::eR32ui, VK_FORMAT_R32_UINT);
  map(GfxFormat::eR32si, VK_FORMAT_R32_SINT);
  map(GfxFormat::eR32f, VK_FORMAT_R32_SFLOAT);
  map(GfxFormat::eR32G32ui, VK_FORMAT_R32G32_UINT);
  map(GfxFormat::eR32G32si, VK_FORMAT_R32G32_SINT);
  map(GfxFormat::eR32G32f, VK_FORMAT_R32G32_SFLOAT);
  map(GfxFormat::eR32G32B32ui, VK_FORMAT_R32G32B32_UINT);
  map(GfxFormat::eR32G32B32si, VK_FORMAT_R32G32B32_SINT);
  map(GfxFormat::eR32G32B32f, VK_FORMAT_R32G32B32_SFLOAT);
  map(GfxFormat::eR32G32B32A32ui, VK_FORMAT_R32G32B32A32_UINT);
  map(GfxFormat::eR32G32B32A32si, VK_FORMAT_R32G32B32A32_SINT);
  map(GfxFormat::eR32G32B32A32f, VK_FORMAT_R32G32B32A32_SFLOAT);
  map(GfxFormat::eBc1un, VK_FORMAT_BC1_RGBA_UNORM_BLOCK);
  map(GfxFormat::eBc1srgb, VK_FORMAT_BC1_RGBA_SRGB_BLOCK);
  map(GfxFormat::eBc2un, VK_FORMAT_BC2_UNORM_BLOCK);
  map(GfxFormat::eBc2srgb, VK_FORMAT_BC2_SRGB_BLOCK);
  map(GfxFormat::eBc3un, VK_FORMAT_BC3_UNORM_BLOCK);
  map(GfxFormat::eBc3srgb, VK_FORMAT_BC3_SRGB_BLOCK);
  map(GfxFormat::eBc4un, VK_FORMAT_BC4_UNORM_BLOCK);
  map(GfxFormat::eBc4sn, VK_FORMAT_BC4_SNORM_BLOCK);
  map(GfxFormat::eBc5un, VK_FORMAT_BC5_UNORM_BLOCK);
  map(GfxFormat::eBc5sn, VK_FORMAT_BC5_SNORM_BLOCK);
  map(GfxFormat::eBc6Huf, VK_FORMAT_BC6H_UFLOAT_BLOCK);
  map(GfxFormat::eBc6Hsf, VK_FORMAT_BC6H_SFLOAT_BLOCK);
  map(GfxFormat::eBc7un, VK_FORMAT_BC7_UNORM_BLOCK);
  map(GfxFormat::eBc7srgb, VK_FORMAT_BC7_SRGB_BLOCK);
  map(GfxFormat::eD16, VK_FORMAT_D16_UNORM);
  map(GfxFormat::eD24, VK_FORMAT_X8_D24_UNORM_PACK32);
  map(GfxFormat::eD24S8, VK_FORMAT_D24_UNORM_S8_UINT);
  map(GfxFormat::eD32, VK_FORMAT_D32_SFLOAT);
  map(GfxFormat::eD32S8, VK_FORMAT_D32_SFLOAT_S8_UINT); 
}


GfxFormat GfxVulkanFormatMap::getGfxFormat(VkFormat format) const {
  auto entry = m_vkToGfxFormats.find(format);

  if (entry == m_vkToGfxFormats.end())
    return GfxFormat::eUnknown;

  return entry->second;
}


void GfxVulkanFormatMap::map(
        GfxFormat                     gfxFormat,
        VkFormat                      vkFormat) {
  m_gfxToVkFormats[uint32_t(gfxFormat)] = vkFormat;
  m_vkToGfxFormats.insert({ vkFormat, gfxFormat });
}

}
