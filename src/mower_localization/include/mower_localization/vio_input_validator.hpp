// Copyright 2026 Mower maintainers
// SPDX-License-Identifier: Apache-2.0

#ifndef MOWER_LOCALIZATION__VIO_INPUT_VALIDATOR_HPP_
#define MOWER_LOCALIZATION__VIO_INPUT_VALIDATOR_HPP_

#include <cstdint>
#include <string>

#include "mower_localization/vio_input_buffer.hpp"

namespace mower_localization
{

enum class VioState {kUnconfigured, kInitializing, kTracking, kDegraded, kLost, kError};

struct VioInputLimits {std::int64_t max_imu_gap_ns{}; std::int64_t input_timeout_ns{};};
struct CameraInfoMetadata {
  std::int64_t stamp_ns{}; std::uint32_t width{}; std::uint32_t height{};
  std::string frame_id; bool calibrated{};
};
struct ImageMetadata {
  std::int64_t stamp_ns{}; std::uint32_t width{}; std::uint32_t height{}; std::string frame_id;
};

class VioInputValidator
{
public:
  VioInputValidator(VioInputLimits limits, std::size_t imu_capacity);
  bool configure(bool calibration_approved);
  ImuInsertResult push_imu(const ImuSample & sample);
  void update_camera_info(CameraInfoMetadata camera_info);
  bool accept_image(const ImageMetadata & image);
  void report_estimator_tracking(bool tracking);
  void report_input_timeout();
  [[nodiscard]] VioState state() const noexcept;
  [[nodiscard]] const std::string & reason() const noexcept;
  [[nodiscard]] std::size_t rejected_images() const noexcept;
private:
  void set_state(VioState state, std::string reason);
  VioInputLimits limits_;
  VioInputBuffer imu_buffer_;
  VioState state_{VioState::kUnconfigured};
  std::string reason_{"not configured"};
  bool calibration_approved_{false};
  bool has_camera_info_{false};
  CameraInfoMetadata camera_info_{};
  bool has_last_imu_stamp_{false}; std::int64_t last_imu_stamp_ns_{};
  bool has_last_image_stamp_{false}; std::int64_t last_image_stamp_ns_{};
  std::size_t rejected_images_{};
};

const char * to_string(VioState state) noexcept;
}  // namespace mower_localization
#endif  // MOWER_LOCALIZATION__VIO_INPUT_VALIDATOR_HPP_
