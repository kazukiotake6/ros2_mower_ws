// Copyright 2026 Mower maintainers
// SPDX-License-Identifier: Apache-2.0
#include <cstdint>
#include <vector>

#include "gtest/gtest.h"
#include "mower_imu/bmi270_driver.hpp"

extern "C" {
#include "bmi270.h"
}

namespace
{
class FakeTransport final : public mower_imu::SpiTransport
{
public:
  bool open() override {++open_calls; return open_result;}
  bool read(uint8_t reg, uint8_t * data, size_t length) override
  {
    ++read_calls;
    last_register = reg;
    if (!read_result) {return false;}
    for (size_t index = 0; index < length; ++index) {data[index] = static_cast<uint8_t>(index + 1U);}
    return true;
  }
  bool write(uint8_t reg, const uint8_t * data, size_t length) override
  {
    ++write_calls;
    last_register = reg;
    last_write.assign(data, data + length);
    return write_result;
  }
  void delay_us(uint32_t period_us) override {last_delay_us = period_us;}

  bool open_result{true};
  bool read_result{true};
  bool write_result{true};
  int open_calls{0};
  int read_calls{0};
  int write_calls{0};
  uint8_t last_register{0};
  uint32_t last_delay_us{0};
  std::vector<uint8_t> last_write;
};

class FakeApi final : public mower_imu::SensorApi
{
public:
  int8_t initialize(bmi2_dev * device) override
  {
    ++initialize_calls;
    captured_interface = device->intf;
    if (exercise_callbacks) {
      uint8_t data[3]{};
      read_callback_status = device->read(0x00, data, sizeof(data), device->intf_ptr);
      const uint8_t burst[4]{1, 2, 3, 4};
      write_callback_status = device->write(0x5e, burst, sizeof(burst), device->intf_ptr);
      device->delay_us(450, device->intf_ptr);
    }
    return initialize_status;
  }
  int8_t configure(bmi2_sens_config * config, uint8_t count, bmi2_dev *) override
  {
    ++configure_calls;
    if (count == 2) {
      accel = config[0];
      gyro = config[1];
    }
    return configure_status;
  }
  int8_t enable(const uint8_t * sensors, uint8_t count, bmi2_dev *) override
  {
    ++enable_calls;
    enabled.assign(sensors, sensors + count);
    return enable_status;
  }

  int8_t initialize_status{BMI2_OK};
  int8_t configure_status{BMI2_OK};
  int8_t enable_status{BMI2_OK};
  bool exercise_callbacks{false};
  int initialize_calls{0};
  int configure_calls{0};
  int enable_calls{0};
  int8_t read_callback_status{0};
  int8_t write_callback_status{0};
  bmi2_intf captured_interface{BMI2_I2C_INTF};
  bmi2_sens_config accel{};
  bmi2_sens_config gyro{};
  std::vector<uint8_t> enabled;
};

TEST(Bmi270Driver, InitializesAndConfiguresBothSensorsAt200Hz)
{
  FakeTransport transport;
  FakeApi api;
  mower_imu::Bmi270Driver driver(transport, api);
  const auto result = driver.initialize();
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(api.initialize_calls, 1);
  EXPECT_EQ(api.configure_calls, 1);
  EXPECT_EQ(api.enable_calls, 1);
  EXPECT_EQ(api.accel.type, BMI2_ACCEL);
  EXPECT_EQ(api.accel.cfg.acc.odr, BMI2_ACC_ODR_200HZ);
  EXPECT_EQ(api.accel.cfg.acc.range, BMI2_ACC_RANGE_4G);
  EXPECT_EQ(api.gyro.type, BMI2_GYRO);
  EXPECT_EQ(api.gyro.cfg.gyr.odr, BMI2_GYR_ODR_200HZ);
  EXPECT_EQ(api.gyro.cfg.gyr.range, BMI2_GYR_RANGE_2000);
  EXPECT_EQ(api.enabled, (std::vector<uint8_t>{BMI2_ACCEL, BMI2_GYRO}));
}

TEST(Bmi270Driver, StopsWhenSpiCannotOpen)
{
  FakeTransport transport;
  transport.open_result = false;
  FakeApi api;
  mower_imu::Bmi270Driver driver(transport, api);
  const auto result = driver.initialize();
  EXPECT_EQ(result.stage, mower_imu::InitializationStage::kTransport);
  EXPECT_EQ(api.initialize_calls, 0);
}

TEST(Bmi270Driver, ReportsChipIdOrConfigurationLoadFailureFromOfficialInitialization)
{
  FakeTransport transport;
  FakeApi api;
  api.initialize_status = BMI2_E_DEV_NOT_FOUND;
  mower_imu::Bmi270Driver driver(transport, api);
  const auto result = driver.initialize();
  EXPECT_EQ(result.stage, mower_imu::InitializationStage::kApiInitialization);
  EXPECT_EQ(result.api_status, BMI2_E_DEV_NOT_FOUND);
  EXPECT_EQ(api.configure_calls, 0);
  EXPECT_EQ(api.enable_calls, 0);
}

TEST(Bmi270Driver, StopsWhenSensorConfigurationFails)
{
  FakeTransport transport;
  FakeApi api;
  api.configure_status = BMI2_E_INVALID_SENSOR;
  mower_imu::Bmi270Driver driver(transport, api);
  const auto result = driver.initialize();
  EXPECT_EQ(result.stage, mower_imu::InitializationStage::kSensorConfiguration);
  EXPECT_EQ(api.enable_calls, 0);
}

TEST(Bmi270Driver, ConnectsBurstSpiAndDelayCallbacks)
{
  FakeTransport transport;
  FakeApi api;
  api.exercise_callbacks = true;
  mower_imu::Bmi270Driver driver(transport, api);
  ASSERT_TRUE(driver.initialize().ok());
  EXPECT_EQ(api.captured_interface, BMI2_SPI_INTF);
  EXPECT_EQ(api.read_callback_status, BMI2_INTF_RET_SUCCESS);
  EXPECT_EQ(api.write_callback_status, BMI2_INTF_RET_SUCCESS);
  EXPECT_EQ(transport.read_calls, 1);
  EXPECT_EQ(transport.write_calls, 1);
  EXPECT_EQ(transport.last_register, 0x5e);
  EXPECT_EQ(transport.last_write, (std::vector<uint8_t>{1, 2, 3, 4}));
  EXPECT_EQ(transport.last_delay_us, 450U);
}

TEST(Bmi270Driver, PropagatesSpiCallbackFailure)
{
  FakeTransport transport;
  transport.read_result = false;
  transport.write_result = false;
  FakeApi api;
  api.exercise_callbacks = true;
  mower_imu::Bmi270Driver driver(transport, api);
  ASSERT_TRUE(driver.initialize().ok());
  EXPECT_NE(api.read_callback_status, BMI2_INTF_RET_SUCCESS);
  EXPECT_NE(api.write_callback_status, BMI2_INTF_RET_SUCCESS);
}

TEST(Bmi270Driver, ReportsSensorEnableFailure)
{
  FakeTransport transport;
  FakeApi api;
  api.enable_status = BMI2_E_COM_FAIL;
  mower_imu::Bmi270Driver driver(transport, api);
  const auto result = driver.initialize();
  EXPECT_EQ(result.stage, mower_imu::InitializationStage::kSensorEnable);
  EXPECT_EQ(result.api_status, BMI2_E_COM_FAIL);
}
}  // namespace
