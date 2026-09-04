// Copyright 2026 Mower maintainers
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>
#include <string>

#include "mower_imu/spi_transport.hpp"

struct bmi2_dev;
struct bmi2_sens_config;

namespace mower_imu
{
enum class InitializationStage
{
  kNotStarted,
  kTransport,
  kApiInitialization,
  kSensorConfiguration,
  kSensorEnable,
  kReady
};

struct InitializationResult
{
  InitializationStage stage{InitializationStage::kNotStarted};
  int8_t api_status{0};
  std::string message;
  bool ok() const { return stage == InitializationStage::kReady; }
};

class SensorApi
{
public:
  virtual ~SensorApi() = default;
  virtual int8_t initialize(bmi2_dev * device) = 0;
  virtual int8_t configure(bmi2_sens_config * config, uint8_t count, bmi2_dev * device) = 0;
  virtual int8_t enable(const uint8_t * sensors, uint8_t count, bmi2_dev * device) = 0;
};

class BoschBmi270Api final : public SensorApi
{
public:
  int8_t initialize(bmi2_dev * device) override;
  int8_t configure(bmi2_sens_config * config, uint8_t count, bmi2_dev * device) override;
  int8_t enable(const uint8_t * sensors, uint8_t count, bmi2_dev * device) override;
};

class Bmi270Driver
{
public:
  Bmi270Driver(SpiTransport & transport, SensorApi & api);
  InitializationResult initialize();

private:
  SpiTransport & transport_;
  SensorApi & api_;
};
}  // namespace mower_imu
