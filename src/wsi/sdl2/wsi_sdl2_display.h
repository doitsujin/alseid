#pragma once

#include <memory>
#include <vector>

#include "../wsi_display.h"

namespace as {

class WsiSdl2;

/**
 * \brief SDL2 display
 */
class WsiSdl2Display : public WsiDisplayIface {

public:

  /**
   * \brief Creates SDL2 display
   *
   * \param [in] wsi WSI instance reference
   * \param [in] index SDL display index
   */
  WsiSdl2Display(
          std::shared_ptr<WsiSdl2>      wsi,
          int                           index);

  WsiSdl2Display();

  /**
   * \brief Queries SDL display index
   * \returns SDL display index
   */
  int getIndex() const {
    return m_index;
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

  std::shared_ptr<WsiSdl2> m_wsi;

  int m_index;

  std::vector<WsiDisplayMode> m_availableModes;

};


/**
 * \brief Converts SDL display mode to generic representation
 * \returns Display mode struct
 */
WsiDisplayMode displayModeFromSdl(
  const SDL_DisplayMode&              mode);


/**
 * \brief Converts display mode to SDL display mode
 * \returns SDL display mode
 */
SDL_DisplayMode displayModeToSdl(
  const WsiDisplayMode&               mode);

}
