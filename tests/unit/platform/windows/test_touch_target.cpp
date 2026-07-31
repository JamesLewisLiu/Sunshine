/**
 * @file tests/unit/platform/windows/test_touch_target.cpp
 * @brief Test Windows touch target selection.
 */
#include "../../../tests_common.h"

#ifdef _WIN32

  // standard includes
  #include <array>
  #include <limits>

  // local includes
  #include "src/platform/windows/input_utils.h"

namespace {
  /**
   * @brief Verify that two touch ports contain identical bounds.
   *
   * @param actual Actual touch port.
   * @param expected Expected touch port.
   */
  void expect_touch_ports_equal(const platf::touch_port_t &actual, const platf::touch_port_t &expected) {
    EXPECT_EQ(actual.offset_x, expected.offset_x);
    EXPECT_EQ(actual.offset_y, expected.offset_y);
    EXPECT_EQ(actual.width, expected.width);
    EXPECT_EQ(actual.height, expected.height);
    EXPECT_EQ(actual.logical_width, expected.logical_width);
    EXPECT_EQ(actual.logical_height, expected.logical_height);
  }
}  // namespace

TEST(WindowsTouchTargetTest, DisabledUsesStreamedTouchPort) {
  const platf::touch_port_t streamed_touch_port {640, 360, 2560, 1440, 1280, 720};
  const std::array displays {
    platf::win_input::display_bounds_t {0, 0, 1920, 1080, true}
  };
  const auto primary_touch_port = platf::win_input::make_primary_display_touch_port(displays);

  const auto selected_touch_port = platf::win_input::select_touch_port(streamed_touch_port, false, primary_touch_port);

  expect_touch_ports_equal(selected_touch_port, streamed_touch_port);
}

TEST(WindowsTouchTargetTest, SingleDisplayUsesPrimaryDisplayBounds) {
  const platf::touch_port_t streamed_touch_port {0, 0, 2560, 1440, 0, 0};
  const std::array displays {
    platf::win_input::display_bounds_t {0, 0, 1920, 1080, true}
  };
  const auto primary_touch_port = platf::win_input::make_primary_display_touch_port(displays);
  ASSERT_TRUE(primary_touch_port);

  const auto selected_touch_port = platf::win_input::select_touch_port(streamed_touch_port, true, primary_touch_port);

  expect_touch_ports_equal(selected_touch_port, {0, 0, 1920, 1080, 0, 0});
}

TEST(WindowsTouchTargetTest, VirtualDesktopOriginOffsetsPrimaryDisplay) {
  const std::array displays {
    platf::win_input::display_bounds_t {-2560, -1440, 2560, 1440, false},
    platf::win_input::display_bounds_t {0, 0, 1920, 1080, true}
  };
  const auto primary_touch_port = platf::win_input::make_primary_display_touch_port(displays);
  ASSERT_TRUE(primary_touch_port);

  expect_touch_ports_equal(*primary_touch_port, {2560, 1440, 1920, 1080, 0, 0});
}

TEST(WindowsTouchTargetTest, DifferentResolutionPreservesRelativePosition) {
  constexpr float normalized_x = 0.5f;
  constexpr float normalized_y = 0.5f;
  const platf::touch_port_t streamed_touch_port {3840, 0, 1280, 768, 0, 0};
  const std::array displays {
    platf::win_input::display_bounds_t {0, 0, 3840, 2160, true},
    platf::win_input::display_bounds_t {3840, 0, 1280, 768, false}
  };
  const auto primary_touch_port = platf::win_input::make_primary_display_touch_port(displays);

  const auto selected_touch_port = platf::win_input::select_touch_port(streamed_touch_port, true, primary_touch_port);

  EXPECT_FLOAT_EQ(selected_touch_port.offset_x + normalized_x * selected_touch_port.width, 1920.0f);
  EXPECT_FLOAT_EQ(selected_touch_port.offset_y + normalized_y * selected_touch_port.height, 1080.0f);
}

TEST(WindowsTouchTargetTest, PrimarySelectionDoesNotMutateStreamedPortUsedByPenAndMouse) {
  platf::touch_port_t streamed_touch_port {640, 360, 2560, 1440, 1280, 720};
  const platf::touch_port_t original_streamed_touch_port = streamed_touch_port;
  const std::array displays {
    platf::win_input::display_bounds_t {-640, -360, 640, 360, false},
    platf::win_input::display_bounds_t {0, 0, 1920, 1080, true}
  };
  const auto primary_touch_port = platf::win_input::make_primary_display_touch_port(displays);

  static_cast<void>(platf::win_input::select_touch_port(streamed_touch_port, true, primary_touch_port));

  expect_touch_ports_equal(streamed_touch_port, original_streamed_touch_port);
}

TEST(WindowsTouchTargetTest, InvalidPrimaryDisplayWidthFallsBackToStreamedDisplay) {
  const platf::touch_port_t streamed_touch_port {640, 0, 2560, 1440, 0, 0};
  const std::array displays {
    platf::win_input::display_bounds_t {0, 0, 0, 1080, true}
  };
  const auto primary_touch_port = platf::win_input::make_primary_display_touch_port(displays);
  ASSERT_FALSE(primary_touch_port);

  const auto selected_touch_port = platf::win_input::select_touch_port(streamed_touch_port, true, primary_touch_port);

  expect_touch_ports_equal(selected_touch_port, streamed_touch_port);
}

TEST(WindowsTouchTargetTest, InvalidPrimaryDisplayHeightFallsBackToStreamedDisplay) {
  const platf::touch_port_t streamed_touch_port {0, 720, 1920, 1080, 0, 0};
  const std::array displays {
    platf::win_input::display_bounds_t {0, 0, 1920, -1, true}
  };
  const auto primary_touch_port = platf::win_input::make_primary_display_touch_port(displays);
  ASSERT_FALSE(primary_touch_port);

  const auto selected_touch_port = platf::win_input::select_touch_port(streamed_touch_port, true, primary_touch_port);

  expect_touch_ports_equal(selected_touch_port, streamed_touch_port);
}

TEST(WindowsTouchTargetTest, MissingPrimaryDisplayFallsBackToStreamedDisplay) {
  const platf::touch_port_t streamed_touch_port {0, 0, 1280, 768, 0, 0};
  const std::array displays {
    platf::win_input::display_bounds_t {0, 0, 1280, 768, false}
  };
  const auto primary_touch_port = platf::win_input::make_primary_display_touch_port(displays);
  ASSERT_FALSE(primary_touch_port);

  const auto selected_touch_port = platf::win_input::select_touch_port(streamed_touch_port, true, primary_touch_port);

  expect_touch_ports_equal(selected_touch_port, streamed_touch_port);
}

TEST(WindowsTouchTargetTest, EmptyDisplayLayoutIsInvalid) {
  const std::array<platf::win_input::display_bounds_t, 0> displays {};

  EXPECT_FALSE(platf::win_input::make_primary_display_touch_port(displays));
}

TEST(WindowsTouchTargetTest, MultiplePrimaryDisplaysAreInvalid) {
  const std::array displays {
    platf::win_input::display_bounds_t {0, 0, 3840, 2160, true},
    platf::win_input::display_bounds_t {3840, 0, 1280, 768, true}
  };

  EXPECT_FALSE(platf::win_input::make_primary_display_touch_port(displays));
}

TEST(WindowsTouchTargetTest, UnrepresentablePrimaryDisplayOffsetIsInvalid) {
  const std::array displays {
    platf::win_input::display_bounds_t {std::numeric_limits<int>::min(), 0, 1280, 768, false},
    platf::win_input::display_bounds_t {std::numeric_limits<int>::max(), 0, 3840, 2160, true}
  };

  EXPECT_FALSE(platf::win_input::make_primary_display_touch_port(displays));
}

#endif  // _WIN32
