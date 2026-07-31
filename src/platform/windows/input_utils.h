/**
 * @file src/platform/windows/input_utils.h
 * @brief Helpers for selecting Windows touch input targets.
 */
#pragma once

// standard includes
#include <optional>
#include <span>

// local includes
#include "src/platform/common.h"

namespace platf::win_input {
  /**
   * @brief Physical bounds of an attached Windows display.
   */
  struct display_bounds_t {
    int offset_x;  ///< Horizontal display offset in physical virtual-desktop coordinates.
    int offset_y;  ///< Vertical display offset in physical virtual-desktop coordinates.
    int width;  ///< Display width in physical pixels.
    int height;  ///< Display height in physical pixels.
    bool is_primary;  ///< Whether Windows marks this display as the primary display.
  };

  /**
   * @brief Build a touch port targeting the Windows primary display.
   *
   * @param displays Physical bounds of the currently attached displays.
   * @return Primary-display touch port, or `std::nullopt` when the display topology is invalid.
   */
  std::optional<touch_port_t> make_primary_display_touch_port(std::span<const display_bounds_t> displays);

  /**
   * @brief Select the touch port used for native Windows touch injection.
   *
   * @param streamed_touch_port Touch port associated with the streamed display.
   * @param send_to_primary_display Whether native touch should target the primary display.
   * @param primary_touch_port Available primary-display touch port, if valid.
   * @return The selected touch port.
   */
  touch_port_t select_touch_port(
    const touch_port_t &streamed_touch_port,
    bool send_to_primary_display,
    const std::optional<touch_port_t> &primary_touch_port
  );
}  // namespace platf::win_input
