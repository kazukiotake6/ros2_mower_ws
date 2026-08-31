// Copyright 2026 Mower maintainers
// SPDX-License-Identifier: Apache-2.0

#include "mower_localization/vio_input_buffer.hpp"

#include <cmath>
#include <stdexcept>

namespace mower_localization
{

VioInputBuffer::VioInputBuffer(const std::size_t imu_capacity)
: imu_capacity_(imu_capacity)
{
  if (imu_capacity_ == 0U) {
    throw std::invalid_argument("IMU buffer capacity must be greater than zero");
  }
}

ImuInsertResult VioInputBuffer::push_imu(const ImuSample & sample)
{
  if (!is_finite(sample)) {
    return ImuInsertResult::kNonFiniteValue;
  }
  if (has_last_stamp_) {
    if (sample.stamp_ns == last_stamp_ns_) {
      return ImuInsertResult::kDuplicateTimestamp;
    }
    if (sample.stamp_ns < last_stamp_ns_) {
      return ImuInsertResult::kOutOfOrderTimestamp;
    }
  }
  // An IMU gap invalidates inertial integration. Do not silently evict an old
  // sample; let the caller mark the estimator degraded or reset it.
  if (imu_samples_.size() >= imu_capacity_) {
    return ImuInsertResult::kCapacityExceeded;
  }
  imu_samples_.push_back(sample);
  last_stamp_ns_ = sample.stamp_ns;
  has_last_stamp_ = true;
  return ImuInsertResult::kAccepted;
}

std::vector<ImuSample> VioInputBuffer::take_imu_through(const std::int64_t stamp_ns)
{
  std::vector<ImuSample> result;
  while (!imu_samples_.empty() && imu_samples_.front().stamp_ns <= stamp_ns) {
    result.push_back(imu_samples_.front());
    imu_samples_.pop_front();
  }
  return result;
}

void VioInputBuffer::reset()
{
  imu_samples_.clear();
  has_last_stamp_ = false;
  last_stamp_ns_ = 0;
}

std::size_t VioInputBuffer::size() const noexcept {return imu_samples_.size();}
std::size_t VioInputBuffer::capacity() const noexcept {return imu_capacity_;}
bool VioInputBuffer::empty() const noexcept {return imu_samples_.empty();}

bool VioInputBuffer::is_finite(const ImuSample & sample) noexcept
{
  for (const double value : sample.angular_velocity) {
    if (!std::isfinite(value)) {return false;}
  }
  for (const double value : sample.linear_acceleration) {
    if (!std::isfinite(value)) {return false;}
  }
  return true;
}

}  // namespace mower_localization
