// Copyright 2026 Mower maintainers
// SPDX-License-Identifier: Apache-2.0
#include "mower_imu/spi_transport.hpp"

#include <chrono>
#include <cstring>
#include <thread>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace mower_imu
{
LinuxSpiTransport::LinuxSpiTransport(std::string device, uint32_t speed_hz)
: device_(std::move(device)), speed_hz_(speed_hz) {}

LinuxSpiTransport::~LinuxSpiTransport()
{
  if (fd_ >= 0) {
    ::close(fd_);
  }
}

bool LinuxSpiTransport::open()
{
  if (fd_ >= 0) {
    return true;
  }
  fd_ = ::open(device_.c_str(), O_RDWR | O_CLOEXEC);
  uint8_t mode = SPI_MODE_0;
  uint8_t bits = 8;
  if (fd_ < 0 || ioctl(fd_, SPI_IOC_WR_MODE, &mode) < 0 ||
    ioctl(fd_, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
    ioctl(fd_, SPI_IOC_WR_MAX_SPEED_HZ, &speed_hz_) < 0)
  {
    if (fd_ >= 0) {
      ::close(fd_);
      fd_ = -1;
    }
    return false;
  }
  return true;
}

bool LinuxSpiTransport::read(uint8_t reg, uint8_t * data, size_t length)
{
  // The Bosch callback contract includes the BMI270 SPI dummy byte in length.
  // Add only the command byte here and return the dummy byte to the Sensor API,
  // which removes it according to bmi2_dev::dummy_byte.
  std::vector<uint8_t> tx(length + 1U, 0U), rx(tx.size(), 0U);
  tx[0] = static_cast<uint8_t>(reg | 0x80U);
  spi_ioc_transfer transfer{};
  transfer.tx_buf = reinterpret_cast<unsigned long>(tx.data());
  transfer.rx_buf = reinterpret_cast<unsigned long>(rx.data());
  transfer.len = static_cast<uint32_t>(tx.size());
  transfer.speed_hz = speed_hz_;
  transfer.bits_per_word = 8;
  if (fd_ < 0 || ioctl(fd_, SPI_IOC_MESSAGE(1), &transfer) < 1) {
    return false;
  }
  std::memcpy(data, rx.data() + 1U, length);
  return true;
}

bool LinuxSpiTransport::write(uint8_t reg, const uint8_t * data, size_t length)
{
  std::vector<uint8_t> tx(length + 1U, 0U);
  tx[0] = static_cast<uint8_t>(reg & 0x7fU);
  std::memcpy(tx.data() + 1U, data, length);
  spi_ioc_transfer transfer{};
  transfer.tx_buf = reinterpret_cast<unsigned long>(tx.data());
  transfer.len = static_cast<uint32_t>(tx.size());
  transfer.speed_hz = speed_hz_;
  transfer.bits_per_word = 8;
  return fd_ >= 0 && ioctl(fd_, SPI_IOC_MESSAGE(1), &transfer) >= 1;
}

void LinuxSpiTransport::delay_us(uint32_t period_us)
{
  std::this_thread::sleep_for(std::chrono::microseconds(period_us));
}
}  // namespace mower_imu
