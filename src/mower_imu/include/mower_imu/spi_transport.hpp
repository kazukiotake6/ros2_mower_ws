// Copyright 2026 Mower maintainers
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace mower_imu
{
class SpiTransport
{
public:
  virtual ~SpiTransport() = default;
  virtual bool open() = 0;
  virtual bool read(uint8_t reg, uint8_t * data, size_t length) = 0;
  virtual bool write(uint8_t reg, const uint8_t * data, size_t length) = 0;
  virtual void delay_us(uint32_t period_us) = 0;
};

class LinuxSpiTransport final : public SpiTransport
{
public:
  LinuxSpiTransport(std::string device, uint32_t speed_hz);
  ~LinuxSpiTransport() override;
  bool open() override;
  bool read(uint8_t reg, uint8_t * data, size_t length) override;
  bool write(uint8_t reg, const uint8_t * data, size_t length) override;
  void delay_us(uint32_t period_us) override;

private:
  std::string device_;
  uint32_t speed_hz_;
  int fd_{-1};
};
}  // namespace mower_imu
