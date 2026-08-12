// Copyright 2026 Mower maintainers
// SPDX-License-Identifier: Apache-2.0
#include "mower_camera/camera_parameters.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>
namespace mower_camera { namespace { std::string uppercase(std::string value) { std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {return std::toupper(c);}); return value; } }
void validate_camera_parameters(const CameraParameters & p) { if (p.width <= 0 || p.height <= 0) throw std::invalid_argument("width and height must be positive"); if (p.frame_rate <= 0.0 || p.frame_rate > 240.0) throw std::invalid_argument("frame_rate must be within (0, 240]"); if (p.frame_id.empty()) throw std::invalid_argument("frame_id must not be empty"); (void)image_encoding_for_pixel_format(p.pixel_format); }
std::string image_encoding_for_pixel_format(const std::string & f) { const auto v = uppercase(f); if (v == "YUYV" || v == "YUYV8") return "yuv422_yuy2"; if (v == "RGB888") return "rgb8"; if (v == "BGR888") return "bgr8"; if (v == "R8" || v == "Y8" || v == "MONO8") return "mono8"; throw std::invalid_argument("pixel_format must be YUYV, RGB888, BGR888, or R8/MONO8"); } }
namespace mower_camera { void validate_camera_controls(int exposure_time_us, double analogue_gain) { if (exposure_time_us < 0) throw std::invalid_argument("exposure_time_us must be non-negative"); if (!std::isfinite(analogue_gain) || analogue_gain < 0.0) throw std::invalid_argument("analogue_gain must be finite and non-negative"); } }
