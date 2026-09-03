// Copyright 2026 Mower maintainers
// SPDX-License-Identifier: Apache-2.0

#ifndef MOWER_LOCALIZATION__BASALT_VIO_NODE_HPP_
#define MOWER_LOCALIZATION__BASALT_VIO_NODE_HPP_

#include <chrono>
#include <memory>
#include <optional>

#include "diagnostic_msgs/msg/diagnostic_array.hpp"
#include "diagnostic_msgs/msg/diagnostic_status.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_lifecycle/lifecycle_node.hpp"
#include "sensor_msgs/msg/camera_info.hpp"
#include "sensor_msgs/msg/image.hpp"
#include "sensor_msgs/msg/imu.hpp"

#include "mower_localization/vio_estimator_adapter.hpp"
#include "mower_localization/vio_input_validator.hpp"

namespace mower_localization
{

class BasaltVioNode : public rclcpp_lifecycle::LifecycleNode
{
public:
  explicit BasaltVioNode(
    std::shared_ptr<VioEstimatorAdapter> estimator = nullptr,
    const rclcpp::NodeOptions & options = rclcpp::NodeOptions());

  CallbackReturn on_configure(const rclcpp_lifecycle::State &) override;
  CallbackReturn on_activate(const rclcpp_lifecycle::State &) override;
  CallbackReturn on_deactivate(const rclcpp_lifecycle::State &) override;
  CallbackReturn on_cleanup(const rclcpp_lifecycle::State &) override;
  CallbackReturn on_shutdown(const rclcpp_lifecycle::State &) override;
  CallbackReturn on_error(const rclcpp_lifecycle::State &) override;

private:
  void record_image_input();
  void record_imu_input();
  void check_input_timeout();
  void on_imu(const sensor_msgs::msg::Imu & message);
  void on_image(const sensor_msgs::msg::Image & message);
  void handle_estimator_result(
    const EstimatorResult & result,
    const sensor_msgs::msg::Image & image);
  void publish_status();
  void reset_resources();

  std::shared_ptr<VioEstimatorAdapter> estimator_;
  std::unique_ptr<VioInputValidator> validator_;
  std::optional<sensor_msgs::msg::CameraInfo> camera_info_;
  rclcpp::TimerBase::SharedPtr timeout_timer_;
  std::chrono::steady_clock::time_point last_image_time_{};
  std::chrono::steady_clock::time_point last_imu_time_{};
  bool input_timed_out_{false};
  rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr image_subscription_;
  rclcpp::Subscription<sensor_msgs::msg::CameraInfo>::SharedPtr camera_info_subscription_;
  rclcpp::Subscription<sensor_msgs::msg::Imu>::SharedPtr imu_subscription_;
  rclcpp_lifecycle::LifecyclePublisher<nav_msgs::msg::Odometry>::SharedPtr odometry_publisher_;
  rclcpp_lifecycle::LifecyclePublisher<diagnostic_msgs::msg::DiagnosticStatus>::SharedPtr
    status_publisher_;
  rclcpp_lifecycle::LifecyclePublisher<diagnostic_msgs::msg::DiagnosticArray>::SharedPtr
    diagnostics_publisher_;
};

}  // namespace mower_localization

#endif  // MOWER_LOCALIZATION__BASALT_VIO_NODE_HPP_
