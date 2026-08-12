// Copyright 2026 Mower maintainers
// SPDX-License-Identifier: Apache-2.0

#include <chrono>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/imu.hpp"

namespace
{
constexpr uint8_t kChipIdReg = 0x00;
constexpr uint8_t kExpectedChipId = 0x24;
constexpr uint8_t kDataReg = 0x0c;
constexpr uint8_t kAccConfReg = 0x40;
constexpr uint8_t kAccRangeReg = 0x41;
constexpr uint8_t kGyrConfReg = 0x42;
constexpr uint8_t kGyrRangeReg = 0x43;
constexpr uint8_t kPwrConfReg = 0x7c;
constexpr uint8_t kPwrCtrlReg = 0x7d;
constexpr uint8_t kCmdReg = 0x7e;
constexpr uint8_t kSoftReset = 0xb6;
constexpr uint8_t kOdr200HzPerformance = 0xa8;
constexpr uint8_t kEnableAccelAndGyro = 0x06;
constexpr double kGravity = 9.80665;
constexpr double kDegToRad = 0.017453292519943295;

int16_t decode_int16(uint8_t low, uint8_t high)
{
  return static_cast<int16_t>(static_cast<uint16_t>(low) | (static_cast<uint16_t>(high) << 8));
}

class Bmi270RawNode : public rclcpp::Node
{
public:
  Bmi270RawNode()
  : Node("bmi270_raw_node")
  {
    device_ = declare_parameter<std::string>("spi_device", "/dev/spidev0.0");
    speed_hz_ = declare_parameter<int>("spi_speed_hz", 1000000);
    accel_range_g_ = declare_parameter<int>("accel_range_g", 4);
    gyro_range_dps_ = declare_parameter<int>("gyro_range_dps", 2000);
    rate_hz_ = declare_parameter<double>("poll_rate_hz", 200.0);
    frame_id_ = declare_parameter<std::string>("frame_id", "imu_link");
    if (speed_hz_ <= 0 || rate_hz_ <= 0.0) throw std::invalid_argument("SPI speed and poll rate must be positive");
    accel_scale_ = accel_scale(accel_range_g_);
    gyro_scale_ = gyro_scale(gyro_range_dps_);
    publisher_ = create_publisher<sensor_msgs::msg::Imu>("/imu/data_raw", rclcpp::SensorDataQoS());
    timer_ = create_wall_timer(std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::duration<double>(1.0 / rate_hz_)), std::bind(&Bmi270RawNode::poll, this));
  }

  ~Bmi270RawNode() override { if (fd_ >= 0) close(fd_); }

private:
  static uint8_t accel_range_value(int g)
  {
    if (g == 2) return 0;
    if (g == 4) return 1;
    if (g == 8) return 2;
    if (g == 16) return 3;
    throw std::invalid_argument("accel_range_g must be 2, 4, 8, or 16");
  }

  static uint8_t gyro_range_value(int dps)
  {
    if (dps == 2000) return 0;
    if (dps == 1000) return 1;
    if (dps == 500) return 2;
    if (dps == 250) return 3;
    if (dps == 125) return 4;
    throw std::invalid_argument("gyro_range_dps must be 125, 250, 500, 1000, or 2000");
  }

  static double accel_scale(int g) { return static_cast<double>(g) * kGravity / 32768.0; }
  static double gyro_scale(int dps) { return static_cast<double>(dps) * kDegToRad / 32768.0; }

  void initialize()
  {
    fd_ = open(device_.c_str(), O_RDWR | O_CLOEXEC);
    if (fd_ < 0) throw std::runtime_error("open " + device_ + ": " + std::strerror(errno));
    uint8_t mode = SPI_MODE_0, bits = 8;
    if (ioctl(fd_, SPI_IOC_WR_MODE, &mode) < 0 || ioctl(fd_, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
      ioctl(fd_, SPI_IOC_WR_MAX_SPEED_HZ, &speed_hz_) < 0) throw std::runtime_error("SPI configuration failed");
    write_reg(kCmdReg, kSoftReset);
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
    if (read_reg(kChipIdReg) != kExpectedChipId) throw std::runtime_error("BMI270 chip ID is not 0x24");
    write_reg(kPwrConfReg, 0x00);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    write_reg(kAccConfReg, kOdr200HzPerformance);
    write_reg(kAccRangeReg, accel_range_value(accel_range_g_));
    write_reg(kGyrConfReg, kOdr200HzPerformance);
    write_reg(kGyrRangeReg, gyro_range_value(gyro_range_dps_));
    write_reg(kPwrCtrlReg, kEnableAccelAndGyro);
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
  }

  uint8_t read_reg(uint8_t reg) { uint8_t value{}; read_regs(reg, &value, 1); return value; }

  void read_regs(uint8_t reg, uint8_t * output, uint16_t length)
  {
    std::vector<uint8_t> tx(static_cast<size_t>(length) + 2U, 0), rx(tx.size(), 0);
    tx[0] = static_cast<uint8_t>(reg | 0x80U);
    spi_ioc_transfer transfer{};
    transfer.tx_buf = reinterpret_cast<unsigned long>(tx.data());
    transfer.rx_buf = reinterpret_cast<unsigned long>(rx.data());
    transfer.len = static_cast<uint32_t>(tx.size());
    transfer.speed_hz = static_cast<uint32_t>(speed_hz_);
    transfer.bits_per_word = 8;
    if (ioctl(fd_, SPI_IOC_MESSAGE(1), &transfer) < 1) throw std::runtime_error("BMI270 SPI read failed");
    std::memcpy(output, rx.data() + 2, length);
  }

  void write_reg(uint8_t reg, uint8_t value)
  {
    uint8_t tx[2]{static_cast<uint8_t>(reg & 0x7fU), value};
    spi_ioc_transfer transfer{};
    transfer.tx_buf = reinterpret_cast<unsigned long>(tx);
    transfer.len = sizeof(tx);
    transfer.speed_hz = static_cast<uint32_t>(speed_hz_);
    transfer.bits_per_word = 8;
    if (ioctl(fd_, SPI_IOC_MESSAGE(1), &transfer) < 1) throw std::runtime_error("BMI270 SPI write failed");
  }

  void poll()
  {
    try {
      if (fd_ < 0) { initialize(); RCLCPP_INFO(get_logger(), "BMI270 initialized on %s", device_.c_str()); }
      uint8_t data[12]{};
      read_regs(kDataReg, data, sizeof(data));
      sensor_msgs::msg::Imu msg;
      msg.header.stamp = now();
      msg.header.frame_id = frame_id_;
      msg.orientation_covariance[0] = -1.0;
      msg.angular_velocity.x = decode_int16(data[0], data[1]) * gyro_scale_;
      msg.angular_velocity.y = decode_int16(data[2], data[3]) * gyro_scale_;
      msg.angular_velocity.z = decode_int16(data[4], data[5]) * gyro_scale_;
      msg.linear_acceleration.x = decode_int16(data[6], data[7]) * accel_scale_;
      msg.linear_acceleration.y = decode_int16(data[8], data[9]) * accel_scale_;
      msg.linear_acceleration.z = decode_int16(data[10], data[11]) * accel_scale_;
      publisher_->publish(msg);
    } catch (const std::exception & error) {
      RCLCPP_ERROR_THROTTLE(get_logger(), *get_clock(), 5000, "BMI270 unavailable: %s", error.what());
      if (fd_ >= 0) { close(fd_); fd_ = -1; }
    }
  }

  std::string device_, frame_id_;
  int speed_hz_{}, accel_range_g_{}, gyro_range_dps_{}, fd_{-1};
  double rate_hz_{}, accel_scale_{}, gyro_scale_{};
  rclcpp::Publisher<sensor_msgs::msg::Imu>::SharedPtr publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
};
}  // namespace

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<Bmi270RawNode>());
  rclcpp::shutdown();
  return 0;
}
