// Copyright 2026 Mower maintainers
// SPDX-License-Identifier: Apache-2.0
#include <gtest/gtest.h>
#include "mower_can/can_protocol.hpp"

TEST(CanProtocol, DisabledOrExpiredCommandCarriesStopAndZeroVelocity)
{
  const auto frame = mower_can::make_motion_frame(0x200, 7, false, 1.0, -1.0);
  EXPECT_EQ(frame.can_id, 0x200U); EXPECT_EQ(frame.can_dlc, 8U); EXPECT_EQ(frame.data[0], 7U);
  EXPECT_EQ(frame.data[1], mower_can::kCommandStop); EXPECT_EQ(frame.data[2], 0U); EXPECT_EQ(frame.data[3], 0U);
}
TEST(CanProtocol, VelocityIsSaturatedToInt16)
{
  const auto frame = mower_can::make_motion_frame(0x200, 0, true, 100.0, -100.0);
  EXPECT_EQ(frame.data[1], mower_can::kCommandEnable); EXPECT_EQ(frame.data[2], 0xFFU); EXPECT_EQ(frame.data[3], 0x7FU);
  EXPECT_EQ(frame.data[4], 0x00U); EXPECT_EQ(frame.data[5], 0x80U);
}
TEST(CanProtocol, HeartbeatHasVersionAndSequence)
{
  const auto frame = mower_can::make_heartbeat_frame(0x201, 42);
  EXPECT_EQ(frame.can_dlc, 2U); EXPECT_EQ(frame.data[0], mower_can::kProtocolVersion); EXPECT_EQ(frame.data[1], 42U);
}
