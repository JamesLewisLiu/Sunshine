/**
 * @file tests/unit/platform/windows/test_touch_target.cpp
 * @brief Test Windows touch target selection.
 */
#include "../../../tests_common.h"

#ifdef _WIN32

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
  const auto primary_touch_port = platf::win_input::make_primary_display_touch_port({0, 0, 1920, 1080});

  const auto selected_touch_port = platf::win_input::select_touch_port(streamed_touch_port, false, primary_touch_port);

  expect_touch_ports_equal(selected_touch_port, streamed_touch_port);
}

TEST(WindowsTouchTargetTest, SingleDisplayUsesPrimaryDisplayBounds) {
  const platf::touch_port_t streamed_touch_port {0, 0, 2560, 1440, 0, 0};
  const auto primary_touch_port = platf::win_input::make_primary_display_touch_port({0, 0, 1920, 1080});
  ASSERT_TRUE(primary_touch_port);

  const auto selected_touch_port = platf::win_input::select_touch_port(streamed_touch_port, true, primary_touch_port);

  expect_touch_ports_equal(selected_touch_port, {0, 0, 1920, 1080, 0, 0});
}

TEST(WindowsTouchTargetTest, VirtualDesktopOriginOffsetsPrimaryDisplay) {
  const auto primary_touch_port = platf::win_input::make_primary_display_touch_port({-2560, -1440, 1920, 1080});
  ASSERT_TRUE(primary_touch_port);

  expect_touch_ports_equal(*primary_touch_port, {2560, 1440, 1920, 1080, 0, 0});
}

TEST(WindowsTouchTargetTest, DifferentResolutionPreservesRelativePosition) {
  constexpr float normalized_x = 0.25f;
  constexpr float normalized_y = 0.75f;
  const platf::touch_port_t streamed_touch_port {0, 0, 1280, 720, 0, 0};
  const auto primary_touch_port = platf::win_input::make_primary_display_touch_port({-1920, 0, 3840, 2160});

  const auto selected_touch_port = platf::win_input::select_touch_port(streamed_touch_port, true, primary_touch_port);

  EXPECT_FLOAT_EQ(selected_touch_port.offset_x + normalized_x * selected_touch_port.width, 2880.0f);
  EXPECT_FLOAT_EQ(selected_touch_port.offset_y + normalized_y * selected_touch_port.height, 1620.0f);
}

TEST(WindowsTouchTargetTest, PrimarySelectionDoesNotMutateStreamedPortUsedByPenAndMouse) {
  platf::touch_port_t streamed_touch_port {640, 360, 2560, 1440, 1280, 720};
  const platf::touch_port_t original_streamed_touch_port = streamed_touch_port;
  const auto primary_touch_port = platf::win_input::make_primary_display_touch_port({-640, -360, 1920, 1080});

  static_cast<void>(platf::win_input::select_touch_port(streamed_touch_port, true, primary_touch_port));

  expect_touch_ports_equal(streamed_touch_port, original_streamed_touch_port);
}

TEST(WindowsTouchTargetTest, InvalidPrimaryDisplayWidthFallsBackToStreamedDisplay) {
  const platf::touch_port_t streamed_touch_port {640, 0, 2560, 1440, 0, 0};
  const auto primary_touch_port = platf::win_input::make_primary_display_touch_port({-640, 0, 0, 1080});
  ASSERT_FALSE(primary_touch_port);

  const auto selected_touch_port = platf::win_input::select_touch_port(streamed_touch_port, true, primary_touch_port);

  expect_touch_ports_equal(selected_touch_port, streamed_touch_port);
}

TEST(WindowsTouchTargetTest, InvalidPrimaryDisplayHeightFallsBackToStreamedDisplay) {
  const platf::touch_port_t streamed_touch_port {0, 720, 1920, 1080, 0, 0};
  const auto primary_touch_port = platf::win_input::make_primary_display_touch_port({0, -720, 1920, -1});
  ASSERT_FALSE(primary_touch_port);

  const auto selected_touch_port = platf::win_input::select_touch_port(streamed_touch_port, true, primary_touch_port);

  expect_touch_ports_equal(selected_touch_port, streamed_touch_port);
}

#endif  // _WIN32
