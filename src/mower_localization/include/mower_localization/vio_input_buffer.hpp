// Copyright 2026 Mower maintainers
// SPDX-License-Identifier: Apache-2.0

#ifndef MOWER_LOCALIZATION__VIO_INPUT_BUFFER_HPP_
#define MOWER_LOCALIZATION__VIO_INPUT_BUFFER_HPP_

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

namespace mower_localization
{

struct ImuSample
{
  std::int64_t stamp_ns{};
  std::array<double, 3> angular_velocity{};
  std::array<double, 3> linear_acceleration{};
};

enum class ImuInsertResult
{
  kAccepted,
  kDuplicateTimestamp,
  kOutOfOrderTimestamp,
  kNonFiniteValue,
  kCapacityExceeded,
};

class VioInputBuffer
{
public:
  explicit VioInputBuffer(std::size_t imu_capacity);

  ImuInsertResult push_imu(const ImuSample & sample);
  std::vector<ImuSample> take_imu_through(std::int64_t stamp_ns);
  void reset();

  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] std::size_t capacity() const noexcept;
  [[nodiscard]] bool empty() const noexcept;

private:
  static bool is_finite(const ImuSample & sample) noexcept;

  std::size_t imu_capacity_;
  std::deque<ImuSample> imu_samples_;
  bool has_last_stamp_{false};
  std::int64_t last_stamp_ns_{};
};

}  // namespace mower_localization

#endif  // MOWER_LOCALIZATION__VIO_INPUT_BUFFER_HPP_
