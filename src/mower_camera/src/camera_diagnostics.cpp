// Copyright 2026 Mower maintainers
// SPDX-License-Identifier: Apache-2.0
#include "mower_camera/camera_diagnostics.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace mower_camera
{
namespace
{
template<typename T>
bool reached(const T value, const std::optional<T> & threshold)
{
  return threshold && value >= *threshold;
}

void raise(
  CameraDiagnosticSnapshot & result, const DiagnosticLevel level, const std::string & message)
{
  if (static_cast<uint8_t>(level) > static_cast<uint8_t>(result.level)) {
    result.level = level;
    result.message = message;
  }
}
}  // namespace

CameraDiagnostics::CameraDiagnostics(DiagnosticThresholds thresholds)
: thresholds_(std::move(thresholds))
{
  validate_thresholds();
}

void CameraDiagnostics::validate_thresholds() const
{
  const auto finite_positive = [](const std::optional<double> & value) {
      return !value || (std::isfinite(*value) && *value > 0.0);
    };
  if (!finite_positive(thresholds_.minimum_fps_warn) ||
    !finite_positive(thresholds_.minimum_fps_error) ||
    !finite_positive(thresholds_.maximum_queue_latency_ms_warn) ||
    !finite_positive(thresholds_.maximum_queue_latency_ms_error))
  {
    throw std::invalid_argument("diagnostic floating-point thresholds must be finite and positive");
  }
  if (thresholds_.minimum_fps_warn && thresholds_.minimum_fps_error &&
    *thresholds_.minimum_fps_error > *thresholds_.minimum_fps_warn)
  {
    throw std::invalid_argument("minimum_fps_error must not exceed minimum_fps_warn");
  }
  if (thresholds_.maximum_queue_latency_ms_warn && thresholds_.maximum_queue_latency_ms_error &&
    *thresholds_.maximum_queue_latency_ms_error < *thresholds_.maximum_queue_latency_ms_warn)
  {
    throw std::invalid_argument("maximum_queue_latency_ms_error must not be below warn");
  }
  const auto positive_count = [](const std::optional<uint64_t> & value) {
      return !value || *value > 0;
    };
  if (!positive_count(thresholds_.missing_frames_warn) ||
    !positive_count(thresholds_.missing_frames_error) ||
    !positive_count(thresholds_.request_failures_warn) ||
    !positive_count(thresholds_.request_failures_error))
  {
    throw std::invalid_argument("diagnostic count thresholds must be positive");
  }
  if (thresholds_.missing_frames_warn && thresholds_.missing_frames_error &&
    *thresholds_.missing_frames_error < *thresholds_.missing_frames_warn)
  {
    throw std::invalid_argument("missing_frames_error must not be below warn");
  }
  if (thresholds_.request_failures_warn && thresholds_.request_failures_error &&
    *thresholds_.request_failures_error < *thresholds_.request_failures_warn)
  {
    throw std::invalid_argument("request_failures_error must not be below warn");
  }
}

uint64_t CameraDiagnostics::saturated_add(const uint64_t value, const uint64_t increment)
{
  const auto maximum = std::numeric_limits<uint64_t>::max();
  return increment > maximum - value ? maximum : value + increment;
}

void CameraDiagnostics::record_frame(
  const uint64_t sequence, const int64_t capture_time_ns, const int64_t completion_time_ns)
{
  frames_ = saturated_add(frames_);
  if (previous_sequence_ && sequence > *previous_sequence_) {
    missing_frames_ = saturated_add(missing_frames_, sequence - *previous_sequence_ - 1);
  }
  if (previous_capture_time_ns_ && capture_time_ns <= *previous_capture_time_ns_) {
    timestamp_regressions_ = saturated_add(timestamp_regressions_);
  }
  if (completion_time_ns >= capture_time_ns) {
    maximum_queue_latency_ms_ = std::max(
      maximum_queue_latency_ms_, static_cast<double>(completion_time_ns - capture_time_ns) / 1e6);
  }
  previous_sequence_ = sequence;
  previous_capture_time_ns_ = capture_time_ns;
  if (!first_completion_time_ns_) first_completion_time_ns_ = completion_time_ns;
  last_completion_time_ns_ = completion_time_ns;
}

void CameraDiagnostics::record_request_failure()
{
  request_failures_ = saturated_add(request_failures_);
}

void CameraDiagnostics::record_reinitialization()
{
  reinitializations_ = saturated_add(reinitializations_);
}

CameraDiagnosticSnapshot CameraDiagnostics::snapshot() const
{
  CameraDiagnosticSnapshot result;
  result.frames = frames_;
  result.missing_frames = missing_frames_;
  result.request_failures = request_failures_;
  result.reinitializations = reinitializations_;
  result.timestamp_regressions = timestamp_regressions_;
  result.maximum_queue_latency_ms = maximum_queue_latency_ms_;
  if (frames_ > 1 && first_completion_time_ns_ && last_completion_time_ns_ &&
    *last_completion_time_ns_ > *first_completion_time_ns_)
  {
    result.effective_fps = static_cast<double>(frames_ - 1) * 1e9 /
      static_cast<double>(*last_completion_time_ns_ - *first_completion_time_ns_);
  }

  if (thresholds_.minimum_fps_warn && frames_ > 1 &&
    result.effective_fps < *thresholds_.minimum_fps_warn)
  {
    raise(result, DiagnosticLevel::warn, "effective FPS below warning threshold");
  }
  if (thresholds_.minimum_fps_error && frames_ > 1 &&
    result.effective_fps < *thresholds_.minimum_fps_error)
  {
    raise(result, DiagnosticLevel::error, "effective FPS below error threshold");
  }
  if (reached(result.maximum_queue_latency_ms, thresholds_.maximum_queue_latency_ms_warn)) {
    raise(result, DiagnosticLevel::warn, "queue latency reached warning threshold");
  }
  if (reached(result.maximum_queue_latency_ms, thresholds_.maximum_queue_latency_ms_error)) {
    raise(result, DiagnosticLevel::error, "queue latency reached error threshold");
  }
  if (reached(result.missing_frames, thresholds_.missing_frames_warn)) {
    raise(result, DiagnosticLevel::warn, "missing frames reached warning threshold");
  }
  if (reached(result.missing_frames, thresholds_.missing_frames_error)) {
    raise(result, DiagnosticLevel::error, "missing frames reached error threshold");
  }
  if (reached(result.request_failures, thresholds_.request_failures_warn)) {
    raise(result, DiagnosticLevel::warn, "request failures reached warning threshold");
  }
  if (reached(result.request_failures, thresholds_.request_failures_error)) {
    raise(result, DiagnosticLevel::error, "request failures reached error threshold");
  }
  if (timestamp_regressions_ > 0) {
    raise(result, DiagnosticLevel::error, "capture timestamp is not monotonic");
  }
  return result;
}

void CameraDiagnostics::reset_window()
{
  frames_ = 0;
  missing_frames_ = 0;
  request_failures_ = 0;
  timestamp_regressions_ = 0;
  maximum_queue_latency_ms_ = 0.0;
  first_completion_time_ns_.reset();
  last_completion_time_ns_.reset();
}
}  // namespace mower_camera
