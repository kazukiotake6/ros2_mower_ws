// Copyright 2026 Mower maintainers
// SPDX-License-Identifier: Apache-2.0

#include <fcntl.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cmath>
#include <cstring>
#include <string>

#include "diagnostic_msgs/msg/diagnostic_array.hpp"
#include "diagnostic_msgs/msg/diagnostic_status.hpp"
#include "diagnostic_msgs/msg/key_value.hpp"
#include "geometry_msgs/msg/twist_stamped.hpp"
#include "mower_can/can_protocol.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_lifecycle/lifecycle_node.hpp"
#include "std_msgs/msg/bool.hpp"

using CallbackReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;
using namespace std::chrono_literals;

class CanGatewayNode final : public rclcpp_lifecycle::LifecycleNode
{
public:
  CanGatewayNode() : LifecycleNode("can_gateway_node")
  {
    interface_ = declare_parameter<std::string>("can_interface", "can0");
    motion_id_ = declare_parameter<int>("motion_can_id", 0x200);
    heartbeat_id_ = declare_parameter<int>("heartbeat_can_id", 0x201);
    command_period_ms_ = declare_parameter<int>("command_period_ms", 20);
    command_timeout_ms_ = declare_parameter<int>("command_timeout_ms", 100);
    max_linear_mps_ = declare_parameter<double>("max_linear_mps", 0.3);
    max_angular_radps_ = declare_parameter<double>("max_angular_radps", 0.8);
  }

  ~CanGatewayNode() override { close_socket(); }

  CallbackReturn on_configure(const rclcpp_lifecycle::State &) override
  {
    if (motion_id_ < 0 || motion_id_ > CAN_SFF_MASK || heartbeat_id_ < 0 || heartbeat_id_ > CAN_SFF_MASK ||
      command_period_ms_ <= 0 || command_timeout_ms_ < command_period_ms_) {
      RCLCPP_ERROR(get_logger(), "Invalid CAN IDs or command timing parameters");
      return CallbackReturn::FAILURE;
    }
    if (!open_socket()) {
      return CallbackReturn::FAILURE;
    }
    diagnostics_pub_ = create_publisher<diagnostic_msgs::msg::DiagnosticArray>("/diagnostics", 10);
    command_sub_ = create_subscription<geometry_msgs::msg::TwistStamped>(
      "/cmd_vel", 10, [this](geometry_msgs::msg::TwistStamped::ConstSharedPtr message) {
        if (!std::isfinite(message->twist.linear.x) || !std::isfinite(message->twist.angular.z) ||
          std::abs(message->twist.linear.x) > max_linear_mps_ ||
          std::abs(message->twist.angular.z) > max_angular_radps_) {
          ++rejected_commands_;
          return;
        }
        linear_mps_ = message->twist.linear.x;
        angular_radps_ = message->twist.angular.z;
        last_command_ = std::chrono::steady_clock::now();
        have_command_ = true;
      });
    enable_sub_ = create_subscription<std_msgs::msg::Bool>(
      "/mower/enable", 10, [this](std_msgs::msg::Bool::ConstSharedPtr message) { enabled_ = message->data; });
    return CallbackReturn::SUCCESS;
  }

  CallbackReturn on_activate(const rclcpp_lifecycle::State &) override
  {
    command_timer_ = create_wall_timer(std::chrono::milliseconds(command_period_ms_), [this] { transmit(); });
    receive_timer_ = create_wall_timer(5ms, [this] { receive(); });
    diagnostics_timer_ = create_wall_timer(1s, [this] { publish_diagnostics(); });
    return CallbackReturn::SUCCESS;
  }

  CallbackReturn on_deactivate(const rclcpp_lifecycle::State &) override
  {
    enabled_ = false;
    transmit();  // Best-effort explicit stop; the MCU timeout remains authoritative.
    command_timer_.reset(); receive_timer_.reset(); diagnostics_timer_.reset();
    return CallbackReturn::SUCCESS;
  }

  CallbackReturn on_cleanup(const rclcpp_lifecycle::State &) override
  {
    command_sub_.reset(); enable_sub_.reset(); diagnostics_pub_.reset(); close_socket();
    return CallbackReturn::SUCCESS;
  }

private:
  bool open_socket()
  {
    socket_fd_ = socket(PF_CAN, SOCK_RAW | SOCK_NONBLOCK, CAN_RAW);
    if (socket_fd_ < 0) { RCLCPP_ERROR(get_logger(), "socket: %s", std::strerror(errno)); return false; }
    const can_err_mask_t error_mask = CAN_ERR_MASK;
    if (setsockopt(socket_fd_, SOL_CAN_RAW, CAN_RAW_ERR_FILTER, &error_mask, sizeof(error_mask)) < 0) {
      RCLCPP_ERROR(get_logger(), "CAN_RAW_ERR_FILTER: %s", std::strerror(errno));
      close_socket(); return false;
    }
    ifreq request{};
    std::strncpy(request.ifr_name, interface_.c_str(), IFNAMSIZ - 1);
    if (ioctl(socket_fd_, SIOCGIFINDEX, &request) < 0) {
      RCLCPP_ERROR(get_logger(), "CAN interface '%s' unavailable: %s", interface_.c_str(), std::strerror(errno));
      close_socket(); return false;
    }
    sockaddr_can address{}; address.can_family = AF_CAN; address.can_ifindex = request.ifr_ifindex;
    if (bind(socket_fd_, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0) {
      RCLCPP_ERROR(get_logger(), "bind: %s", std::strerror(errno)); close_socket(); return false;
    }
    return true;
  }
  void close_socket() { if (socket_fd_ >= 0) { close(socket_fd_); socket_fd_ = -1; } }
  void transmit()
  {
    if (socket_fd_ < 0) { return; }
    const auto now = std::chrono::steady_clock::now();
    const bool valid = have_command_ && now - last_command_ <= std::chrono::milliseconds(command_timeout_ms_);
    const auto motion = mower_can::make_motion_frame(motion_id_, motion_sequence_++, enabled_ && valid,
        valid ? linear_mps_ : 0.0, valid ? angular_radps_ : 0.0);
    const auto heartbeat = mower_can::make_heartbeat_frame(heartbeat_id_, heartbeat_sequence_++);
    for (const auto & frame : {motion, heartbeat}) {
      if (write(socket_fd_, &frame, sizeof(frame)) == static_cast<ssize_t>(sizeof(frame))) { ++tx_frames_; }
      else { ++tx_errors_; }
    }
  }
  void receive()
  {
    can_frame frame{};
    while (socket_fd_ >= 0 && read(socket_fd_, &frame, sizeof(frame)) == static_cast<ssize_t>(sizeof(frame))) {
      ++rx_frames_;
      if ((frame.can_id & CAN_ERR_FLAG) != 0U) { ++error_frames_; }
    }
  }
  void publish_diagnostics()
  {
    diagnostic_msgs::msg::DiagnosticStatus status;
    status.name = get_fully_qualified_name() + std::string(": SocketCAN");
    status.hardware_id = interface_;
    status.level = tx_errors_ == 0 ? diagnostic_msgs::msg::DiagnosticStatus::OK : diagnostic_msgs::msg::DiagnosticStatus::ERROR;
    status.message = tx_errors_ == 0 ? "CAN link active" : "CAN transmit errors observed";
    for (const auto & item : {std::pair{"tx_frames", tx_frames_}, {"rx_frames", rx_frames_},
        {"tx_errors", tx_errors_}, {"error_frames", error_frames_}, {"rejected_commands", rejected_commands_}}) {
      diagnostic_msgs::msg::KeyValue value; value.key = item.first; value.value = std::to_string(item.second); status.values.push_back(value);
    }
    diagnostic_msgs::msg::DiagnosticArray array; array.header.stamp = now(); array.status.push_back(status); diagnostics_pub_->publish(array);
  }
  std::string interface_; int motion_id_{}; int heartbeat_id_{}; int command_period_ms_{}; int command_timeout_ms_{};
  double max_linear_mps_{}; double max_angular_radps_{}; int socket_fd_{-1}; bool enabled_{false}; bool have_command_{false};
  double linear_mps_{0.0}; double angular_radps_{0.0}; std::chrono::steady_clock::time_point last_command_{};
  uint8_t motion_sequence_{0}; uint8_t heartbeat_sequence_{0}; uint64_t tx_frames_{0}, rx_frames_{0}, tx_errors_{0}, error_frames_{0}, rejected_commands_{0};
  rclcpp::Subscription<geometry_msgs::msg::TwistStamped>::SharedPtr command_sub_;
  rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr enable_sub_;
  rclcpp::Publisher<diagnostic_msgs::msg::DiagnosticArray>::SharedPtr diagnostics_pub_;
  rclcpp::TimerBase::SharedPtr command_timer_, receive_timer_, diagnostics_timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<CanGatewayNode>();
  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(node->get_node_base_interface());
  executor.spin();
  rclcpp::shutdown();
  return 0;
}
