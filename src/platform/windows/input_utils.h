/**
 * @file src/platform/windows/input_utils.h
 * @brief Helpers for selecting Windows touch input targets.
 */
#pragma once

// standard includes
#include <optional>

// local includes
#include "src/platform/common.h"

namespace platf::win_input {
  /**
   * @brief Windows display metrics used to locate the primary display in the virtual desktop.
   */
  struct display_metrics_t {
    int virtual_origin_x;  ///< Horizontal origin of the virtual desktop in screen coordinates.
    int virtual_origin_y;  ///< Vertical origin of the virtual desktop in screen coordinates.
    int primary_width;  ///< Width of the primary display in physical pixels.
    int primary_height;  ///< Height of the primary display in physical pixels.
  };

  /**
   * @brief Build a touch port targeting the Windows primary display.
   *
   * @param metrics Current primary-display and virtual-desktop metrics.
   * @return Primary-display touch port, or `std::nullopt` when the dimensions are invalid.
   */
  std::optional<touch_port_t> make_primary_display_touch_port(const display_metrics_t &metrics);

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
