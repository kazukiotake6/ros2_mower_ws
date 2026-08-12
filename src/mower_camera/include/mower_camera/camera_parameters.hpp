// Copyright 2026 Mower maintainers
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <string>
namespace mower_camera { struct CameraParameters { int width{1280}; int height{800}; double frame_rate{60.0}; std::string pixel_format{"YUYV"}; std::string frame_id{"camera_optical_frame"}; }; void validate_camera_parameters(const CameraParameters & parameters); std::string image_encoding_for_pixel_format(const std::string & pixel_format); }
namespace mower_camera { void validate_camera_controls(int exposure_time_us, double analogue_gain); }
