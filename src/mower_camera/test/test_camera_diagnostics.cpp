// Copyright 2026 Mower maintainers
// SPDX-License-Identifier: Apache-2.0
#include "mower_camera/camera_diagnostics.hpp"

#include <limits>
#include <stdexcept>

#include <gtest/gtest.h>

using mower_camera::CameraDiagnostics;
using mower_camera::DiagnosticLevel;
using mower_camera::DiagnosticThresholds;

TEST(CameraDiagnostics, calculates_rate_latency_and_missing_frames)
{
  CameraDiagnostics diagnostics;
  diagnostics.record_frame(10, 0, 2000000);
  diagnostics.record_frame(12, 100000000, 103000000);
  const auto result = diagnostics.snapshot();
  EXPECT_EQ(result.frames, 2U);
  EXPECT_EQ(result.missing_frames, 1U);
  EXPECT_DOUBLE_EQ(result.maximum_queue_latency_ms, 3.0);
  EXPECT_NEAR(result.effective_fps, 9.90099, 0.00001);
  EXPECT_EQ(result.level, DiagnosticLevel::ok);
}

TEST(CameraDiagnostics, applies_only_injected_thresholds)
{
  DiagnosticThresholds thresholds;
  thresholds.minimum_fps_warn = 15.0;
  thresholds.minimum_fps_error = 5.0;
  thresholds.missing_frames_error = 2;
  CameraDiagnostics diagnostics(thresholds);
  diagnostics.record_frame(1, 0, 1000000);
  diagnostics.record_frame(3, 100000000, 101000000);
  EXPECT_EQ(diagnostics.snapshot().level, DiagnosticLevel::warn);
  diagnostics.record_frame(5, 200000000, 201000000);
  EXPECT_EQ(diagnostics.snapshot().level, DiagnosticLevel::error);
}

TEST(CameraDiagnostics, treats_timestamp_regression_as_error)
{
  CameraDiagnostics diagnostics;
  diagnostics.record_frame(1, 20, 30);
  diagnostics.record_frame(2, 20, 40);
  const auto result = diagnostics.snapshot();
  EXPECT_EQ(result.timestamp_regressions, 1U);
  EXPECT_EQ(result.level, DiagnosticLevel::error);
}

TEST(CameraDiagnostics, evaluatesLatencyAndRequestFailureThresholds)
{
  DiagnosticThresholds thresholds;
  thresholds.maximum_queue_latency_ms_warn = 2.0;
  thresholds.maximum_queue_latency_ms_error = 5.0;
  thresholds.request_failures_warn = 1;
  thresholds.request_failures_error = 2;
  CameraDiagnostics diagnostics(thresholds);
  diagnostics.record_frame(1, 0, 3000000);
  EXPECT_EQ(diagnostics.snapshot().level, DiagnosticLevel::warn);
  diagnostics.record_request_failure();
  diagnostics.record_request_failure();
  EXPECT_EQ(diagnostics.snapshot().request_failures, 2U);
  EXPECT_EQ(diagnostics.snapshot().level, DiagnosticLevel::error);
}

TEST(CameraDiagnostics, resets_window_but_retains_stream_continuity_and_restart_count)
{
  CameraDiagnostics diagnostics;
  diagnostics.record_frame(4, 100, 110);
  diagnostics.record_reinitialization();
  diagnostics.reset_window();
  diagnostics.record_frame(6, 200, 210);
  const auto result = diagnostics.snapshot();
  EXPECT_EQ(result.frames, 1U);
  EXPECT_EQ(result.missing_frames, 1U);
  EXPECT_EQ(result.reinitializations, 1U);
}

TEST(CameraDiagnostics, rejectsInvalidThresholdRelationships)
{
  DiagnosticThresholds thresholds;
  thresholds.minimum_fps_warn = 10.0;
  thresholds.minimum_fps_error = 20.0;
  EXPECT_THROW(CameraDiagnostics diagnostics(thresholds), std::invalid_argument);
  thresholds = {};
  thresholds.maximum_queue_latency_ms_warn = 20.0;
  thresholds.maximum_queue_latency_ms_error = 10.0;
  EXPECT_THROW(CameraDiagnostics diagnostics(thresholds), std::invalid_argument);
  thresholds = {};
  thresholds.missing_frames_warn = 0;
  EXPECT_THROW(CameraDiagnostics diagnostics(thresholds), std::invalid_argument);
  thresholds = {};
  thresholds.request_failures_warn = 2;
  thresholds.request_failures_error = 1;
  EXPECT_THROW(CameraDiagnostics diagnostics(thresholds), std::invalid_argument);
}

TEST(CameraDiagnostics, saturatesCounters)
{
  CameraDiagnostics diagnostics;
  diagnostics.record_frame(0, 1, 2);
  diagnostics.record_frame(std::numeric_limits<uint64_t>::max(), 2, 3);
  EXPECT_EQ(diagnostics.snapshot().missing_frames, std::numeric_limits<uint64_t>::max() - 1);
  diagnostics.record_frame(std::numeric_limits<uint64_t>::max(), 3, 4);
  EXPECT_EQ(diagnostics.snapshot().missing_frames, std::numeric_limits<uint64_t>::max() - 1);
}
