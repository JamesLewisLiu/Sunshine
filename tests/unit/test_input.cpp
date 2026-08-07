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

  EXPECT_TRUE(densifier.observe(make_touch(LI_TOUCH_EVENT_DOWN, 1, 0.0f, 0.0f), start, 240).predictions.empty());
  EXPECT_TRUE(densifier.observe(make_touch(LI_TOUCH_EVENT_MOVE, 1, 0.08f, 0.0f), start + std::chrono::microseconds {8333}, 240).predictions.empty());
  const auto observation = densifier.observe(
    make_touch(LI_TOUCH_EVENT_MOVE, 1, 0.16f, 0.0f),
    start + std::chrono::microseconds {16666},
    240
  );

  ASSERT_EQ(observation.predictions.size(), 1);
  const auto &prediction = observation.predictions.front();
  EXPECT_NEAR(prediction.touch.x, 0.20f, 0.001f);
  EXPECT_FLOAT_EQ(prediction.touch.y, 0.0f);
  EXPECT_EQ(prediction.touch.pressure_or_distance, 0.5f);
  EXPECT_NEAR((std::chrono::duration<double, std::milli>(prediction.delay).count()), 4.1667, 0.001);
  EXPECT_TRUE(densifier.consume(1, prediction.generation));
  EXPECT_FALSE(densifier.consume(1, prediction.generation));
}

TEST(InputTouchDensifierTest, Stable120HzMovementProducesThree480HzPredictions) {
  input::touch_densifier_t densifier;
  const auto start = input::touch_densifier_t::clock_t::time_point {};

  densifier.observe(make_touch(LI_TOUCH_EVENT_DOWN, 1, 0.0f, 0.0f), start, 480);
  densifier.observe(make_touch(LI_TOUCH_EVENT_MOVE, 1, 0.04f, 0.0f), start + std::chrono::microseconds {8333}, 480);
  const auto observation = densifier.observe(
    make_touch(LI_TOUCH_EVENT_MOVE, 1, 0.08f, 0.0f),
    start + std::chrono::microseconds {16666},
    480
  );

  ASSERT_EQ(observation.predictions.size(), 3);
  EXPECT_NEAR(observation.predictions[0].touch.x, 0.09f, 0.001f);
  EXPECT_NEAR(observation.predictions[1].touch.x, 0.10f, 0.001f);
  EXPECT_NEAR(observation.predictions[2].touch.x, 0.11f, 0.001f);
  EXPECT_NEAR((std::chrono::duration<double, std::milli>(observation.predictions[0].delay).count()), 2.0833, 0.001);
  EXPECT_NEAR((std::chrono::duration<double, std::milli>(observation.predictions[1].delay).count()), 4.1667, 0.001);
  EXPECT_NEAR((std::chrono::duration<double, std::milli>(observation.predictions[2].delay).count()), 6.25, 0.001);
  for (const auto &prediction : observation.predictions) {
    EXPECT_TRUE(densifier.consume(1, prediction.generation));
  }
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

  EXPECT_TRUE(observation.predictions.empty());
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

  EXPECT_TRUE(observation.predictions.empty());
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

  EXPECT_TRUE(observation.predictions.empty());
}

TEST(InputTouchDensifierTest, Stable100HzInputIsOutsideThe120HzExperiment) {
  input::touch_densifier_t densifier;
  const auto start = input::touch_densifier_t::clock_t::time_point {};

  densifier.observe(make_touch(LI_TOUCH_EVENT_DOWN, 1, 0.0f, 0.0f), start, 240);
  densifier.observe(make_touch(LI_TOUCH_EVENT_MOVE, 1, 0.08f, 0.0f), start + std::chrono::milliseconds {10}, 240);
  const auto observation = densifier.observe(
    make_touch(LI_TOUCH_EVENT_MOVE, 1, 0.16f, 0.0f),
    start + std::chrono::milliseconds {20},
    240
  );

  EXPECT_TRUE(observation.predictions.empty());
}

TEST(InputTouchDensifierTest, UpImmediatelyInvalidatesPendingPrediction) {
  input::touch_densifier_t densifier;
  const auto start = input::touch_densifier_t::clock_t::time_point {};

  densifier.observe(make_touch(LI_TOUCH_EVENT_DOWN, 7, 0.0f, 0.0f), start, 240);
  densifier.observe(make_touch(LI_TOUCH_EVENT_MOVE, 7, 0.08f, 0.0f), start + std::chrono::microseconds {8333}, 240);
  const auto predictions = densifier.observe(
                                      make_touch(LI_TOUCH_EVENT_MOVE, 7, 0.16f, 0.0f),
                                      start + std::chrono::microseconds {16666},
                                      240
  )
                             .predictions;
  ASSERT_EQ(predictions.size(), 1);

  const auto up = densifier.observe(
    make_touch(LI_TOUCH_EVENT_UP, 7, 0.16f, 0.0f),
    start + std::chrono::microseconds {17000},
    240
  );

  ASSERT_EQ(up.cancel_pointer_ids.size(), 1);
  EXPECT_EQ(up.cancel_pointer_ids.front(), 7);
  EXPECT_FALSE(densifier.consume(7, predictions.front().generation));
}

TEST(InputTouchDensifierTest, NewRealMoveInvalidatesAndReplacesPending480HzBatch) {
  input::touch_densifier_t densifier;
  const auto start = input::touch_densifier_t::clock_t::time_point {};

  densifier.observe(make_touch(LI_TOUCH_EVENT_DOWN, 1, 0.0f, 0.0f), start, 480);
  densifier.observe(make_touch(LI_TOUCH_EVENT_MOVE, 1, 0.04f, 0.0f), start + std::chrono::microseconds {8333}, 480);
  const auto first = densifier.observe(
    make_touch(LI_TOUCH_EVENT_MOVE, 1, 0.08f, 0.0f),
    start + std::chrono::microseconds {16666},
    480
  );
  ASSERT_EQ(first.predictions.size(), 3);

  const auto correction = densifier.observe(
    make_touch(LI_TOUCH_EVENT_MOVE, 1, 0.12f, 0.0f),
    start + std::chrono::microseconds {24999},
    480
  );

  EXPECT_EQ(correction.cancel_pointer_ids, std::vector<std::uint32_t> {1});
  ASSERT_EQ(correction.predictions.size(), 3);
  for (const auto &prediction : first.predictions) {
    EXPECT_FALSE(densifier.consume(1, prediction.generation));
  }
  for (const auto &prediction : correction.predictions) {
    EXPECT_TRUE(densifier.consume(1, prediction.generation));
  }
}

TEST(InputTouchDensifierTest, CancelAllClearsPendingPredictionAndHistory) {
  input::touch_densifier_t densifier;
  const auto start = input::touch_densifier_t::clock_t::time_point {};

  densifier.observe(make_touch(LI_TOUCH_EVENT_DOWN, 1, 0.0f, 0.0f), start, 240);
  densifier.observe(make_touch(LI_TOUCH_EVENT_MOVE, 1, 0.08f, 0.0f), start + std::chrono::microseconds {8333}, 240);
  const auto predictions = densifier.observe(
                                      make_touch(LI_TOUCH_EVENT_MOVE, 1, 0.16f, 0.0f),
                                      start + std::chrono::microseconds {16666},
                                      240
  )
                             .predictions;
  ASSERT_EQ(predictions.size(), 1);

  const auto cancel_all = densifier.observe(
    make_touch(LI_TOUCH_EVENT_CANCEL_ALL, 0, 0.0f, 0.0f),
    start + std::chrono::microseconds {17000},
    240
  );

  EXPECT_EQ(cancel_all.cancel_pointer_ids, std::vector<std::uint32_t> {1});
  EXPECT_FALSE(densifier.consume(1, predictions.front().generation));
  EXPECT_TRUE(densifier.observe(make_touch(LI_TOUCH_EVENT_MOVE, 1, 0.24f, 0.0f), start + std::chrono::microseconds {25000}, 240).predictions.empty());
}

TEST(InputTouchDensifierTest, MultiTouchCancelsAndSuppressesPrediction) {
  input::touch_densifier_t densifier;
  const auto start = input::touch_densifier_t::clock_t::time_point {};

  densifier.observe(make_touch(LI_TOUCH_EVENT_DOWN, 1, 0.0f, 0.0f), start, 240);
  densifier.observe(make_touch(LI_TOUCH_EVENT_MOVE, 1, 0.08f, 0.0f), start + std::chrono::microseconds {8333}, 240);
  const auto first_predictions = densifier.observe(
                                            make_touch(LI_TOUCH_EVENT_MOVE, 1, 0.16f, 0.0f),
                                            start + std::chrono::microseconds {16666},
                                            240
  )
                                   .predictions;
  ASSERT_EQ(first_predictions.size(), 1);

  const auto second_down = densifier.observe(
    make_touch(LI_TOUCH_EVENT_DOWN, 2, 0.5f, 0.5f),
    start + std::chrono::microseconds {17000},
    240
  );
  EXPECT_EQ(second_down.cancel_pointer_ids, std::vector<std::uint32_t> {1});
  EXPECT_FALSE(densifier.consume(1, first_predictions.front().generation));

  densifier.observe(make_touch(LI_TOUCH_EVENT_MOVE, 1, 0.3f, 0.0f), start + std::chrono::microseconds {25000}, 240);
  const auto multi_touch_move = densifier.observe(
    make_touch(LI_TOUCH_EVENT_MOVE, 1, 0.4f, 0.0f),
    start + std::chrono::microseconds {33333},
    240
  );
  EXPECT_TRUE(multi_touch_move.predictions.empty());
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

  EXPECT_TRUE(observation.predictions.empty());
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
