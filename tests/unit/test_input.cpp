/**
 * @file tests/unit/test_input.cpp
 * @brief Test input coordinate normalization helpers.
 */

// test includes
#include "../tests_common.h"

extern "C" {
#include <moonlight-common-c/src/Limelight.h>
}

// local includes
#include "src/input.h"

namespace {
  /**
   * @brief Build a normalized touch sample for densifier tests.
   *
   * @param event_type Moonlight touch event type.
   * @param pointer_id Client pointer identifier.
   * @param x Normalized horizontal coordinate.
   * @param y Normalized vertical coordinate.
   * @return Touch sample populated with stable contact attributes.
   */
  input::touch_sample_t make_touch(std::uint8_t event_type, std::uint32_t pointer_id, float x, float y) {
    return input::touch_sample_t {event_type, 0, pointer_id, x, y, 0.5f, 4.0f, 3.0f};
  }
}  // namespace

TEST(InputTouchDensifierTest, Stable120HzMovementProducesOne240HzPrediction) {
  input::touch_densifier_t densifier;
  const auto start = input::touch_densifier_t::clock_t::time_point {};

  EXPECT_FALSE(densifier.observe(make_touch(LI_TOUCH_EVENT_DOWN, 1, 0.0f, 0.0f), start, 240).prediction);
  EXPECT_FALSE(densifier.observe(make_touch(LI_TOUCH_EVENT_MOVE, 1, 0.08f, 0.0f), start + std::chrono::microseconds {8333}, 240).prediction);
  const auto observation = densifier.observe(
    make_touch(LI_TOUCH_EVENT_MOVE, 1, 0.16f, 0.0f),
    start + std::chrono::microseconds {16666},
    240
  );

  ASSERT_TRUE(observation.prediction);
  EXPECT_NEAR(observation.prediction->touch.x, 0.20f, 0.001f);
  EXPECT_FLOAT_EQ(observation.prediction->touch.y, 0.0f);
  EXPECT_EQ(observation.prediction->touch.pressure_or_distance, 0.5f);
  EXPECT_NEAR(std::chrono::duration<double, std::milli>(observation.prediction->delay).count(), 4.1667, 0.001);
  EXPECT_TRUE(densifier.consume(1, observation.prediction->generation));
  EXPECT_FALSE(densifier.consume(1, observation.prediction->generation));
}

TEST(InputTouchDensifierTest, DisabledModeNeverPredicts) {
  input::touch_densifier_t densifier;
  const auto start = input::touch_densifier_t::clock_t::time_point {};

  densifier.observe(make_touch(LI_TOUCH_EVENT_DOWN, 1, 0.0f, 0.0f), start, 240);
  densifier.observe(make_touch(LI_TOUCH_EVENT_MOVE, 1, 0.1f, 0.0f), start + std::chrono::microseconds {8333}, 240);
  const auto observation = densifier.observe(
    make_touch(LI_TOUCH_EVENT_MOVE, 1, 0.2f, 0.0f),
    start + std::chrono::microseconds {16666},
    0
  );

  EXPECT_FALSE(observation.prediction);
}

TEST(InputTouchDensifierTest, ArrivalJitterFallsBackToRealSamples) {
  input::touch_densifier_t densifier;
  const auto start = input::touch_densifier_t::clock_t::time_point {};

  densifier.observe(make_touch(LI_TOUCH_EVENT_DOWN, 1, 0.0f, 0.0f), start, 240);
  densifier.observe(make_touch(LI_TOUCH_EVENT_MOVE, 1, 0.1f, 0.0f), start + std::chrono::milliseconds {8}, 240);
  const auto observation = densifier.observe(
    make_touch(LI_TOUCH_EVENT_MOVE, 1, 0.2f, 0.0f),
    start + std::chrono::milliseconds {23},
    240
  );

  EXPECT_FALSE(observation.prediction);
}

TEST(InputTouchDensifierTest, Native240HzInputIsNotDensified) {
  input::touch_densifier_t densifier;
  const auto start = input::touch_densifier_t::clock_t::time_point {};

  densifier.observe(make_touch(LI_TOUCH_EVENT_DOWN, 1, 0.0f, 0.0f), start, 240);
  densifier.observe(make_touch(LI_TOUCH_EVENT_MOVE, 1, 0.1f, 0.0f), start + std::chrono::microseconds {4167}, 240);
  const auto observation = densifier.observe(
    make_touch(LI_TOUCH_EVENT_MOVE, 1, 0.2f, 0.0f),
    start + std::chrono::microseconds {8334},
    240
  );

  EXPECT_FALSE(observation.prediction);
}

TEST(InputTouchDensifierTest, Stable100HzInputIsOutsideThe120To240Experiment) {
  input::touch_densifier_t densifier;
  const auto start = input::touch_densifier_t::clock_t::time_point {};

  densifier.observe(make_touch(LI_TOUCH_EVENT_DOWN, 1, 0.0f, 0.0f), start, 240);
  densifier.observe(make_touch(LI_TOUCH_EVENT_MOVE, 1, 0.08f, 0.0f), start + std::chrono::milliseconds {10}, 240);
  const auto observation = densifier.observe(
    make_touch(LI_TOUCH_EVENT_MOVE, 1, 0.16f, 0.0f),
    start + std::chrono::milliseconds {20},
    240
  );

  EXPECT_FALSE(observation.prediction);
}

TEST(InputTouchDensifierTest, UpImmediatelyInvalidatesPendingPrediction) {
  input::touch_densifier_t densifier;
  const auto start = input::touch_densifier_t::clock_t::time_point {};

  densifier.observe(make_touch(LI_TOUCH_EVENT_DOWN, 7, 0.0f, 0.0f), start, 240);
  densifier.observe(make_touch(LI_TOUCH_EVENT_MOVE, 7, 0.08f, 0.0f), start + std::chrono::microseconds {8333}, 240);
  const auto prediction = densifier.observe(
                                     make_touch(LI_TOUCH_EVENT_MOVE, 7, 0.16f, 0.0f),
                                     start + std::chrono::microseconds {16666},
                                     240
  )
                            .prediction;
  ASSERT_TRUE(prediction);

  const auto up = densifier.observe(
    make_touch(LI_TOUCH_EVENT_UP, 7, 0.16f, 0.0f),
    start + std::chrono::microseconds {17000},
    240
  );

  ASSERT_EQ(up.cancel_pointer_ids.size(), 1);
  EXPECT_EQ(up.cancel_pointer_ids.front(), 7);
  EXPECT_FALSE(densifier.consume(7, prediction->generation));
}

TEST(InputTouchDensifierTest, NewRealMoveInvalidatesAndReplacesPendingPrediction) {
  input::touch_densifier_t densifier;
  const auto start = input::touch_densifier_t::clock_t::time_point {};

  densifier.observe(make_touch(LI_TOUCH_EVENT_DOWN, 1, 0.0f, 0.0f), start, 240);
  densifier.observe(make_touch(LI_TOUCH_EVENT_MOVE, 1, 0.08f, 0.0f), start + std::chrono::microseconds {8333}, 240);
  const auto first = densifier.observe(
    make_touch(LI_TOUCH_EVENT_MOVE, 1, 0.16f, 0.0f),
    start + std::chrono::microseconds {16666},
    240
  );
  ASSERT_TRUE(first.prediction);

  const auto correction = densifier.observe(
    make_touch(LI_TOUCH_EVENT_MOVE, 1, 0.24f, 0.0f),
    start + std::chrono::microseconds {24999},
    240
  );

  EXPECT_EQ(correction.cancel_pointer_ids, std::vector<std::uint32_t> {1});
  ASSERT_TRUE(correction.prediction);
  EXPECT_FALSE(densifier.consume(1, first.prediction->generation));
  EXPECT_TRUE(densifier.consume(1, correction.prediction->generation));
}

TEST(InputTouchDensifierTest, CancelAllClearsPendingPredictionAndHistory) {
  input::touch_densifier_t densifier;
  const auto start = input::touch_densifier_t::clock_t::time_point {};

  densifier.observe(make_touch(LI_TOUCH_EVENT_DOWN, 1, 0.0f, 0.0f), start, 240);
  densifier.observe(make_touch(LI_TOUCH_EVENT_MOVE, 1, 0.08f, 0.0f), start + std::chrono::microseconds {8333}, 240);
  const auto prediction = densifier.observe(
                                     make_touch(LI_TOUCH_EVENT_MOVE, 1, 0.16f, 0.0f),
                                     start + std::chrono::microseconds {16666},
                                     240
  )
                            .prediction;
  ASSERT_TRUE(prediction);

  const auto cancel_all = densifier.observe(
    make_touch(LI_TOUCH_EVENT_CANCEL_ALL, 0, 0.0f, 0.0f),
    start + std::chrono::microseconds {17000},
    240
  );

  EXPECT_EQ(cancel_all.cancel_pointer_ids, std::vector<std::uint32_t> {1});
  EXPECT_FALSE(densifier.consume(1, prediction->generation));
  EXPECT_FALSE(densifier.observe(make_touch(LI_TOUCH_EVENT_MOVE, 1, 0.24f, 0.0f), start + std::chrono::microseconds {25000}, 240).prediction);
}

TEST(InputTouchDensifierTest, MultiTouchCancelsAndSuppressesPrediction) {
  input::touch_densifier_t densifier;
  const auto start = input::touch_densifier_t::clock_t::time_point {};

  densifier.observe(make_touch(LI_TOUCH_EVENT_DOWN, 1, 0.0f, 0.0f), start, 240);
  densifier.observe(make_touch(LI_TOUCH_EVENT_MOVE, 1, 0.08f, 0.0f), start + std::chrono::microseconds {8333}, 240);
  const auto first_prediction = densifier.observe(
                                           make_touch(LI_TOUCH_EVENT_MOVE, 1, 0.16f, 0.0f),
                                           start + std::chrono::microseconds {16666},
                                           240
  )
                                  .prediction;
  ASSERT_TRUE(first_prediction);

  const auto second_down = densifier.observe(
    make_touch(LI_TOUCH_EVENT_DOWN, 2, 0.5f, 0.5f),
    start + std::chrono::microseconds {17000},
    240
  );
  EXPECT_EQ(second_down.cancel_pointer_ids, std::vector<std::uint32_t> {1});
  EXPECT_FALSE(densifier.consume(1, first_prediction->generation));

  densifier.observe(make_touch(LI_TOUCH_EVENT_MOVE, 1, 0.3f, 0.0f), start + std::chrono::microseconds {25000}, 240);
  const auto multi_touch_move = densifier.observe(
    make_touch(LI_TOUCH_EVENT_MOVE, 1, 0.4f, 0.0f),
    start + std::chrono::microseconds {33333},
    240
  );
  EXPECT_FALSE(multi_touch_move.prediction);
}

TEST(InputTouchDensifierTest, ExcessivePredictionDistanceFallsBackToRealSample) {
  input::touch_densifier_t densifier;
  const auto start = input::touch_densifier_t::clock_t::time_point {};

  densifier.observe(make_touch(LI_TOUCH_EVENT_DOWN, 1, 0.0f, 0.0f), start, 240);
  densifier.observe(make_touch(LI_TOUCH_EVENT_MOVE, 1, 0.2f, 0.0f), start + std::chrono::microseconds {8333}, 240);
  const auto observation = densifier.observe(
    make_touch(LI_TOUCH_EVENT_MOVE, 1, 0.4f, 0.0f),
    start + std::chrono::microseconds {16666},
    240
  );

  EXPECT_FALSE(observation.prediction);
}

TEST(InputTouchPortTest, EncodedPillarboxUsesStreamedDisplayResolution) {
  const input::touch_port_t touch_port {
    {3840, 0, 1920, 1080},
    5120,
    2160,
    60.0f,
    0.0f,
    32.0f / 45.0f,
    1.0f,
    0,
    0
  };
  std::pair coords {5120.0f, 768.0f};

  const auto normalized_port = input::monitor_touch_port(touch_port, coords, true);

  ASSERT_TRUE(normalized_port);
  EXPECT_EQ(normalized_port->width, 1280);
  EXPECT_EQ(normalized_port->height, 768);
  EXPECT_FLOAT_EQ(coords.first, 1.0f);
  EXPECT_FLOAT_EQ(coords.second, 1.0f);
}

TEST(InputTouchPortTest, EncodedLetterboxCoversBottomOfPrimaryDisplay) {
  const input::touch_port_t touch_port {
    {1280, 0, 1280, 1024},
    2560,
    1024,
    0.0f,
    128.0f,
    1.0f,
    1.0f,
    0,
    0
  };
  std::pair coords {640.0f + touch_port.offset_x, 768.0f + touch_port.offset_y};

  const auto normalized_port = input::monitor_touch_port(touch_port, coords, true);

  ASSERT_TRUE(normalized_port);
  EXPECT_EQ(normalized_port->width, 1280);
  EXPECT_EQ(normalized_port->height, 768);
  EXPECT_FLOAT_EQ(coords.first, 0.5f);
  EXPECT_FLOAT_EQ(coords.second, 1.0f);

  constexpr platf::touch_port_t primary_display {0, 0, 3840, 2160};
  EXPECT_FLOAT_EQ(coords.first * primary_display.width, 1920.0f);
  EXPECT_FLOAT_EQ(coords.second * primary_display.height, 2160.0f);
}

TEST(InputTouchPortTest, NormalizedCoordinatesAreClampedToDisplayBounds) {
  const input::touch_port_t touch_port {
    {0, 0, 1280, 768},
    1280,
    768,
    0.0f,
    0.0f,
    1.0f,
    1.0f,
    0,
    0
  };
  std::pair coords {-1.0f, 769.0f};

  ASSERT_TRUE(input::monitor_touch_port(touch_port, coords, true));
  EXPECT_FLOAT_EQ(coords.first, 0.0f);
  EXPECT_FLOAT_EQ(coords.second, 1.0f);
}

TEST(InputTouchPortTest, InvalidEffectiveContentDimensionsAreRejected) {
  const input::touch_port_t touch_port {
    {0, 0, 1280, 768},
    1280,
    768,
    640.0f,
    0.0f,
    1.0f,
    1.0f,
    0,
    0
  };
  std::pair coords {0.0f, 0.0f};

  EXPECT_FALSE(input::monitor_touch_port(touch_port, coords, true));
}

TEST(InputTouchPortTest, InvalidCoordinateScaleIsRejected) {
  const input::touch_port_t touch_port {
    {0, 0, 1280, 768},
    1280,
    768,
    0.0f,
    0.0f,
    1.0f,
    0.0f,
    0,
    0
  };
  std::pair coords {0.0f, 0.0f};

  EXPECT_FALSE(input::monitor_touch_port(touch_port, coords, false));
}

TEST(InputTouchPortTest, EncodedFrameNormalizationIsPreservedWhenPrimaryMappingIsDisabled) {
  const input::touch_port_t touch_port {
    {1280, 0, 1280, 1024},
    2560,
    1024,
    0.0f,
    128.0f,
    1.0f,
    1.0f,
    0,
    0
  };
  std::pair coords {640.0f + touch_port.offset_x, 768.0f + touch_port.offset_y};

  const auto normalized_port = input::monitor_touch_port(touch_port, coords, false);

  ASSERT_TRUE(normalized_port);
  EXPECT_EQ(normalized_port->width, 1280);
  EXPECT_EQ(normalized_port->height, 1024);
  EXPECT_FLOAT_EQ(coords.first, 0.5f);
  EXPECT_FLOAT_EQ(coords.second, 0.75f);
}
