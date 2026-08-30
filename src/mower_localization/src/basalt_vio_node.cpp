// Copyright 2026 Mower maintainers
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include "diagnostic_msgs/msg/diagnostic_array.hpp"
#include "diagnostic_msgs/msg/diagnostic_status.hpp"
#include "diagnostic_msgs/msg/key_value.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_lifecycle/lifecycle_node.hpp"
#include "sensor_msgs/msg/camera_info.hpp"
#include "sensor_msgs/msg/image.hpp"
#include "sensor_msgs/msg/imu.hpp"

#include "mower_localization/vio_input_validator.hpp"

namespace mower_localization
{
namespace
{
std::int64_t to_nanoseconds(const builtin_interfaces::msg::Time & stamp)
{
  return static_cast<std::int64_t>(stamp.sec) * 1000000000LL + stamp.nanosec;
}
bool has_valid_calibration(const sensor_msgs::msg::CameraInfo & message)
{return message.k[0] > 0.0 && message.k[4] > 0.0 && !message.d.empty();}
}  // namespace

class BasaltVioNode : public rclcpp_lifecycle::LifecycleNode
{
public:
  BasaltVioNode() : LifecycleNode("basalt_vio_node")
  {
    declare_parameter<bool>("calibration_approved", false);
    declare_parameter<int>("max_imu_gap_ms", 0);
    declare_parameter<int>("input_timeout_ms", 0);
    declare_parameter<int>("imu_queue_capacity", 0);
  }

  CallbackReturn on_configure(const rclcpp_lifecycle::State &) override
  {
    const auto max_imu_gap_ms = get_parameter("max_imu_gap_ms").as_int();
    const auto input_timeout_ms = get_parameter("input_timeout_ms").as_int();
    const auto imu_queue_capacity = get_parameter("imu_queue_capacity").as_int();
    if (imu_queue_capacity <= 0) {
      RCLCPP_ERROR(get_logger(), "imu_queue_capacity must be explicitly configured and positive");
      return CallbackReturn::FAILURE;
    }
    validator_ = std::make_unique<VioInputValidator>(
      VioInputLimits{max_imu_gap_ms * 1000000LL, input_timeout_ms * 1000000LL},
      static_cast<std::size_t>(imu_queue_capacity));
    if (!validator_->configure(get_parameter("calibration_approved").as_bool())) {
      RCLCPP_ERROR(get_logger(), "%s", validator_->reason().c_str());
      validator_.reset();
      return CallbackReturn::FAILURE;
    }
    const auto sensor_qos = rclcpp::SensorDataQoS();
    image_subscription_ = create_subscription<sensor_msgs::msg::Image>(
      "/image_raw", sensor_qos, [this](sensor_msgs::msg::Image::ConstSharedPtr message) {on_image(*message);});
    camera_info_subscription_ = create_subscription<sensor_msgs::msg::CameraInfo>(
      "/camera_info", sensor_qos, [this](sensor_msgs::msg::CameraInfo::ConstSharedPtr message) {
        record_input();
        validator_->update_camera_info(CameraInfoMetadata{
          to_nanoseconds(message->header.stamp), message->width, message->height,
          message->header.frame_id, has_valid_calibration(*message)});
      });
    imu_subscription_ = create_subscription<sensor_msgs::msg::Imu>(
      "/imu/data_raw", sensor_qos, [this](sensor_msgs::msg::Imu::ConstSharedPtr message) {on_imu(*message);});
    odometry_publisher_ = create_publisher<nav_msgs::msg::Odometry>("/vio/odometry", 10);
    status_publisher_ = create_publisher<diagnostic_msgs::msg::DiagnosticStatus>("/vio/status", 10);
    diagnostics_publisher_ = create_publisher<diagnostic_msgs::msg::DiagnosticArray>("/diagnostics", 10);
    return CallbackReturn::SUCCESS;
  }

  CallbackReturn on_activate(const rclcpp_lifecycle::State &) override
  {
    odometry_publisher_->on_activate();
    status_publisher_->on_activate();
    diagnostics_publisher_->on_activate();
    last_input_time_ = std::chrono::steady_clock::now();
    input_timed_out_ = false;
    const auto timeout_ms = get_parameter("input_timeout_ms").as_int();
    const auto check_period_ms =
      std::max<std::int64_t>(1, std::min<std::int64_t>(timeout_ms / 2, 100));
    timeout_timer_ = create_wall_timer(
      std::chrono::milliseconds(check_period_ms), [this]() {check_input_timeout();});
    publish_status();
    return CallbackReturn::SUCCESS;
  }

  CallbackReturn on_deactivate(const rclcpp_lifecycle::State &) override
  {
    if (timeout_timer_) {timeout_timer_->cancel();}
    odometry_publisher_->on_deactivate();
    status_publisher_->on_deactivate();
    diagnostics_publisher_->on_deactivate();
    return CallbackReturn::SUCCESS;
  }

  CallbackReturn on_cleanup(const rclcpp_lifecycle::State &) override
  {
    reset_resources();
    return CallbackReturn::SUCCESS;
  }

  CallbackReturn on_shutdown(const rclcpp_lifecycle::State &) override
  {
    reset_resources();
    return CallbackReturn::SUCCESS;
  }

  CallbackReturn on_error(const rclcpp_lifecycle::State &) override
  {
    reset_resources();
    return CallbackReturn::SUCCESS;
  }

private:
  void record_input()
  {
    last_input_time_ = std::chrono::steady_clock::now();
    input_timed_out_ = false;
  }

  void check_input_timeout()
  {
    if (!validator_ || input_timed_out_) {return;}
    const auto timeout = std::chrono::milliseconds(get_parameter("input_timeout_ms").as_int());
    if (std::chrono::steady_clock::now() - last_input_time_ >= timeout) {
      validator_->report_input_timeout();
      input_timed_out_ = true;
      publish_status();
    }
  }

  void on_imu(const sensor_msgs::msg::Imu & message)
  {
    record_input();
    validator_->push_imu(ImuSample{to_nanoseconds(message.header.stamp),
      {message.angular_velocity.x, message.angular_velocity.y, message.angular_velocity.z},
      {message.linear_acceleration.x, message.linear_acceleration.y, message.linear_acceleration.z}});
    publish_status();
  }

  void on_image(const sensor_msgs::msg::Image & message)
  {
    record_input();
    const bool accepted = validator_->accept_image(ImageMetadata{
      to_nanoseconds(message.header.stamp), message.width, message.height, message.header.frame_id});
    if (accepted) {
      // Basalt is intentionally not linked until its version and vendor strategy are approved.
      // Never publish unsubstantiated Odometry while no estimator adapter is available.
      RCLCPP_DEBUG(get_logger(), "image accepted, but estimator adapter is not configured");
    }
    publish_status();
  }

  void publish_status()
  {
    if (!validator_ || !status_publisher_ || !status_publisher_->is_activated()) {return;}
    diagnostic_msgs::msg::DiagnosticStatus status;
    status.name = "mower_localization/vio";
    status.hardware_id = "non-safety-vio";
    status.message = validator_->reason();
    status.level = validator_->state() == VioState::kError ? diagnostic_msgs::msg::DiagnosticStatus::ERROR :
      (validator_->state() == VioState::kDegraded || validator_->state() == VioState::kLost ?
      diagnostic_msgs::msg::DiagnosticStatus::WARN : diagnostic_msgs::msg::DiagnosticStatus::OK);
    diagnostic_msgs::msg::KeyValue state;
    state.key = "state";
    state.value = to_string(validator_->state());
    diagnostic_msgs::msg::KeyValue rejected;
    rejected.key = "rejected_images";
    rejected.value = std::to_string(validator_->rejected_images());
    status.values.push_back(state);
    status.values.push_back(rejected);
    status_publisher_->publish(status);
    diagnostic_msgs::msg::DiagnosticArray diagnostics;
    diagnostics.header.stamp = now();
    diagnostics.status.push_back(status);
    diagnostics_publisher_->publish(diagnostics);
  }

  void reset_resources()
  {
    if (timeout_timer_) {timeout_timer_->cancel();}
    timeout_timer_.reset();
    image_subscription_.reset();
    camera_info_subscription_.reset();
    imu_subscription_.reset();
    odometry_publisher_.reset();
    status_publisher_.reset();
    diagnostics_publisher_.reset();
    validator_.reset();
    input_timed_out_ = false;
  }

  std::unique_ptr<VioInputValidator> validator_;
  rclcpp::TimerBase::SharedPtr timeout_timer_;
  std::chrono::steady_clock::time_point last_input_time_{};
  bool input_timed_out_{false};
  rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr image_subscription_;
  rclcpp::Subscription<sensor_msgs::msg::CameraInfo>::SharedPtr camera_info_subscription_;
  rclcpp::Subscription<sensor_msgs::msg::Imu>::SharedPtr imu_subscription_;
  rclcpp_lifecycle::LifecyclePublisher<nav_msgs::msg::Odometry>::SharedPtr odometry_publisher_;
  rclcpp_lifecycle::LifecyclePublisher<diagnostic_msgs::msg::DiagnosticStatus>::SharedPtr status_publisher_;
  rclcpp_lifecycle::LifecyclePublisher<diagnostic_msgs::msg::DiagnosticArray>::SharedPtr diagnostics_publisher_;
};
}  // namespace mower_localization

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  const auto node = std::make_shared<mower_localization::BasaltVioNode>();
  rclcpp::spin(node->get_node_base_interface());
  rclcpp::shutdown();
  return 0;
}
