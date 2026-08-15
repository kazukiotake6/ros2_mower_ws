// Copyright 2026 Mower maintainers
// SPDX-License-Identifier: Apache-2.0
#include <stdexcept>
#include <limits>
#include <gtest/gtest.h>
#include "mower_camera/camera_parameters.hpp"
TEST(CameraParameters, accepts_vio_profile) { mower_camera::CameraParameters p; EXPECT_NO_THROW(mower_camera::validate_camera_parameters(p)); EXPECT_EQ(mower_camera::image_encoding_for_pixel_format("yuyv"), "yuv422_yuy2"); }
TEST(CameraParameters, rejects_invalid_frame_rate)
{ mower_camera::CameraParameters p; p.frame_rate = 0.0; EXPECT_THROW(mower_camera::validate_camera_parameters(p), std::invalid_argument); }
TEST(CameraParameters, rejects_unknown_pixel_format)
{ mower_camera::CameraParameters p; p.pixel_format = "NV12"; EXPECT_THROW(mower_camera::validate_camera_parameters(p), std::invalid_argument); }
TEST(CameraParameters, rejects_invalid_camera_controls)
{
  EXPECT_NO_THROW(mower_camera::validate_camera_controls(0, 0.0));
  EXPECT_NO_THROW(mower_camera::validate_camera_controls(4000, 2.0));
  EXPECT_THROW(mower_camera::validate_camera_controls(-1, 0.0), std::invalid_argument);
  EXPECT_THROW(mower_camera::validate_camera_controls(0, -0.1), std::invalid_argument);
  EXPECT_THROW(mower_camera::validate_camera_controls(
    0, std::numeric_limits<double>::infinity()), std::invalid_argument);
}

TEST(CameraParameters, rejects_invalid_dimensions_and_frame_id)
{
  mower_camera::CameraParameters p;
  p.width = 0;
  EXPECT_THROW(mower_camera::validate_camera_parameters(p), std::invalid_argument);
  p.width = 1280; p.height = -1;
  EXPECT_THROW(mower_camera::validate_camera_parameters(p), std::invalid_argument);
  p.height = 800; p.frame_id = "";
  EXPECT_THROW(mower_camera::validate_camera_parameters(p), std::invalid_argument);
}

TEST(CameraCalibration, accepts_matching_calibration)
{
  sensor_msgs::msg::CameraInfo info;
  info.width = 1280; info.height = 800; info.distortion_model = "plumb_bob";
  info.d = {0.1, -0.2, 0.0, 0.0, 0.0};
  info.k = {500.0, 0.0, 640.0, 0.0, 501.0, 400.0, 0.0, 0.0, 1.0};
  info.p = {500.0, 0.0, 640.0, 0.0, 0.0, 501.0, 400.0, 0.0, 0.0, 0.0, 1.0, 0.0};
  EXPECT_NO_THROW(mower_camera::validate_calibrated_camera_info(info, 1280, 800));
}

TEST(CameraCalibration, rejects_size_mismatch_and_invalid_intrinsics)
{
  sensor_msgs::msg::CameraInfo info;
  info.width = 640; info.height = 480; info.distortion_model = "plumb_bob"; info.d = {0.1};
  info.k = {500.0, 0.0, 320.0, 0.0, 500.0, 240.0, 0.0, 0.0, 1.0};
  info.p = {500.0, 0.0, 320.0, 0.0, 0.0, 500.0, 240.0, 0.0, 0.0, 0.0, 1.0, 0.0};
  EXPECT_THROW(mower_camera::validate_calibrated_camera_info(info, 1280, 800), std::invalid_argument);
  info.width = 1280; info.height = 800; info.k[0] = 0.0;
  EXPECT_THROW(mower_camera::validate_calibrated_camera_info(info, 1280, 800), std::invalid_argument);
}
