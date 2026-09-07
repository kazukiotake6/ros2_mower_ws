// Copyright 2026 Mower maintainers
// SPDX-License-Identifier: Apache-2.0
#include "mower_imu/bmi270_driver.hpp"

#include <array>

extern "C" {
#include "bmi270.h"
}

namespace
{
BMI2_INTF_RETURN_TYPE read_callback(uint8_t reg, uint8_t * data, uint32_t length, void * context)
{
  auto * transport = static_cast<mower_imu::SpiTransport *>(context);
  return transport->read(reg, data, length) ? BMI2_INTF_RET_SUCCESS : -1;
}

BMI2_INTF_RETURN_TYPE write_callback(uint8_t reg, const uint8_t * data, uint32_t length, void * context)
{
  auto * transport = static_cast<mower_imu::SpiTransport *>(context);
  return transport->write(reg, data, length) ? BMI2_INTF_RET_SUCCESS : -1;
}

void delay_callback(uint32_t period_us, void * context)
{
  static_cast<mower_imu::SpiTransport *>(context)->delay_us(period_us);
}
}  // namespace

namespace mower_imu
{
int8_t BoschBmi270Api::initialize(bmi2_dev * device) {return bmi270_init(device);}
int8_t BoschBmi270Api::configure(bmi2_sens_config * config, uint8_t count, bmi2_dev * device)
{return bmi270_set_sensor_config(config, count, device);}
int8_t BoschBmi270Api::enable(const uint8_t * sensors, uint8_t count, bmi2_dev * device)
{return bmi270_sensor_enable(sensors, count, device);}

Bmi270Driver::Bmi270Driver(SpiTransport & transport, SensorApi & api)
: transport_(transport), api_(api) {}

InitializationResult Bmi270Driver::initialize()
{
  if (!transport_.open()) {
    return {InitializationStage::kTransport, 0, "SPI device open or configuration failed"};
  }
  bmi2_dev device{};
  device.intf = BMI2_SPI_INTF;
  device.read = read_callback;
  device.write = write_callback;
  device.delay_us = delay_callback;
  device.intf_ptr = &transport_;
  device.read_write_len = 32;

  int8_t status = api_.initialize(&device);
  if (status != BMI2_OK) {
    return {InitializationStage::kApiInitialization, status,
      "bmi270_init failed; chip ID, SPI communication, or configuration load was rejected"};
  }

  std::array<bmi2_sens_config, 2> config{};
  config[0].type = BMI2_ACCEL;
  config[0].cfg.acc.odr = BMI2_ACC_ODR_200HZ;
  config[0].cfg.acc.range = BMI2_ACC_RANGE_4G;
  config[0].cfg.acc.bwp = BMI2_ACC_NORMAL_AVG4;
  config[0].cfg.acc.filter_perf = BMI2_PERF_OPT_MODE;
  config[1].type = BMI2_GYRO;
  config[1].cfg.gyr.odr = BMI2_GYR_ODR_200HZ;
  config[1].cfg.gyr.range = BMI2_GYR_RANGE_2000;
  config[1].cfg.gyr.bwp = BMI2_GYR_NORMAL_MODE;
  config[1].cfg.gyr.noise_perf = BMI2_POWER_OPT_MODE;
  config[1].cfg.gyr.filter_perf = BMI2_PERF_OPT_MODE;
  status = api_.configure(config.data(), static_cast<uint8_t>(config.size()), &device);
  if (status != BMI2_OK) {
    return {InitializationStage::kSensorConfiguration, status, "accelerometer/gyroscope configuration failed"};
  }

  const std::array<uint8_t, 2> sensors{BMI2_ACCEL, BMI2_GYRO};
  status = api_.enable(sensors.data(), static_cast<uint8_t>(sensors.size()), &device);
  if (status != BMI2_OK) {
    return {InitializationStage::kSensorEnable, status, "accelerometer/gyroscope enable failed"};
  }
  return {InitializationStage::kReady, BMI2_OK, "BMI270 initialized with the official Sensor API"};
}
}  // namespace mower_imu
