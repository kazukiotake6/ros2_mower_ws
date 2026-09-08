// Copyright 2026 Mower maintainers
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace mower_camera
{
enum class DiagnosticLevel : uint8_t { ok = 0, warn = 1, error = 2 };

struct DiagnosticThresholds
{
  std::optional<double> minimum_fps_warn;
  std::optional<double> minimum_fps_error;
  std::optional<double> maximum_queue_latency_ms_warn;
  std::optional<double> maximum_queue_latency_ms_error;
  std::optional<uint64_t> missing_frames_warn;
  std::optional<uint64_t> missing_frames_error;
  std::optional<uint64_t> request_failures_warn;
  std::optional<uint64_t> request_failures_error;
};

struct CameraDiagnosticSnapshot
{
  uint64_t frames{};
  uint64_t missing_frames{};
  uint64_t request_failures{};
  uint64_t reinitializations{};
  uint64_t timestamp_regressions{};
  double effective_fps{};
  double maximum_queue_latency_ms{};
  DiagnosticLevel level{DiagnosticLevel::ok};
  std::string message{"streaming"};
};

class CameraDiagnostics
{
public:
  explicit CameraDiagnostics(DiagnosticThresholds thresholds = {});
  void record_frame(uint64_t sequence, int64_t capture_time_ns, int64_t completion_time_ns);
  void record_request_failure();
  void record_reinitialization();
  CameraDiagnosticSnapshot snapshot() const;
  void reset_window();

private:
  void validate_thresholds() const;
  static uint64_t saturated_add(uint64_t value, uint64_t increment = 1);

  DiagnosticThresholds thresholds_;
  uint64_t frames_{};
  uint64_t missing_frames_{};
  uint64_t request_failures_{};
  uint64_t reinitializations_{};
  uint64_t timestamp_regressions_{};
  double maximum_queue_latency_ms_{};
  std::optional<uint64_t> previous_sequence_;
  std::optional<int64_t> previous_capture_time_ns_;
  std::optional<int64_t> first_completion_time_ns_;
  std::optional<int64_t> last_completion_time_ns_;
};
}  // namespace mower_camera
