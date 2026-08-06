/**
 * @file src/touch_densifier.cpp
 * @brief Conservative touch sample densification implementation.
 */

// standard includes
#include <algorithm>
#include <cmath>
#include <unordered_map>

// third-party includes
extern "C" {
#include <moonlight-common-c/src/Limelight.h>
}

// local includes
#include "touch_densifier.h"

namespace input {
  namespace {
    constexpr int TOUCH_DENSIFICATION_HZ = 240;  ///< Supported experimental touch injection frequency.
    constexpr double MIN_SOURCE_PERIODS = 1.75;  ///< Minimum source interval relative to the target interval.
    constexpr double MAX_SOURCE_PERIODS = 2.25;  ///< Maximum source interval relative to the target interval.
    constexpr double MIN_INTERVAL_RATIO = 0.75;  ///< Minimum stable interval ratio relative to the previous sample.
    constexpr double MAX_INTERVAL_RATIO = 1.25;  ///< Maximum stable interval ratio relative to the previous sample.
    constexpr auto MAX_PREDICTION_DELAY = std::chrono::milliseconds {5};  ///< Maximum time horizon for one prediction.
    constexpr float MAX_PREDICTION_DISTANCE = 0.05f;  ///< Maximum normalized distance of one prediction.
    constexpr float MAX_PREDICTION_VELOCITY = 12.0f;  ///< Maximum normalized motion velocity in screen lengths per second.
  }  // namespace

  /**
   * @brief Private state for conservative touch sample prediction.
   */
  struct touch_densifier_t::impl_t {
    /**
     * @brief Real sample history for one active touch contact.
     */
    struct contact_t {
      touch_sample_t last_touch;  ///< Most recent real touch sample.
      clock_t::time_point last_arrival;  ///< Host arrival time of the most recent real sample.
      std::optional<clock_t::duration> previous_interval;  ///< Previous real sample interval used for stability checks.
      std::uint64_t generation {};  ///< Most recent prediction generation for this contact.
      bool prediction_pending {};  ///< Whether delayed work exists for this contact.
    };

    std::unordered_map<std::uint32_t, contact_t> contacts;  ///< Active contacts keyed by client pointer ID.
    std::uint64_t next_generation {1};  ///< Monotonically increasing delayed-work generation.
  };

  touch_densifier_t::touch_densifier_t():
      impl_ {std::make_unique<impl_t>()} {
  }

  touch_densifier_t::~touch_densifier_t() = default;
  touch_densifier_t::touch_densifier_t(touch_densifier_t &&) noexcept = default;
  touch_densifier_t &touch_densifier_t::operator=(touch_densifier_t &&) noexcept = default;

  touch_densifier_t::observation_t touch_densifier_t::observe(
    const touch_sample_t &touch,
    clock_t::time_point arrival,
    int target_hz
  ) {
    observation_t result;

    const auto request_cancellation = [&result](std::uint32_t pointer_id) {
      if (std::find(result.cancel_pointer_ids.begin(), result.cancel_pointer_ids.end(), pointer_id) == result.cancel_pointer_ids.end()) {
        result.cancel_pointer_ids.push_back(pointer_id);
      }
    };
    const auto invalidate_pending = [&request_cancellation](auto &contacts) {
      for (auto &[pointer_id, contact] : contacts) {
        if (contact.prediction_pending) {
          request_cancellation(pointer_id);
          contact.prediction_pending = false;
        }
        ++contact.generation;
        contact.previous_interval.reset();
      }
    };

    if (target_hz != TOUCH_DENSIFICATION_HZ) {
      invalidate_pending(impl_->contacts);
      impl_->contacts.clear();
      return result;
    }

    if (touch.event_type == LI_TOUCH_EVENT_CANCEL_ALL) {
      invalidate_pending(impl_->contacts);
      impl_->contacts.clear();
      return result;
    }

    auto existing = impl_->contacts.find(touch.pointer_id);
    if (existing != impl_->contacts.end() && existing->second.prediction_pending) {
      request_cancellation(touch.pointer_id);
      existing->second.prediction_pending = false;
      ++existing->second.generation;
    }

    if (touch.event_type == LI_TOUCH_EVENT_UP || touch.event_type == LI_TOUCH_EVENT_CANCEL || touch.event_type == LI_TOUCH_EVENT_HOVER ||
        touch.event_type == LI_TOUCH_EVENT_HOVER_LEAVE) {
      if (existing != impl_->contacts.end()) {
        impl_->contacts.erase(existing);
      }
      if (impl_->contacts.size() == 1) {
        impl_->contacts.begin()->second.previous_interval.reset();
        impl_->contacts.begin()->second.last_arrival = arrival;
      }
      return result;
    }

    if (touch.event_type == LI_TOUCH_EVENT_DOWN) {
      impl_->contacts.insert_or_assign(touch.pointer_id, impl_t::contact_t {touch, arrival, std::nullopt, impl_->next_generation++, false});
      if (impl_->contacts.size() > 1) {
        invalidate_pending(impl_->contacts);
      }
      return result;
    }

    if (touch.event_type == LI_TOUCH_EVENT_BUTTON_ONLY) {
      if (existing != impl_->contacts.end()) {
        existing->second.previous_interval.reset();
        existing->second.last_arrival = arrival;
      }
      return result;
    }

    if (touch.event_type != LI_TOUCH_EVENT_MOVE) {
      return result;
    }

    if (existing == impl_->contacts.end()) {
      existing = impl_->contacts.insert_or_assign(
                                  touch.pointer_id,
                                  impl_t::contact_t {touch, arrival, std::nullopt, impl_->next_generation++, false}
      )
                   .first;
      return result;
    }

    auto &contact = existing->second;
    const auto current_interval = arrival - contact.last_arrival;
    const auto prior_touch = contact.last_touch;

    contact.last_touch = touch;
    contact.last_arrival = arrival;

    if (impl_->contacts.size() != 1) {
      invalidate_pending(impl_->contacts);
      return result;
    }

    const auto target_period = std::chrono::duration_cast<clock_t::duration>(std::chrono::duration<double> {1.0 / target_hz});
    const double source_periods = std::chrono::duration<double>(current_interval).count() / std::chrono::duration<double>(target_period).count();
    const bool source_rate_supported = source_periods >= MIN_SOURCE_PERIODS && source_periods <= MAX_SOURCE_PERIODS;

    bool intervals_stable = false;
    if (contact.previous_interval && current_interval > clock_t::duration::zero() && *contact.previous_interval > clock_t::duration::zero()) {
      const double interval_ratio = std::chrono::duration<double>(current_interval).count() /
                                    std::chrono::duration<double>(*contact.previous_interval).count();
      intervals_stable = interval_ratio >= MIN_INTERVAL_RATIO && interval_ratio <= MAX_INTERVAL_RATIO;
    }
    contact.previous_interval = current_interval;

    if (!source_rate_supported || !intervals_stable) {
      return result;
    }

    if (target_period > MAX_PREDICTION_DELAY) {
      return result;
    }

    const float current_seconds = static_cast<float>(std::chrono::duration<double>(current_interval).count());
    const float current_velocity = std::hypot(touch.x - prior_touch.x, touch.y - prior_touch.y) / current_seconds;
    if (!std::isfinite(current_velocity) || current_velocity > MAX_PREDICTION_VELOCITY) {
      return result;
    }

    const float prediction_scale = static_cast<float>(
      std::chrono::duration<double>(target_period).count() /
      std::chrono::duration<double>(current_interval).count()
    );
    const float prediction_dx = (touch.x - prior_touch.x) * prediction_scale;
    const float prediction_dy = (touch.y - prior_touch.y) * prediction_scale;
    const float prediction_distance = std::hypot(prediction_dx, prediction_dy);
    if (!std::isfinite(prediction_distance) || prediction_distance <= 0.0f || prediction_distance > MAX_PREDICTION_DISTANCE) {
      return result;
    }

    auto predicted_touch = touch;
    predicted_touch.x = std::clamp(touch.x + prediction_dx, 0.0f, 1.0f);
    predicted_touch.y = std::clamp(touch.y + prediction_dy, 0.0f, 1.0f);

    contact.generation = impl_->next_generation++;
    contact.prediction_pending = true;
    result.prediction = prediction_t {predicted_touch, target_period, contact.generation};
    return result;
  }

  bool touch_densifier_t::consume(std::uint32_t pointer_id, std::uint64_t generation) {
    const auto contact = impl_->contacts.find(pointer_id);
    if (contact == impl_->contacts.end() || !contact->second.prediction_pending || contact->second.generation != generation) {
      return false;
    }

    contact->second.prediction_pending = false;
    return true;
  }

  std::vector<std::uint32_t> touch_densifier_t::reset() {
    std::vector<std::uint32_t> pending_pointer_ids;
    for (const auto &[pointer_id, contact] : impl_->contacts) {
      if (contact.prediction_pending) {
        pending_pointer_ids.push_back(pointer_id);
      }
    }
    impl_->contacts.clear();
    return pending_pointer_ids;
  }
}  // namespace input
