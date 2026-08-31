// Copyright 2026 Mower maintainers
// SPDX-License-Identifier: Apache-2.0

#include <chrono>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "diagnostic_msgs/msg/diagnostic_status.hpp"
#include "gtest/gtest.h"
#include "nav_msgs/msg/odometry.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp/executors/single_threaded_executor.hpp"
#include "rclcpp_lifecycle/state.hpp"
#include "sensor_msgs/msg/camera_info.hpp"
#include "sensor_msgs/msg/image.hpp"
#include "sensor_msgs/msg/imu.hpp"

#include "mower_localization/basalt_vio_node.hpp"
#include "mower_localization/vio_estimator_adapter.hpp"

namespace
{
using namespace std::chrono_literals;
using mower_localization::BasaltVioNode;
using mower_localization::EstimatorResult;
using mower_localization::EstimatorState;
using mower_localization::VioEstimatorAdapter;

class FakeEstimator final : public VioEstimatorAdapter
{
public:
  void reset() override {++reset_count; imu_count = 0;}
  bool add_imu(const sensor_msgs::msg::Imu &) override
  {
    ++imu_count;
    return accept_imu;
  }
  EstimatorResult process_image(
    const sensor_msgs::msg::Image & image,
    const sensor_msgs::msg::CameraInfo &) override
  {
    if (results.empty()) {
      return {EstimatorState::kInitializing, std::nullopt, "waiting"};
    }
    auto result = results.front();
    results.pop_front();
    if (result.state == EstimatorState::kTracking && result.odometry) {
      result.odometry->header.stamp = image.header.stamp;
    }
    return result;
  }

  std::deque<EstimatorResult> results;
  std::size_t reset_count{};
  std::size_t imu_count{};
  bool accept_imu{true};
};

class VioEstimatorAdapterTest : public ::testing::Test
{
protected:
  static void SetUpTestSuite()
  {
    if (!rclcpp::ok()) {
      int argc = 0;
      rclcpp::init(argc, nullptr);
    }
  }

  static void TearDownTestSuite()
  {
    if (rclcpp::ok()) {rclcpp::shutdown();}
  }

  void SetUp() override
  {
    estimator_ = std::make_shared<FakeEstimator>();
    rclcpp::NodeOptions options;
    options.parameter_overrides({
        rclcpp::Parameter("calibration_approved", true),
        rclcpp::Parameter("max_imu_gap_ms", 100),
        rclcpp::Parameter("input_timeout_ms", 5000),
        rclcpp::Parameter("imu_queue_capacity", 32),
    });
    vio_node_ = std::make_shared<BasaltVioNode>(estimator_, options);
    test_node_ = std::make_shared<rclcpp::Node>("test_vio_estimator_adapter");
    executor_.add_node(vio_node_->get_node_base_interface());
    executor_.add_node(test_node_);

    odometry_subscription_ = test_node_->create_subscription<nav_msgs::msg::Odometry>(
      "/vio/odometry", 10,
      [this](nav_msgs::msg::Odometry::ConstSharedPtr message) {odometry_.push_back(*message);});
    status_subscription_ = test_node_->create_subscription<diagnostic_msgs::msg::DiagnosticStatus>(
      "/vio/status", 10,
      [this](diagnostic_msgs::msg::DiagnosticStatus::ConstSharedPtr message) {
        statuses_.push_back(*message);
      });
    imu_publisher_ = test_node_->create_publisher<sensor_msgs::msg::Imu>(
      "/imu/data_raw", rclcpp::SensorDataQoS());
    info_publisher_ = test_node_->create_publisher<sensor_msgs::msg::CameraInfo>(
      "/camera_info", rclcpp::SensorDataQoS());
    image_publisher_ = test_node_->create_publisher<sensor_msgs::msg::Image>(
      "/image_raw", rclcpp::SensorDataQoS());

    ASSERT_EQ(
      vio_node_->on_configure(rclcpp_lifecycle::State()),
      BasaltVioNode::CallbackReturn::SUCCESS);
    ASSERT_EQ(
      vio_node_->on_activate(rclcpp_lifecycle::State()),
      BasaltVioNode::CallbackReturn::SUCCESS);
    spin_for(300ms);
  }

  void TearDown() override
  {
    if (vio_node_) {
      if (!cleaned_) {
        vio_node_->on_deactivate(rclcpp_lifecycle::State());
        vio_node_->on_cleanup(rclcpp_lifecycle::State());
      }
      executor_.remove_node(vio_node_->get_node_base_interface());
    }
    if (test_node_) {executor_.remove_node(test_node_);}
    odometry_subscription_.reset();
    status_subscription_.reset();
    imu_publisher_.reset();
    info_publisher_.reset();
    image_publisher_.reset();
    vio_node_.reset();
    test_node_.reset();
    estimator_.reset();
  }

  void spin_for(std::chrono::milliseconds duration)
  {
    const auto deadline = std::chrono::steady_clock::now() + duration;
    while (std::chrono::steady_clock::now() < deadline) {
      executor_.spin_some();
      std::this_thread::sleep_for(5ms);
    }
  }

  void publish_inputs(std::int32_t second)
  {
    builtin_interfaces::msg::Time stamp;
    stamp.sec = second;

    sensor_msgs::msg::CameraInfo info;
    info.header.stamp = stamp;
    info.header.frame_id = "camera_optical_frame";
    info.width = 1280;
    info.height = 800;
    info.k[0] = 500.0;
    info.k[4] = 500.0;
    info.d = {0.01};
    info_publisher_->publish(info);
    spin_for(50ms);

    sensor_msgs::msg::Imu imu;
    imu.header.stamp = stamp;
    imu.header.frame_id = "imu_link";
    imu.angular_velocity.x = 0.1;
    imu.linear_acceleration.z = 9.8;
    imu_publisher_->publish(imu);
    spin_for(50ms);

    sensor_msgs::msg::Image image;
    image.header.stamp = stamp;
    image.header.frame_id = "camera_optical_frame";
    image.width = 1280;
    image.height = 800;
    image.encoding = "mono8";
    image.step = 1280;
    image_publisher_->publish(image);
    spin_for(150ms);
  }

  std::string latest_state() const
  {
    if (statuses_.empty()) {return {};}
    for (const auto & value : statuses_.back().values) {
      if (value.key == "state") {return value.value;}
    }
    return {};
  }

  static nav_msgs::msg::Odometry valid_odometry()
  {
    nav_msgs::msg::Odometry odometry;
    odometry.header.frame_id = "vio_odom";
    odometry.child_frame_id = "base_link";
    odometry.pose.pose.orientation.w = 1.0;
    odometry.pose.covariance[0] = 0.25;
    return odometry;
  }

  std::shared_ptr<FakeEstimator> estimator_;
  std::shared_ptr<BasaltVioNode> vio_node_;
  std::shared_ptr<rclcpp::Node> test_node_;
  rclcpp::executors::SingleThreadedExecutor executor_;
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odometry_subscription_;
  rclcpp::Subscription<diagnostic_msgs::msg::DiagnosticStatus>::SharedPtr status_subscription_;
  rclcpp::Publisher<sensor_msgs::msg::Imu>::SharedPtr imu_publisher_;
  rclcpp::Publisher<sensor_msgs::msg::CameraInfo>::SharedPtr info_publisher_;
  rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr image_publisher_;
  std::vector<nav_msgs::msg::Odometry> odometry_;
  std::vector<diagnostic_msgs::msg::DiagnosticStatus> statuses_;
  bool cleaned_{false};
};

TEST_F(VioEstimatorAdapterTest, publishesOnlyTrackingOdometryAndStopsAfterLoss)
{
  estimator_->results.push_back(
    {EstimatorState::kTracking, valid_odometry(), "tracking"});
  publish_inputs(1);
  ASSERT_EQ(odometry_.size(), 1U);
  EXPECT_EQ(latest_state(), "TRACKING");
  EXPECT_DOUBLE_EQ(odometry_.front().pose.covariance[0], 0.25);

  estimator_->results.push_back(
    {EstimatorState::kLost, std::nullopt, "tracking lost"});
  publish_inputs(2);
  EXPECT_EQ(odometry_.size(), 1U);
  EXPECT_EQ(latest_state(), "LOST");

  ASSERT_EQ(
    vio_node_->on_deactivate(rclcpp_lifecycle::State()),
    BasaltVioNode::CallbackReturn::SUCCESS);
  estimator_->results.push_back(
    {EstimatorState::kTracking, valid_odometry(), "tracking"});
  publish_inputs(3);
  EXPECT_EQ(odometry_.size(), 1U);

  ASSERT_EQ(
    vio_node_->on_cleanup(rclcpp_lifecycle::State()),
    BasaltVioNode::CallbackReturn::SUCCESS);
  EXPECT_GE(estimator_->reset_count, 2U);
  cleaned_ = true;
}

TEST_F(VioEstimatorAdapterTest, rejectsTrackingWithoutValidOdometry)
{
  estimator_->results.push_back(
    {EstimatorState::kTracking, std::nullopt, "tracking"});
  publish_inputs(1);
  EXPECT_TRUE(odometry_.empty());
  EXPECT_EQ(latest_state(), "ERROR");
  ASSERT_FALSE(statuses_.empty());
  EXPECT_EQ(statuses_.back().level, diagnostic_msgs::msg::DiagnosticStatus::ERROR);
}

TEST_F(VioEstimatorAdapterTest, reportsEstimatorImuFailureWithoutPublishing)
{
  estimator_->accept_imu = false;
  publish_inputs(1);
  EXPECT_TRUE(odometry_.empty());
  EXPECT_EQ(latest_state(), "ERROR");
}
TEST_F(VioEstimatorAdapterTest, reportsEstimatorInternalErrorWithoutPublishing)
{
  estimator_->results.push_back(
    {EstimatorState::kError, std::nullopt, "fake estimator failure"});
  publish_inputs(1);
  EXPECT_TRUE(odometry_.empty());
  EXPECT_EQ(latest_state(), "ERROR");
  ASSERT_FALSE(statuses_.empty());
  EXPECT_EQ(statuses_.back().message, "fake estimator failure");
}


}  // namespace
