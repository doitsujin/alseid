#pragma once

#include <memory>
#include <vector>

#include "../wsi_display.h"

#include "wsi_sdl3_include.h"

namespace as {

class WsiSdl3;

/**
 * \brief SDL3 display
 */
class WsiSdl3Display : public WsiDisplayIface {

public:

  /**
   * \brief Creates SDL2 display
   *
   * \param [in] wsi WSI instance reference
   * \param [in] id SDL display ID
   */
  WsiSdl3Display(
          std::shared_ptr<WsiSdl3>      wsi,
          SDL_DisplayID                 id);

  WsiSdl3Display();

  /**
   * \brief Queries SDL display ID
   * \returns SDL display ID
   */
  SDL_DisplayID getId() const {
    return m_id;
  }

  /**
   * \brief Queries human-readable display name
   * \returns Display name
   */
  std::string getName() override;

  /**
   * \brief Queries desktop area of the display
   * \returns Desktop area of the display
   */
  Rect2D getDesktopArea() override;

  /**
   * \brief Queries current display mode
   * \returns Current display mdoe
   */
  WsiDisplayMode getCurrentDisplayMode() override;

  /**
   * \brief Queries default display mode
   * \returns Default display mdoe
   */
  WsiDisplayMode getDefaultDisplayMode() override;

  /**
   * \brief Enumerates available display modes
   *
   * \param [in] index Display mode index
   * \returns The display mode, or \c nullopt
   *    if \c index is out of bounds.
   */
  std::optional<WsiDisplayMode> enumDisplayModes(
          uint32_t                      index) override;

private:

  std::shared_ptr<WsiSdl3> m_wsi;

  SDL_DisplayID m_id = 0u;

  std::vector<WsiDisplayMode> m_availableModes;

};


/**
 * \brief Converts SDL display mode to generic representation
 * \returns Display mode struct
 */
WsiDisplayMode displayModeFromSdl3(
  const SDL_DisplayMode&              mode);


/**
 * \brief Converts display mode to SDL display mode
 * \returns SDL display mode
 */
SDL_DisplayMode displayModeToSdl3(
  const WsiDisplayMode&               mode);

}
