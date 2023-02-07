#include <algorithm>

#include "../../util/util_string.h"

#include "wsi_sdl2.h"
#include "wsi_sdl2_display.h"

namespace as {

WsiSdl2Display::WsiSdl2Display(
        std::shared_ptr<WsiSdl2>      wsi,
        int                           index)
: m_wsi(std::move(wsi)), m_index(index) {
  int modeCount = SDL_GetNumDisplayModes(index);

  if (modeCount < 1)
    throw SdlError("Failed to query number of display modes.");

  m_availableModes.reserve(modeCount);

  for (int i = 0; i < modeCount; i++) {
    SDL_DisplayMode mode = { };

    if (SDL_GetDisplayMode(m_index, i, &mode))
      throw SdlError("Failed to query display mode");

    m_availableModes.push_back(displayModeFromSdl(mode));
  }

  std::sort(m_availableModes.begin(), m_availableModes.end(),
    [] (const WsiDisplayMode& a, const WsiDisplayMode& b) {
      if (a.resolution.at<0>() > b.resolution.at<0>()) return true;
      if (a.resolution.at<0>() < b.resolution.at<0>()) return true;

      if (a.resolution.at<1>() > b.resolution.at<1>()) return true;
      if (a.resolution.at<1>() < b.resolution.at<1>()) return true;

      return a.refreshRate > b.refreshRate;
    });
}


WsiSdl2Display::WsiSdl2Display() {

}


std::string WsiSdl2Display::getName() {
  const char* name = SDL_GetDisplayName(m_index);

  if (!name)
    return strcat("Display ", m_index);

  return name;
}


Rect2D WsiSdl2Display::getDesktopArea() {
  SDL_Rect area = { };

  if (SDL_GetDisplayBounds(m_index, &area))
    throw SdlError("Failed to query desktop area");

  Rect2D result;
  result.offset = Offset2D( int32_t(area.x),  int32_t(area.y));
  result.extent = Extent2D(uint32_t(area.w), uint32_t(area.h));
  return result;
}


WsiDisplayMode WsiSdl2Display::getCurrentDisplayMode() {
  SDL_DisplayMode mode = { };

  if (SDL_GetCurrentDisplayMode(m_index, &mode))
    throw SdlError("Failed to query current display mode");

  return displayModeFromSdl(mode);
}


WsiDisplayMode WsiSdl2Display::getDefaultDisplayMode() {
  SDL_DisplayMode mode = { };

  if (SDL_GetDesktopDisplayMode(m_index, &mode))
    throw SdlError("Failed to query desktop display mode");

  return displayModeFromSdl(mode);
}


std::optional<WsiDisplayMode> WsiSdl2Display::enumDisplayModes(
        uint32_t                      index) {
  if (index >= m_availableModes.size())
    return std::nullopt;

  return m_availableModes[index];
}


WsiDisplayMode displayModeFromSdl(
  const SDL_DisplayMode&              mode) {
  WsiDisplayMode result = { };
  result.resolution = Extent2D(
    uint32_t(mode.w),
    uint32_t(mode.h));
  result.refreshRate = mode.refresh_rate * 1000;
  return result;
}


SDL_DisplayMode displayModeToSdl(
  const WsiDisplayMode&               mode) {
  SDL_DisplayMode result = { };
  result.format = SDL_PIXELFORMAT_UNKNOWN;
  result.w = int(mode.resolution.at<0>());
  result.h = int(mode.resolution.at<1>());
  result.refresh_rate = int(mode.refreshRate / 1000);
  return result;
}

}
