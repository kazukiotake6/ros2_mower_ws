// Copyright 2026 Mower maintainers
// SPDX-License-Identifier: Apache-2.0

#include "mower_localization/vio_input_buffer.hpp"

#include <limits>
#include <stdexcept>

#include "gtest/gtest.h"

namespace
{
using mower_localization::ImuInsertResult;
using mower_localization::ImuSample;
using mower_localization::VioInputBuffer;

ImuSample sample_at(const std::int64_t stamp_ns)
{
  return ImuSample{stamp_ns, {0.1, 0.2, 0.3}, {1.0, 2.0, 9.8}};
}

TEST(VioInputBuffer, rejectsZeroCapacity)
{
  EXPECT_THROW(VioInputBuffer{0U}, std::invalid_argument);
}

TEST(VioInputBuffer, acceptsStrictlyIncreasingSamples)
{
  VioInputBuffer buffer{3U};
  EXPECT_EQ(buffer.push_imu(sample_at(10)), ImuInsertResult::kAccepted);
  EXPECT_EQ(buffer.push_imu(sample_at(20)), ImuInsertResult::kAccepted);
  EXPECT_EQ(buffer.size(), 2U);
}

TEST(VioInputBuffer, rejectsDuplicateAndOutOfOrderSamples)
{
  VioInputBuffer buffer{3U};
  ASSERT_EQ(buffer.push_imu(sample_at(20)), ImuInsertResult::kAccepted);
  EXPECT_EQ(buffer.push_imu(sample_at(20)), ImuInsertResult::kDuplicateTimestamp);
  EXPECT_EQ(buffer.push_imu(sample_at(19)), ImuInsertResult::kOutOfOrderTimestamp);
  EXPECT_EQ(buffer.size(), 1U);
}

TEST(VioInputBuffer, rejectsNonFiniteMeasurements)
{
  VioInputBuffer buffer{2U};
  auto sample = sample_at(10);
  sample.angular_velocity[1] = std::numeric_limits<double>::quiet_NaN();
  EXPECT_EQ(buffer.push_imu(sample), ImuInsertResult::kNonFiniteValue);
  sample = sample_at(11);
  sample.linear_acceleration[2] = std::numeric_limits<double>::infinity();
  EXPECT_EQ(buffer.push_imu(sample), ImuInsertResult::kNonFiniteValue);
  EXPECT_TRUE(buffer.empty());
}

TEST(VioInputBuffer, reportsCapacityWithoutSilentlyDroppingImu)
{
  VioInputBuffer buffer{2U};
  ASSERT_EQ(buffer.push_imu(sample_at(10)), ImuInsertResult::kAccepted);
  ASSERT_EQ(buffer.push_imu(sample_at(20)), ImuInsertResult::kAccepted);
  EXPECT_EQ(buffer.push_imu(sample_at(30)), ImuInsertResult::kCapacityExceeded);
  const auto samples = buffer.take_imu_through(100);
  ASSERT_EQ(samples.size(), 2U);
  EXPECT_EQ(samples[0].stamp_ns, 10);
  EXPECT_EQ(samples[1].stamp_ns, 20);
}

TEST(VioInputBuffer, takesOnlySamplesAtOrBeforeImageTime)
{
  VioInputBuffer buffer{4U};
  ASSERT_EQ(buffer.push_imu(sample_at(10)), ImuInsertResult::kAccepted);
  ASSERT_EQ(buffer.push_imu(sample_at(20)), ImuInsertResult::kAccepted);
  ASSERT_EQ(buffer.push_imu(sample_at(30)), ImuInsertResult::kAccepted);
  const auto first = buffer.take_imu_through(20);
  ASSERT_EQ(first.size(), 2U);
  EXPECT_EQ(first.back().stamp_ns, 20);
  EXPECT_EQ(buffer.size(), 1U);
  EXPECT_TRUE(buffer.take_imu_through(29).empty());
}

TEST(VioInputBuffer, resetAllowsAReinitializedTimestampEpoch)
{
  VioInputBuffer buffer{2U};
  ASSERT_EQ(buffer.push_imu(sample_at(100)), ImuInsertResult::kAccepted);
  buffer.reset();
  EXPECT_TRUE(buffer.empty());
  EXPECT_EQ(buffer.push_imu(sample_at(1)), ImuInsertResult::kAccepted);
}
}  // namespace
