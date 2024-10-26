#include <algorithm>

#include "../../util/util_string.h"

#include "wsi_sdl3.h"
#include "wsi_sdl3_display.h"

namespace as {

WsiSdl3Display::WsiSdl3Display(
        std::shared_ptr<WsiSdl3>      wsi,
        SDL_DisplayID                 id)
: m_wsi(std::move(wsi)), m_id(id) {
  int modeCount = 0;

  SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(m_id, &modeCount);

  if (!modes)
    throw Sdl3Error("Failed to query available display modes.");

  m_availableModes.reserve(modeCount);

  for (int i = 0; i < modeCount; i++)
    m_availableModes.push_back(displayModeFromSdl3(*modes[i]));

  SDL_free(modes);

  std::sort(m_availableModes.begin(), m_availableModes.end(),
    [] (const WsiDisplayMode& a, const WsiDisplayMode& b) {
      if (a.resolution.at<0>() > b.resolution.at<0>()) return true;
      if (a.resolution.at<0>() < b.resolution.at<0>()) return false;

      if (a.resolution.at<1>() > b.resolution.at<1>()) return true;
      if (a.resolution.at<1>() < b.resolution.at<1>()) return false;

      return a.refreshRate > b.refreshRate;
    });
}


WsiSdl3Display::WsiSdl3Display() {

}


std::string WsiSdl3Display::getName() {
  const char* name = SDL_GetDisplayName(m_id);

  if (!name)
    return strcat("Display ", uint32_t(m_id));

  return name;
}


Rect2D WsiSdl3Display::getDesktopArea() {
  SDL_Rect area = { };

  if (!SDL_GetDisplayBounds(uint32_t(m_id), &area))
    throw Sdl3Error("Failed to query desktop area");

  Rect2D result;
  result.offset = Offset2D( int32_t(area.x),  int32_t(area.y));
  result.extent = Extent2D(uint32_t(area.w), uint32_t(area.h));
  return result;
}


WsiDisplayMode WsiSdl3Display::getCurrentDisplayMode() {
  const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(m_id);

  if (!mode)
    throw Sdl3Error("Failed to query current display mode");

  return displayModeFromSdl3(*mode);
}


WsiDisplayMode WsiSdl3Display::getDefaultDisplayMode() {
  const SDL_DisplayMode* mode = SDL_GetDesktopDisplayMode(m_id);

  if (!mode)
    throw Sdl3Error("Failed to query desktop display mode");

  return displayModeFromSdl3(*mode);
}


std::optional<WsiDisplayMode> WsiSdl3Display::enumDisplayModes(
        uint32_t                      index) {
  if (index >= m_availableModes.size())
    return std::nullopt;

  return m_availableModes[index];
}


WsiDisplayMode displayModeFromSdl3(
  const SDL_DisplayMode&              mode) {
  WsiDisplayMode result = { };
  result.resolution = Extent2D(
    uint32_t(mode.w),
    uint32_t(mode.h));
  result.refreshRate = uint32_t(mode.refresh_rate * 1000.0f);
  return result;
}


SDL_DisplayMode displayModeToSdl3(
  const WsiDisplayMode&               mode) {
  SDL_DisplayMode result = { };
  result.format = SDL_PIXELFORMAT_UNKNOWN;
  result.w = int(mode.resolution.at<0>());
  result.h = int(mode.resolution.at<1>());
  result.refresh_rate = int(mode.refreshRate / 1000);
  return result;
}

}
