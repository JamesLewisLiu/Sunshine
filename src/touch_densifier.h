/**
 * @file src/touch_densifier.h
 * @brief Conservative touch sample densification for experimental 120 Hz to 240/480 Hz input.
 */
#pragma once

// standard includes
#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

namespace input {
  /**
   * @brief Platform-independent normalized touch state used by the densifier.
   */
  struct touch_sample_t {
    std::uint8_t event_type;  ///< Moonlight touch event type.
    std::uint16_t rotation;  ///< Degrees (0..360) or LI_ROT_UNKNOWN.
    std::uint32_t pointer_id;  ///< Client-provided pointer identifier.
    float x;  ///< Normalized horizontal coordinate.
    float y;  ///< Normalized vertical coordinate.
    float pressure_or_distance;  ///< Contact pressure or hover distance.
    float contact_area_major;  ///< Major axis of the reported contact area.
    float contact_area_minor;  ///< Minor axis of the reported contact area.
  };

  /**
   * @brief Builds conservative synthetic touch updates between real client samples.
   * @details The densifier never delays real samples. It emits at most one linear
   * extrapolation batch for a stable single-touch MOVE sequence and invalidates pending
   * predictions at every state boundary or newer real sample.
   */
  class touch_densifier_t {
  public:
    using clock_t = std::chrono::steady_clock;  ///< Monotonic clock used for observed arrival intervals.

    /**
     * @brief A synthetic touch update that may be scheduled for later injection.
     */
    struct prediction_t {
      touch_sample_t touch;  ///< Predicted touch state.
      clock_t::duration delay;  ///< Delay from the real sample before injection.
      std::uint64_t generation;  ///< Generation used to reject stale delayed work.
    };

    /**
     * @brief Result of observing a real touch update.
     */
    struct observation_t {
      std::vector<std::uint32_t> cancel_pointer_ids;  ///< Pointer predictions that must be cancelled.
      std::vector<prediction_t> predictions;  ///< New predictions, if the sample stream is stable enough.
    };

    /**
     * @brief Construct an empty touch densifier.
     */
    touch_densifier_t();

    /**
     * @brief Destroy the touch densifier.
     */
    ~touch_densifier_t();

    touch_densifier_t(const touch_densifier_t &) = delete;  ///< Touch history is stream-local and cannot be copied.
    touch_densifier_t &operator=(const touch_densifier_t &) = delete;  ///< Touch history is stream-local and cannot be copied.

    /**
     * @brief Move touch history from another densifier.
     */
    touch_densifier_t(touch_densifier_t &&) noexcept;

    /**
     * @brief Replace this densifier with moved touch history.
     * @return This densifier.
     */
    touch_densifier_t &operator=(touch_densifier_t &&) noexcept;

    /**
     * @brief Observe a real touch event and optionally create a prediction batch.
     *
     * @param touch Real touch event after coordinate normalization.
     * @param arrival Monotonic host arrival time for the event.
     * @param target_hz Requested injection frequency, or zero to disable densification.
     * @return Cancellation requests and zero or more delayed synthetic updates.
     */
    observation_t observe(const touch_sample_t &touch, clock_t::time_point arrival, int target_hz);

    /**
     * @brief Claim a prediction if no newer real event has invalidated it.
     *
     * @param pointer_id Pointer identifier carried by the delayed prediction.
     * @param generation Prediction generation returned by @ref observe.
     * @return True when the prediction is still current and may be injected.
     */
    bool consume(std::uint32_t pointer_id, std::uint64_t generation);

    /**
     * @brief Invalidate every pending prediction and clear all touch history.
     *
     * @return Pointer identifiers whose delayed tasks should be cancelled.
     */
    std::vector<std::uint32_t> reset();

  private:
    struct impl_t;
    std::unique_ptr<impl_t> impl_;  ///< Private touch history and generation state.
  };
}  // namespace input
