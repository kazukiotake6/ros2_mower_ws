// Copyright 2026 Mower maintainers
// SPDX-License-Identifier: Apache-2.0
#include <memory>
#include <string>

#include "diagnostic_msgs/msg/diagnostic_array.hpp"
#include "diagnostic_msgs/msg/diagnostic_status.hpp"
#include "diagnostic_msgs/msg/key_value.hpp"
#include "mower_imu/bmi270_driver.hpp"
#include "rclcpp/rclcpp.hpp"

namespace mower_imu
{
class Bmi270Node final : public rclcpp::Node
{
public:
  Bmi270Node() : Node("bmi270_node")
  {
    const auto device = declare_parameter<std::string>("spi_device", "/dev/spidev0.0");
    const auto speed = declare_parameter<int>("spi_speed_hz", 1000000);
    diagnostics_ = create_publisher<diagnostic_msgs::msg::DiagnosticArray>(
      "/diagnostics", rclcpp::QoS(1).reliable().transient_local());
    transport_ = std::make_unique<LinuxSpiTransport>(device, static_cast<uint32_t>(speed));
    api_ = std::make_unique<BoschBmi270Api>();
    driver_ = std::make_unique<Bmi270Driver>(*transport_, *api_);
    const auto result = speed > 0 ? driver_->initialize() :
      InitializationResult{InitializationStage::kTransport, 0, "spi_speed_hz must be positive"};
    publish_diagnostic(result);
    if (!result.ok()) {
      RCLCPP_ERROR(get_logger(), "BMI270 initialization stopped: %s (API status %d)",
        result.message.c_str(), result.api_status);
    }
  }

private:
  void publish_diagnostic(const InitializationResult & result)
  {
    diagnostic_msgs::msg::DiagnosticArray message;
    message.header.stamp = now();
    diagnostic_msgs::msg::DiagnosticStatus status;
    status.name = "mower_imu/BMI270 initialization";
    status.hardware_id = "bmi270";
    status.level = result.ok() ? status.OK : status.ERROR;
    status.message = result.message;
    diagnostic_msgs::msg::KeyValue stage;
    stage.key = "initialization_stage";
    stage.value = std::to_string(static_cast<int>(result.stage));
    diagnostic_msgs::msg::KeyValue api_status;
    api_status.key = "api_status";
    api_status.value = std::to_string(result.api_status);
    status.values = {stage, api_status};
    message.status.push_back(status);
    diagnostics_->publish(message);
  }

  rclcpp::Publisher<diagnostic_msgs::msg::DiagnosticArray>::SharedPtr diagnostics_;
  std::unique_ptr<LinuxSpiTransport> transport_;
  std::unique_ptr<BoschBmi270Api> api_;
  std::unique_ptr<Bmi270Driver> driver_;
};
}  // namespace mower_imu

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<mower_imu::Bmi270Node>());
  rclcpp::shutdown();
  return 0;
}
