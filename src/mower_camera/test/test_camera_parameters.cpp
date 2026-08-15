// Copyright 2026 Mower maintainers
// SPDX-License-Identifier: Apache-2.0
#include <stdexcept>
#include <limits>
#include <gtest/gtest.h>
#include "mower_camera/camera_parameters.hpp"
TEST(CameraParameters, accepts_vio_profile) { mower_camera::CameraParameters p; EXPECT_NO_THROW(mower_camera::validate_camera_parameters(p)); EXPECT_EQ(mower_camera::image_encoding_for_pixel_format("yuyv"), "yuv422_yuy2"); }
TEST(CameraParameters, rejects_invalid_frame_rate)
{ mower_camera::CameraParameters p; p.frame_rate = 0.0; EXPECT_THROW(mower_camera::validate_camera_parameters(p), std::invalid_argument); }
TEST(CameraParameters, rejects_non_finite_frame_rate)
{
  mower_camera::CameraParameters p;
  p.frame_rate = std::numeric_limits<double>::quiet_NaN();
  EXPECT_THROW(mower_camera::validate_camera_parameters(p), std::invalid_argument);
  p.frame_rate = std::numeric_limits<double>::infinity();
  EXPECT_THROW(mower_camera::validate_camera_parameters(p), std::invalid_argument);
}
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

TEST(CameraParameters, validates_image_buffer_layout)
{
  mower_camera::ImageBufferLayout layout{1280, 800, 2560, 2048000, 2048000, "YUYV"};
  EXPECT_NO_THROW(mower_camera::validate_image_buffer_layout(layout));
  layout.stride = 2559;
  EXPECT_THROW(mower_camera::validate_image_buffer_layout(layout), std::invalid_argument);
  layout.stride = 2560; layout.bytes_used = 2047999;
  EXPECT_THROW(mower_camera::validate_image_buffer_layout(layout), std::invalid_argument);
  layout.bytes_used = 2048000; layout.mapped_length = 2047999;
  EXPECT_THROW(mower_camera::validate_image_buffer_layout(layout), std::invalid_argument);
}
