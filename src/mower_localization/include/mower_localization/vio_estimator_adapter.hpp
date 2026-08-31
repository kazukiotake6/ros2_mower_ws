// Copyright 2026 Mower maintainers
// SPDX-License-Identifier: Apache-2.0

#ifndef MOWER_LOCALIZATION__VIO_ESTIMATOR_ADAPTER_HPP_
#define MOWER_LOCALIZATION__VIO_ESTIMATOR_ADAPTER_HPP_

#include <optional>
#include <string>

#include "nav_msgs/msg/odometry.hpp"
#include "sensor_msgs/msg/camera_info.hpp"
#include "sensor_msgs/msg/image.hpp"
#include "sensor_msgs/msg/imu.hpp"

namespace mower_localization
{

enum class EstimatorState {kInitializing, kTracking, kLost, kError};

struct EstimatorResult
{
  EstimatorState state{EstimatorState::kInitializing};
  std::optional<nav_msgs::msg::Odometry> odometry;
  std::string reason;
};

class VioEstimatorAdapter
{
public:
  virtual ~VioEstimatorAdapter() = default;
  virtual void reset() = 0;
  virtual bool add_imu(const sensor_msgs::msg::Imu & imu) = 0;
  virtual EstimatorResult process_image(
    const sensor_msgs::msg::Image & image,
    const sensor_msgs::msg::CameraInfo & camera_info) = 0;
};

}  // namespace mower_localization

#endif  // MOWER_LOCALIZATION__VIO_ESTIMATOR_ADAPTER_HPP_
