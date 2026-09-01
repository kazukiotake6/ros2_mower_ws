// Copyright 2026 Mower maintainers
// SPDX-License-Identifier: Apache-2.0

#include "mower_localization/vio_input_validator.hpp"
#include <utility>

namespace mower_localization
{
VioInputValidator::VioInputValidator(const VioInputLimits limits, const std::size_t imu_capacity)
: limits_(limits), imu_buffer_(imu_capacity) {}

bool VioInputValidator::configure(const bool calibration_approved)
{
  calibration_approved_ = calibration_approved;
  if (limits_.max_imu_gap_ns <= 0 || limits_.input_timeout_ns <= 0) {
    set_state(VioState::kError, "input limits must be explicitly configured"); return false;
  }
  if (!calibration_approved_) {
    set_state(VioState::kError, "approved calibration is required"); return false;
  }
  set_state(VioState::kInitializing, "waiting for synchronized inputs"); return true;
}

ImuInsertResult VioInputValidator::push_imu(const ImuSample & sample)
{
  if (state_ == VioState::kUnconfigured || state_ == VioState::kError) {
    return ImuInsertResult::kOutOfOrderTimestamp;
  }
  if (has_last_imu_stamp_ && sample.stamp_ns - last_imu_stamp_ns_ > limits_.max_imu_gap_ns) {
    imu_buffer_.reset(); has_last_imu_stamp_ = false;
    set_state(VioState::kDegraded, "IMU timestamp gap exceeds configured limit");
  }
  const auto result = imu_buffer_.push_imu(sample);
  if (result == ImuInsertResult::kAccepted) {
    last_imu_stamp_ns_ = sample.stamp_ns; has_last_imu_stamp_ = true;
  } else {set_state(VioState::kDegraded, "invalid IMU sample");}
  return result;
}

void VioInputValidator::update_camera_info(CameraInfoMetadata camera_info)
{camera_info_ = std::move(camera_info); has_camera_info_ = true;}

bool VioInputValidator::accept_image(const ImageMetadata & image)
{
  if (state_ == VioState::kUnconfigured || state_ == VioState::kError) {
    ++rejected_images_; return false;
  }
  if (!has_camera_info_ || !camera_info_.calibrated || camera_info_.stamp_ns != image.stamp_ns ||
    camera_info_.width != image.width || camera_info_.height != image.height ||
    camera_info_.frame_id != image.frame_id || image.frame_id.empty())
  {
    ++rejected_images_;
    set_state(VioState::kDegraded, "Image and calibrated CameraInfo do not match"); return false;
  }
  if (has_last_image_stamp_ && image.stamp_ns <= last_image_stamp_ns_) {
    ++rejected_images_;
    set_state(VioState::kDegraded, "image timestamp is duplicate or out of order"); return false;
  }
  if (imu_buffer_.take_imu_through(image.stamp_ns).empty()) {
    ++rejected_images_;
    set_state(VioState::kInitializing, "waiting for IMU samples through image timestamp");
    return false;
  }
  has_last_image_stamp_ = true; last_image_stamp_ns_ = image.stamp_ns;
  set_state(VioState::kInitializing, "image accepted; waiting for estimator result"); return true;
}

void VioInputValidator::report_estimator_tracking(const bool tracking)
{
  if (state_ != VioState::kError && state_ != VioState::kUnconfigured) {
    set_state(tracking ? VioState::kTracking : VioState::kLost,
      tracking ? "estimator tracking" : "estimator tracking lost");
  }
}
void VioInputValidator::report_input_timeout()
{
  if (state_ != VioState::kUnconfigured && state_ != VioState::kError) {
    set_state(VioState::kLost, "sensor input timeout");
  }
}
VioState VioInputValidator::state() const noexcept {return state_;}
const std::string & VioInputValidator::reason() const noexcept {return reason_;}
std::size_t VioInputValidator::rejected_images() const noexcept {return rejected_images_;}
void VioInputValidator::report_estimator_error(std::string reason)
{
  set_state(VioState::kError, std::move(reason));
}
void VioInputValidator::set_state(const VioState state, std::string reason)
{state_ = state; reason_ = std::move(reason);}
const char * to_string(const VioState state) noexcept
{
  switch (state) {
    case VioState::kUnconfigured: return "UNCONFIGURED";
    case VioState::kInitializing: return "INITIALIZING";
    case VioState::kTracking: return "TRACKING"; case VioState::kDegraded: return "DEGRADED";
    case VioState::kLost: return "LOST"; case VioState::kError: return "ERROR";
  }
  return "ERROR";
}
}  // namespace mower_localization
