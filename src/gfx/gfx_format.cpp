#include "../util/util_assert.h"

#include "gfx_format.h"

namespace as {

GfxFormatMetadataMap::GfxFormatMetadataMap() {
  const Extent2D g_1x1(0, 0);
  const Extent2D g_4x4(2, 2);

  addFormat(GfxFormat::eR4G4B4A4un,         GfxImageAspect::eColor, g_1x1, 0, { 2u, g_1x1, GfxFormatType::eUnorm, 4, 4, 4, 4 });
  addFormat(GfxFormat::eR8un,               GfxImageAspect::eColor, g_1x1, 0, { 1u, g_1x1, GfxFormatType::eUnorm, 8 });
  addFormat(GfxFormat::eR8sn,               GfxImageAspect::eColor, g_1x1, 0, { 1u, g_1x1, GfxFormatType::eSnorm, 8 });
  addFormat(GfxFormat::eR8ui,               GfxImageAspect::eColor, g_1x1, 0, { 1u, g_1x1, GfxFormatType::eUint,  8 });
  addFormat(GfxFormat::eR8si,               GfxImageAspect::eColor, g_1x1, 0, { 1u, g_1x1, GfxFormatType::eSint,  8 });
  addFormat(GfxFormat::eR8G8un,             GfxImageAspect::eColor, g_1x1, 0, { 2u, g_1x1, GfxFormatType::eUnorm, 8, 8 });
  addFormat(GfxFormat::eR8G8sn,             GfxImageAspect::eColor, g_1x1, 0, { 2u, g_1x1, GfxFormatType::eSnorm, 8, 8 });
  addFormat(GfxFormat::eR8G8ui,             GfxImageAspect::eColor, g_1x1, 0, { 2u, g_1x1, GfxFormatType::eUint,  8, 8 });
  addFormat(GfxFormat::eR8G8si,             GfxImageAspect::eColor, g_1x1, 0, { 2u, g_1x1, GfxFormatType::eSint,  8, 8 });
  addFormat(GfxFormat::eR8G8B8un,           GfxImageAspect::eColor, g_1x1, 0, { 3u, g_1x1, GfxFormatType::eUnorm, 8, 8, 8 });
  addFormat(GfxFormat::eR8G8B8sn,           GfxImageAspect::eColor, g_1x1, 0, { 3u, g_1x1, GfxFormatType::eSnorm, 8, 8, 8 });
  addFormat(GfxFormat::eR8G8B8ui,           GfxImageAspect::eColor, g_1x1, 0, { 3u, g_1x1, GfxFormatType::eUint,  8, 8, 8 });
  addFormat(GfxFormat::eR8G8B8si,           GfxImageAspect::eColor, g_1x1, 0, { 3u, g_1x1, GfxFormatType::eSint,  8, 8, 8 });
  addFormat(GfxFormat::eR8G8B8A8un,         GfxImageAspect::eColor, g_1x1, 0, { 4u, g_1x1, GfxFormatType::eUnorm, 8, 8, 8, 8 });
  addFormat(GfxFormat::eR8G8B8A8sn,         GfxImageAspect::eColor, g_1x1, 0, { 4u, g_1x1, GfxFormatType::eSnorm, 8, 8, 8, 8 });
  addFormat(GfxFormat::eR8G8B8A8ui,         GfxImageAspect::eColor, g_1x1, 0, { 4u, g_1x1, GfxFormatType::eUint,  8, 8, 8, 8 });
  addFormat(GfxFormat::eR8G8B8A8si,         GfxImageAspect::eColor, g_1x1, 0, { 4u, g_1x1, GfxFormatType::eSint,  8, 8, 8, 8 });
  addFormat(GfxFormat::eR8G8B8A8srgb,       GfxImageAspect::eColor, g_1x1, GfxFormatFlag::eSrgb, { 4u, g_1x1, GfxFormatType::eUnorm, 8, 8, 8, 8 });
  addFormat(GfxFormat::eB8G8R8A8un,         GfxImageAspect::eColor, g_1x1, 0, { 4u, g_1x1, GfxFormatType::eUnorm, 8, 8, 8, 8 });
  addFormat(GfxFormat::eB8G8R8A8sn,         GfxImageAspect::eColor, g_1x1, 0, { 4u, g_1x1, GfxFormatType::eSnorm, 8, 8, 8, 8 });
  addFormat(GfxFormat::eB8G8R8A8ui,         GfxImageAspect::eColor, g_1x1, 0, { 4u, g_1x1, GfxFormatType::eUint,  8, 8, 8, 8 });
  addFormat(GfxFormat::eB8G8R8A8si,         GfxImageAspect::eColor, g_1x1, 0, { 4u, g_1x1, GfxFormatType::eSint,  8, 8, 8, 8 });
  addFormat(GfxFormat::eB8G8R8A8srgb,       GfxImageAspect::eColor, g_1x1, GfxFormatFlag::eSrgb, { 4u, g_1x1, GfxFormatType::eUnorm, 8, 8, 8, 8 });
  addFormat(GfxFormat::eR9G9B9E5f,          GfxImageAspect::eColor, g_1x1, 0, { 4u, g_1x1, GfxFormatType::eFloat, 9, 9, 9, 5 });
  addFormat(GfxFormat::eR10G10B10A2un,      GfxImageAspect::eColor, g_1x1, 0, { 4u, g_1x1, GfxFormatType::eUnorm, 10, 10, 10, 2 });
  addFormat(GfxFormat::eR10G10B10A2sn,      GfxImageAspect::eColor, g_1x1, 0, { 4u, g_1x1, GfxFormatType::eSnorm, 10, 10, 10, 2 });
  addFormat(GfxFormat::eR10G10B10A2ui,      GfxImageAspect::eColor, g_1x1, 0, { 4u, g_1x1, GfxFormatType::eUint,  10, 10, 10, 2 });
  addFormat(GfxFormat::eB10G10R10A2un,      GfxImageAspect::eColor, g_1x1, 0, { 4u, g_1x1, GfxFormatType::eUnorm, 10, 10, 10, 2 });
  addFormat(GfxFormat::eB10G10R10A2sn,      GfxImageAspect::eColor, g_1x1, 0, { 4u, g_1x1, GfxFormatType::eSnorm, 10, 10, 10, 2 });
  addFormat(GfxFormat::eB10G10R10A2ui,      GfxImageAspect::eColor, g_1x1, 0, { 4u, g_1x1, GfxFormatType::eUint,  10, 10, 10, 2 });
  addFormat(GfxFormat::eR11G11B10f,         GfxImageAspect::eColor, g_1x1, 0, { 4u, g_1x1, GfxFormatType::eFloat, 11, 11, 10 });
  addFormat(GfxFormat::eR16un,              GfxImageAspect::eColor, g_1x1, 0, { 2u, g_1x1, GfxFormatType::eUnorm, 16 });
  addFormat(GfxFormat::eR16sn,              GfxImageAspect::eColor, g_1x1, 0, { 2u, g_1x1, GfxFormatType::eSnorm, 16 });
  addFormat(GfxFormat::eR16ui,              GfxImageAspect::eColor, g_1x1, 0, { 2u, g_1x1, GfxFormatType::eUint,  16 });
  addFormat(GfxFormat::eR16si,              GfxImageAspect::eColor, g_1x1, 0, { 2u, g_1x1, GfxFormatType::eSint,  16 });
  addFormat(GfxFormat::eR16f,               GfxImageAspect::eColor, g_1x1, 0, { 2u, g_1x1, GfxFormatType::eFloat, 16 });
  addFormat(GfxFormat::eR16G16un,           GfxImageAspect::eColor, g_1x1, 0, { 4u, g_1x1, GfxFormatType::eUnorm, 16, 16 });
  addFormat(GfxFormat::eR16G16sn,           GfxImageAspect::eColor, g_1x1, 0, { 4u, g_1x1, GfxFormatType::eSnorm, 16, 16 });
  addFormat(GfxFormat::eR16G16ui,           GfxImageAspect::eColor, g_1x1, 0, { 4u, g_1x1, GfxFormatType::eUint,  16, 16 });
  addFormat(GfxFormat::eR16G16si,           GfxImageAspect::eColor, g_1x1, 0, { 4u, g_1x1, GfxFormatType::eSint,  16, 16 });
  addFormat(GfxFormat::eR16G16f,            GfxImageAspect::eColor, g_1x1, 0, { 4u, g_1x1, GfxFormatType::eFloat, 16, 16 });
  addFormat(GfxFormat::eR16G16B16un,        GfxImageAspect::eColor, g_1x1, 0, { 6u, g_1x1, GfxFormatType::eUnorm, 16, 16, 16 });
  addFormat(GfxFormat::eR16G16B16sn,        GfxImageAspect::eColor, g_1x1, 0, { 6u, g_1x1, GfxFormatType::eSnorm, 16, 16, 16 });
  addFormat(GfxFormat::eR16G16B16ui,        GfxImageAspect::eColor, g_1x1, 0, { 6u, g_1x1, GfxFormatType::eUint,  16, 16, 16 });
  addFormat(GfxFormat::eR16G16B16si,        GfxImageAspect::eColor, g_1x1, 0, { 6u, g_1x1, GfxFormatType::eSint,  16, 16, 16 });
  addFormat(GfxFormat::eR16G16B16f,         GfxImageAspect::eColor, g_1x1, 0, { 6u, g_1x1, GfxFormatType::eFloat, 16, 16, 16 });
  addFormat(GfxFormat::eR16G16B16A16un,     GfxImageAspect::eColor, g_1x1, 0, { 8u, g_1x1, GfxFormatType::eUnorm, 16, 16, 16, 16 });
  addFormat(GfxFormat::eR16G16B16A16sn,     GfxImageAspect::eColor, g_1x1, 0, { 8u, g_1x1, GfxFormatType::eSnorm, 16, 16, 16, 16 });
  addFormat(GfxFormat::eR16G16B16A16ui,     GfxImageAspect::eColor, g_1x1, 0, { 8u, g_1x1, GfxFormatType::eUint,  16, 16, 16, 16 });
  addFormat(GfxFormat::eR16G16B16A16si,     GfxImageAspect::eColor, g_1x1, 0, { 8u, g_1x1, GfxFormatType::eSint,  16, 16, 16, 16 });
  addFormat(GfxFormat::eR16G16B16A16f,      GfxImageAspect::eColor, g_1x1, 0, { 8u, g_1x1, GfxFormatType::eFloat, 16, 16, 16, 16 });
  addFormat(GfxFormat::eR32ui,              GfxImageAspect::eColor, g_1x1, 0, { 4u, g_1x1, GfxFormatType::eUint,  32 });
  addFormat(GfxFormat::eR32si,              GfxImageAspect::eColor, g_1x1, 0, { 4u, g_1x1, GfxFormatType::eSint,  32 });
  addFormat(GfxFormat::eR32f,               GfxImageAspect::eColor, g_1x1, 0, { 4u, g_1x1, GfxFormatType::eFloat, 32 });
  addFormat(GfxFormat::eR32G32ui,           GfxImageAspect::eColor, g_1x1, 0, { 8u, g_1x1, GfxFormatType::eUint,  32, 32 });
  addFormat(GfxFormat::eR32G32si,           GfxImageAspect::eColor, g_1x1, 0, { 8u, g_1x1, GfxFormatType::eSint,  32, 32 });
  addFormat(GfxFormat::eR32G32f,            GfxImageAspect::eColor, g_1x1, 0, { 8u, g_1x1, GfxFormatType::eFloat, 32, 32 });
  addFormat(GfxFormat::eR32G32B32ui,        GfxImageAspect::eColor, g_1x1, 0, {12u, g_1x1, GfxFormatType::eUint,  32, 32, 32 });
  addFormat(GfxFormat::eR32G32B32si,        GfxImageAspect::eColor, g_1x1, 0, {12u, g_1x1, GfxFormatType::eSint,  32, 32, 32 });
  addFormat(GfxFormat::eR32G32B32f,         GfxImageAspect::eColor, g_1x1, 0, {12u, g_1x1, GfxFormatType::eFloat, 32, 32, 32 });
  addFormat(GfxFormat::eR32G32B32A32ui,     GfxImageAspect::eColor, g_1x1, 0, {16u, g_1x1, GfxFormatType::eUint,  32, 32, 32, 32 });
  addFormat(GfxFormat::eR32G32B32A32si,     GfxImageAspect::eColor, g_1x1, 0, {16u, g_1x1, GfxFormatType::eSint,  32, 32, 32, 32 });
  addFormat(GfxFormat::eR32G32B32A32f,      GfxImageAspect::eColor, g_1x1, 0, {16u, g_1x1, GfxFormatType::eFloat, 32, 32, 32, 32 });
  addFormat(GfxFormat::eBc1un,              GfxImageAspect::eColor, g_4x4, 0, { 8u, g_1x1, GfxFormatType::eFloat });
  addFormat(GfxFormat::eBc1srgb,            GfxImageAspect::eColor, g_4x4, GfxFormatFlag::eSrgb, { 8u, g_1x1, GfxFormatType::eUnorm });
  addFormat(GfxFormat::eBc2un,              GfxImageAspect::eColor, g_4x4, 0, {16u, g_1x1, GfxFormatType::eUnorm });
  addFormat(GfxFormat::eBc2srgb,            GfxImageAspect::eColor, g_4x4, GfxFormatFlag::eSrgb, {16u, g_1x1, GfxFormatType::eUnorm });
  addFormat(GfxFormat::eBc3un,              GfxImageAspect::eColor, g_4x4, 0, {16u, g_1x1, GfxFormatType::eUnorm });
  addFormat(GfxFormat::eBc3srgb,            GfxImageAspect::eColor, g_4x4, GfxFormatFlag::eSrgb, {16u, g_1x1, GfxFormatType::eUnorm });
  addFormat(GfxFormat::eBc4un,              GfxImageAspect::eColor, g_4x4, 0, { 8u, g_1x1, GfxFormatType::eUnorm });
  addFormat(GfxFormat::eBc4sn,              GfxImageAspect::eColor, g_4x4, 0, { 8u, g_1x1, GfxFormatType::eSnorm });
  addFormat(GfxFormat::eBc5un,              GfxImageAspect::eColor, g_4x4, 0, {16u, g_1x1, GfxFormatType::eUnorm });
  addFormat(GfxFormat::eBc5sn,              GfxImageAspect::eColor, g_4x4, 0, {16u, g_1x1, GfxFormatType::eSnorm });
  addFormat(GfxFormat::eBc6Huf,             GfxImageAspect::eColor, g_4x4, 0, {16u, g_1x1, GfxFormatType::eFloat });
  addFormat(GfxFormat::eBc6Hsf,             GfxImageAspect::eColor, g_4x4, 0, {16u, g_1x1, GfxFormatType::eFloat });
  addFormat(GfxFormat::eBc7un,              GfxImageAspect::eColor, g_4x4, 0, {16u, g_1x1, GfxFormatType::eUnorm });
  addFormat(GfxFormat::eBc7srgb,            GfxImageAspect::eColor, g_4x4, GfxFormatFlag::eSrgb, {16u, g_1x1, GfxFormatType::eUnorm });
  addFormat(GfxFormat::eD16,                GfxImageAspect::eDepth, g_1x1, 0, { 2u, g_1x1, GfxFormatType::eUnorm, 16 });
  addFormat(GfxFormat::eD24,                GfxImageAspect::eDepth, g_1x1, 0, { 4u, g_1x1, GfxFormatType::eUnorm, 24 });
  addFormat(GfxFormat::eD32,                GfxImageAspect::eDepth, g_1x1, 0, { 4u, g_1x1, GfxFormatType::eFloat, 32 });
  addFormat(GfxFormat::eD24S8,              GfxImageAspect::eDepth | GfxImageAspect::eStencil, g_1x1, 0, { 4u, g_1x1, GfxFormatType::eUnorm, 24 }, { 1u, g_1x1, GfxFormatType::eUint, 8 });
  addFormat(GfxFormat::eD32S8,              GfxImageAspect::eDepth | GfxImageAspect::eStencil, g_1x1, 0, { 4u, g_1x1, GfxFormatType::eFloat, 32 }, { 1u, g_1x1, GfxFormatType::eUint, 8 });
}


void GfxFormatMetadataMap::addFormat(
        GfxFormat         format,
        GfxImageAspects   aspects,
        Extent2D          blockExtentLog2,
        GfxFormatFlags    flags,
        PlaneDesc         plane0Info,
        PlaneDesc         plane1Info,
        PlaneDesc         plane2Info) {
  const std::array<PlaneDesc, 3> planes = {{
    plane0Info, plane1Info, plane2Info
  }};

  GfxFormatInfo info = { };
  info.aspects = aspects;
  info.blockExtent = Extent2D(1u, 1u) << blockExtentLog2;
  info.blockExtentLog2 = blockExtentLog2;
  info.flags = flags;

  if (blockExtentLog2 != Extent2D(0u, 0u))
    info.flags |= GfxFormatFlag::eCompressed;

  for (auto aspect : aspects) {
    uint32_t plane = info.planeCount++;
    dbg_assert(plane < planes.size());

    info.planes[plane].aspect = aspect;
    info.planes[plane].elementSize = planes[plane].elementSize;
    info.planes[plane].subsampleLog2 = planes[plane].subsampleLog2;
    info.planes[plane].subsample = Extent2D(1, 1) << planes[plane].subsampleLog2;
    info.planes[plane].type = planes[plane].type;
    info.planes[plane].rBits = planes[plane].rBits;
    info.planes[plane].gBits = planes[plane].gBits;
    info.planes[plane].bBits = planes[plane].bBits;
    info.planes[plane].aBits = planes[plane].aBits;
  }

  set(format, info);
}

}
