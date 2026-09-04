// Copyright 2026 Mower maintainers
// SPDX-License-Identifier: Apache-2.0

#include "mower_localization/basalt_vio_node.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "diagnostic_msgs/msg/key_value.hpp"

namespace mower_localization
{
namespace
{
std::int64_t to_nanoseconds(const builtin_interfaces::msg::Time & stamp)
{
  return static_cast<std::int64_t>(stamp.sec) * 1000000000LL + stamp.nanosec;
}

bool has_valid_calibration(const sensor_msgs::msg::CameraInfo & message)
{
  return message.k[0] > 0.0 && message.k[4] > 0.0 && !message.d.empty();
}

bool finite_odometry(const nav_msgs::msg::Odometry & odometry)
{
  const auto & p = odometry.pose.pose.position;
  const auto & q = odometry.pose.pose.orientation;
  const auto & linear = odometry.twist.twist.linear;
  const auto & angular = odometry.twist.twist.angular;
  return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z) &&
         std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.z) && std::isfinite(q.w) &&
         std::isfinite(linear.x) && std::isfinite(linear.y) && std::isfinite(linear.z) &&
         std::isfinite(angular.x) && std::isfinite(angular.y) && std::isfinite(angular.z);
}

class UnavailableEstimatorAdapter final : public VioEstimatorAdapter
{
public:
  void reset() override {}
  bool add_imu(const sensor_msgs::msg::Imu &) override {return true;}
  EstimatorResult process_image(
    const sensor_msgs::msg::Image &, const sensor_msgs::msg::CameraInfo &) override
  {
    return {EstimatorState::kInitializing, std::nullopt, "estimator adapter is not configured"};
  }
};
}  // namespace

BasaltVioNode::BasaltVioNode(
  std::shared_ptr<VioEstimatorAdapter> estimator,
  const rclcpp::NodeOptions & options)
: LifecycleNode("basalt_vio_node", options),
  estimator_(estimator ? std::move(estimator) : std::make_shared<UnavailableEstimatorAdapter>())
{
  declare_parameter<bool>("calibration_approved", false);
  declare_parameter<int>("max_imu_gap_ms", 0);
  declare_parameter<int>("input_timeout_ms", 0);
  declare_parameter<int>("imu_queue_capacity", 0);
}

BasaltVioNode::CallbackReturn BasaltVioNode::on_configure(const rclcpp_lifecycle::State &)
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
  estimator_->reset();
  camera_info_.reset();
  const auto sensor_qos = rclcpp::SensorDataQoS();
  image_subscription_ = create_subscription<sensor_msgs::msg::Image>(
    "/image_raw", sensor_qos,
    [this](sensor_msgs::msg::Image::ConstSharedPtr message) {on_image(*message);});
  camera_info_subscription_ = create_subscription<sensor_msgs::msg::CameraInfo>(
    "/camera_info", sensor_qos,
    [this](sensor_msgs::msg::CameraInfo::ConstSharedPtr message) {
      camera_info_ = *message;
      validator_->update_camera_info(CameraInfoMetadata{
        to_nanoseconds(message->header.stamp), message->width, message->height,
        message->header.frame_id, has_valid_calibration(*message)});
    });
  imu_subscription_ = create_subscription<sensor_msgs::msg::Imu>(
    "/imu/data_raw", sensor_qos,
    [this](sensor_msgs::msg::Imu::ConstSharedPtr message) {on_imu(*message);});
  odometry_publisher_ = create_publisher<nav_msgs::msg::Odometry>("/vio/odometry", 10);
  status_publisher_ = create_publisher<diagnostic_msgs::msg::DiagnosticStatus>("/vio/status", 10);
  diagnostics_publisher_ =
    create_publisher<diagnostic_msgs::msg::DiagnosticArray>("/diagnostics", 10);
  return CallbackReturn::SUCCESS;
}

BasaltVioNode::CallbackReturn BasaltVioNode::on_activate(const rclcpp_lifecycle::State &)
{
  odometry_publisher_->on_activate();
  status_publisher_->on_activate();
  diagnostics_publisher_->on_activate();
  last_image_time_ = std::chrono::steady_clock::now();
  last_imu_time_ = last_image_time_;
  input_timed_out_ = false;
  const auto timeout_ms = get_parameter("input_timeout_ms").as_int();
  const auto check_period_ms =
    std::max<std::int64_t>(1, std::min<std::int64_t>(timeout_ms / 2, 100));
  timeout_timer_ = create_wall_timer(
    std::chrono::milliseconds(check_period_ms), [this]() {check_input_timeout();});
  publish_status();
  return CallbackReturn::SUCCESS;
}

BasaltVioNode::CallbackReturn BasaltVioNode::on_deactivate(const rclcpp_lifecycle::State &)
{
  if (timeout_timer_) {timeout_timer_->cancel();}
  odometry_publisher_->on_deactivate();
  status_publisher_->on_deactivate();
  diagnostics_publisher_->on_deactivate();
  return CallbackReturn::SUCCESS;
}

BasaltVioNode::CallbackReturn BasaltVioNode::on_cleanup(const rclcpp_lifecycle::State &)
{
  reset_resources();
  return CallbackReturn::SUCCESS;
}

BasaltVioNode::CallbackReturn BasaltVioNode::on_shutdown(const rclcpp_lifecycle::State &)
{
  reset_resources();
  return CallbackReturn::SUCCESS;
}

BasaltVioNode::CallbackReturn BasaltVioNode::on_error(const rclcpp_lifecycle::State &)
{
  reset_resources();
  return CallbackReturn::SUCCESS;
}

void BasaltVioNode::record_image_input()
{
  last_image_time_ = std::chrono::steady_clock::now();
  input_timed_out_ = false;
}

void BasaltVioNode::record_imu_input()
{
  last_imu_time_ = std::chrono::steady_clock::now();
  input_timed_out_ = false;
}

void BasaltVioNode::check_input_timeout()
{
  if (!validator_ || input_timed_out_) {return;}
  const auto timeout = std::chrono::milliseconds(get_parameter("input_timeout_ms").as_int());
  const auto now = std::chrono::steady_clock::now();
  if (now - last_image_time_ >= timeout || now - last_imu_time_ >= timeout) {
    validator_->report_input_timeout();
    input_timed_out_ = true;
    publish_status();
  }
}

void BasaltVioNode::on_imu(const sensor_msgs::msg::Imu & message)
{
  record_imu_input();
  const auto result = validator_->push_imu(ImuSample{to_nanoseconds(message.header.stamp),
        {message.angular_velocity.x, message.angular_velocity.y, message.angular_velocity.z},
        {message.linear_acceleration.x, message.linear_acceleration.y,
          message.linear_acceleration.z}});
  if (result == ImuInsertResult::kAccepted && !estimator_->add_imu(message)) {
    validator_->report_estimator_error("estimator rejected accepted IMU input");
  }
  publish_status();
}

void BasaltVioNode::on_image(const sensor_msgs::msg::Image & message)
{
  record_image_input();
  const bool accepted = validator_->accept_image(ImageMetadata{
      to_nanoseconds(message.header.stamp), message.width, message.height,
      message.header.frame_id});
  if (accepted && camera_info_) {
    try {
      handle_estimator_result(estimator_->process_image(message, *camera_info_), message);
    } catch (const std::exception & exception) {
      validator_->report_estimator_error(
        std::string("estimator exception: ") + exception.what());
    }
  }
  publish_status();
}

void BasaltVioNode::handle_estimator_result(
  const EstimatorResult & result, const sensor_msgs::msg::Image & image)
{
  if (result.state == EstimatorState::kInitializing) {return;}
  if (result.state == EstimatorState::kLost) {
    validator_->report_estimator_tracking(false);
    return;
  }
  if (result.state == EstimatorState::kError) {
    validator_->report_estimator_error(
      result.reason.empty() ? "estimator internal error" : result.reason);
    return;
  }
  if (!result.odometry) {
    validator_->report_estimator_error("tracking result has no odometry");
    return;
  }
  const auto & odometry = *result.odometry;
  if (odometry.header.stamp != image.header.stamp ||
    odometry.header.frame_id != "vio_odom" ||
    odometry.child_frame_id != "base_link" || !finite_odometry(odometry))
  {
    validator_->report_estimator_error("estimator returned invalid odometry");
    return;
  }
  validator_->report_estimator_tracking(true);
  if (odometry_publisher_->is_activated()) {
    odometry_publisher_->publish(odometry);
  }
}

void BasaltVioNode::publish_status()
{
  if (!validator_ || !status_publisher_ || !status_publisher_->is_activated()) {return;}
  diagnostic_msgs::msg::DiagnosticStatus status;
  status.name = "mower_localization/vio";
  status.hardware_id = "non-safety-vio";
  status.message = validator_->reason();
  status.level = validator_->state() == VioState::kError ?
    diagnostic_msgs::msg::DiagnosticStatus::ERROR :
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

void BasaltVioNode::reset_resources()
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
  camera_info_.reset();
  estimator_->reset();
  input_timed_out_ = false;
}

}  // namespace mower_localization
