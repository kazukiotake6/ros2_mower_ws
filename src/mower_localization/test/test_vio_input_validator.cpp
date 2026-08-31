// Copyright 2026 Mower maintainers
// SPDX-License-Identifier: Apache-2.0

#include "mower_localization/vio_input_validator.hpp"
#include <limits>
#include "gtest/gtest.h"

namespace
{
using mower_localization::CameraInfoMetadata;
using mower_localization::ImageMetadata;
using mower_localization::ImuInsertResult;
using mower_localization::ImuSample;
using mower_localization::VioInputLimits;
using mower_localization::VioInputValidator;
using mower_localization::VioState;

VioInputValidator configured_validator()
{VioInputValidator validator{VioInputLimits{20, 100}, 4}; EXPECT_TRUE(validator.configure(true)); return validator;}
ImuSample imu_at(const std::int64_t stamp)
{return ImuSample{stamp, {0.1, 0.2, 0.3}, {0.0, 0.0, 9.8}};}
CameraInfoMetadata camera_info_at(const std::int64_t stamp)
{return CameraInfoMetadata{stamp, 1280, 800, "camera_optical_frame", true};}
ImageMetadata image_at(const std::int64_t stamp)
{return ImageMetadata{stamp, 1280, 800, "camera_optical_frame"};}

TEST(VioInputValidator, requiresExplicitLimitsAndApprovedCalibration)
{
  VioInputValidator missing_limits{VioInputLimits{}, 4}; EXPECT_FALSE(missing_limits.configure(true));
  EXPECT_EQ(missing_limits.state(), VioState::kError);
  VioInputValidator unapproved{VioInputLimits{20, 100}, 4}; EXPECT_FALSE(unapproved.configure(false));
  EXPECT_EQ(unapproved.state(), VioState::kError);
}
TEST(VioInputValidator, acceptsMatchingCalibratedInputsBeforeEstimatorTracking)
{
  auto validator = configured_validator(); ASSERT_EQ(validator.push_imu(imu_at(10)), ImuInsertResult::kAccepted);
  validator.update_camera_info(camera_info_at(10)); EXPECT_TRUE(validator.accept_image(image_at(10)));
  EXPECT_EQ(validator.state(), VioState::kInitializing); validator.report_estimator_tracking(true);
  EXPECT_EQ(validator.state(), VioState::kTracking);
}
TEST(VioInputValidator, rejectsMismatchedOrOutOfOrderImages)
{
  auto validator = configured_validator(); ASSERT_EQ(validator.push_imu(imu_at(10)), ImuInsertResult::kAccepted);
  validator.update_camera_info(camera_info_at(10)); auto mismatched = image_at(10); mismatched.width = 640;
  EXPECT_FALSE(validator.accept_image(mismatched)); EXPECT_EQ(validator.state(), VioState::kDegraded);
  ASSERT_EQ(validator.push_imu(imu_at(20)), ImuInsertResult::kAccepted); validator.update_camera_info(camera_info_at(20));
  EXPECT_TRUE(validator.accept_image(image_at(20))); ASSERT_EQ(validator.push_imu(imu_at(30)), ImuInsertResult::kAccepted);
  validator.update_camera_info(camera_info_at(20)); EXPECT_FALSE(validator.accept_image(image_at(20)));
  EXPECT_EQ(validator.rejected_images(), 2U);
}
TEST(VioInputValidator, rejectsInvalidImuAndReportsTimestampGap)
{
  auto validator = configured_validator(); ASSERT_EQ(validator.push_imu(imu_at(10)), ImuInsertResult::kAccepted);
  auto invalid = imu_at(11); invalid.angular_velocity[0] = std::numeric_limits<double>::quiet_NaN();
  EXPECT_EQ(validator.push_imu(invalid), ImuInsertResult::kNonFiniteValue); EXPECT_EQ(validator.state(), VioState::kDegraded);
  EXPECT_EQ(validator.push_imu(imu_at(40)), ImuInsertResult::kAccepted); EXPECT_EQ(validator.state(), VioState::kDegraded);
}
TEST(VioInputValidator, doesNotTreatOldOdometryAsValidAfterLoss)
{
  auto validator = configured_validator(); validator.report_estimator_tracking(true); validator.report_input_timeout();
  EXPECT_EQ(validator.state(), VioState::kLost); EXPECT_NE(validator.state(), VioState::kTracking);
}
}  // namespace
