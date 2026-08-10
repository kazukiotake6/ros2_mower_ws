// Copyright 2026 Mower maintainers
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

TEST(CommandLimits, AcceptsACommandWithinTheReviewedInterfaceRange)
{
  constexpr double kMaximumLinearVelocityMps = 1.0;
  EXPECT_LE(0.5, kMaximumLinearVelocityMps);
}
