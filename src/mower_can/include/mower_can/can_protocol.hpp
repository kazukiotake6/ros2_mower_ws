// Copyright 2026 Mower maintainers
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <linux/can.h>

namespace mower_can
{
constexpr uint8_t kProtocolVersion = 1;
constexpr uint8_t kCommandEnable = 0x01;
constexpr uint8_t kCommandStop = 0x02;

inline int16_t saturating_scale(double value, double scale)
{
  const double scaled = std::round(value * scale);
  return static_cast<int16_t>(std::clamp(scaled, -32768.0, 32767.0));
}

inline void put_i16_le(uint8_t * bytes, int16_t value)
{
  const auto raw = static_cast<uint16_t>(value);
  bytes[0] = static_cast<uint8_t>(raw & 0xffU);
  bytes[1] = static_cast<uint8_t>((raw >> 8U) & 0xffU);
}

// Provisional motion frame. The MCU protocol review must approve IDs and layout
// before this frame is connected to enabled motor hardware.
inline can_frame make_motion_frame(
  canid_t id, uint8_t sequence, bool enabled, double linear_mps, double angular_radps)
{
  can_frame frame{};
  frame.can_id = id;
  frame.can_dlc = 8;
  frame.data[0] = sequence;
  frame.data[1] = enabled ? kCommandEnable : kCommandStop;
  put_i16_le(&frame.data[2], saturating_scale(enabled ? linear_mps : 0.0, 1000.0));
  put_i16_le(&frame.data[4], saturating_scale(enabled ? angular_radps : 0.0, 1000.0));
  frame.data[6] = kProtocolVersion;
  frame.data[7] = 0;
  return frame;
}

inline can_frame make_heartbeat_frame(canid_t id, uint8_t sequence)
{
  can_frame frame{};
  frame.can_id = id;
  frame.can_dlc = 2;
  frame.data[0] = kProtocolVersion;
  frame.data[1] = sequence;
  return frame;
}
}  // namespace mower_can
