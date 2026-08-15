// Copyright 2026 Mower maintainers
// SPDX-License-Identifier: Apache-2.0

#include <sys/mman.h>

#include <camera_info_manager/camera_info_manager.hpp>
#include <diagnostic_msgs/msg/diagnostic_array.hpp>
#include <diagnostic_msgs/msg/diagnostic_status.hpp>
#include <libcamera/libcamera.h>
#include <libcamera/formats.h>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "mower_camera/camera_parameters.hpp"

namespace
{
libcamera::PixelFormat pixel_format_for_parameter(const std::string & pixel_format)
{
  std::string normalized = pixel_format;
  std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char value) {
    return std::toupper(value);
  });
  if (normalized == "YUYV" || normalized == "YUYV8") return libcamera::formats::YUYV;
  if (normalized == "RGB888") return libcamera::formats::RGB888;
  if (normalized == "BGR888") return libcamera::formats::BGR888;
  if (normalized == "R8" || normalized == "Y8" || normalized == "MONO8") return libcamera::formats::R8;
  throw std::invalid_argument("unsupported pixel_format");
}

class LibcameraNode : public rclcpp::Node
{
public:
  LibcameraNode() : Node("libcamera_node"), info_manager_(this, "mower_camera")
  {
    params_.width = declare_parameter<int>("width", 1280);
    params_.height = declare_parameter<int>("height", 800);
    params_.frame_rate = declare_parameter<double>("frame_rate", 60.0);
    params_.pixel_format = declare_parameter<std::string>("pixel_format", "YUYV");
    params_.frame_id = declare_parameter<std::string>("frame_id", "camera_optical_frame");
    camera_id_ = declare_parameter<std::string>("camera_id", "");
    camera_info_url_ = declare_parameter<std::string>("camera_info_url", "");
    fixed_exposure_us_ = declare_parameter<int>("exposure_time_us", 0);
    fixed_gain_ = declare_parameter<double>("analogue_gain", 0.0);
    mower_camera::validate_camera_parameters(params_);
    mower_camera::validate_camera_controls(fixed_exposure_us_, fixed_gain_);
    if (!camera_info_url_.empty()) {
      if (!info_manager_.loadCameraInfo(camera_info_url_)) {
        throw std::runtime_error("failed to load camera calibration: " + camera_info_url_);
      }
      mower_camera::validate_calibrated_camera_info(
        info_manager_.getCameraInfo(), params_.width, params_.height);
    }
    image_pub_ = create_publisher<sensor_msgs::msg::Image>("image_raw", rclcpp::SensorDataQoS());
    info_pub_ = create_publisher<sensor_msgs::msg::CameraInfo>("camera_info", rclcpp::SensorDataQoS());
    diagnostics_pub_ = create_publisher<diagnostic_msgs::msg::DiagnosticArray>("/diagnostics", 10);
    initialize();
  }

  ~LibcameraNode() override { stop(); }

private:
  void initialize()
  {
    manager_ = std::make_unique<libcamera::CameraManager>();
    if (manager_->start() != 0 || manager_->cameras().empty()) throw std::runtime_error("no libcamera camera found");
    camera_ = camera_id_.empty() ? manager_->cameras().front() : manager_->get(camera_id_);
    if (!camera_) throw std::runtime_error("requested camera_id was not found");
    if (camera_->acquire() != 0) throw std::runtime_error("failed to acquire camera");
    config_ = camera_->generateConfiguration({libcamera::StreamRole::Viewfinder});
    if (!config_ || config_->empty()) throw std::runtime_error("camera has no viewfinder configuration");
    auto & stream_config = config_->at(0);
    stream_config.size.width = params_.width;
    stream_config.size.height = params_.height;
    stream_config.pixelFormat = pixel_format_for_parameter(params_.pixel_format);
    if (config_->validate() == libcamera::CameraConfiguration::Invalid || camera_->configure(config_.get()) != 0) throw std::runtime_error("requested camera stream is unsupported");
    stream_ = stream_config.stream();
    allocator_ = std::make_unique<libcamera::FrameBufferAllocator>(camera_);
    if (allocator_->allocate(stream_) < 0) throw std::runtime_error("failed to allocate camera buffers");
    for (const auto & buffer : allocator_->buffers(stream_)) {
      for (const auto & plane : buffer->planes()) {
        void * address = mmap(nullptr, plane.length, PROT_READ, MAP_SHARED, plane.fd.get(), plane.offset);
        if (address == MAP_FAILED) throw std::runtime_error("failed to map camera buffer");
        mappings_[buffer.get()].push_back({address, plane.length});
      }
      auto request = camera_->createRequest();
      if (!request || request->addBuffer(stream_, buffer.get()) < 0) throw std::runtime_error("failed to create camera request");
      requests_.push_back(std::move(request));
    }
    camera_->requestCompleted.connect(this, &LibcameraNode::request_complete);
    libcamera::ControlList controls(camera_->controls());
    const auto frame_duration_us = static_cast<int64_t>(1000000.0 / params_.frame_rate);
    const std::array<int64_t, 2> frame_duration_limits{frame_duration_us, frame_duration_us};
    controls.set(
      libcamera::controls::FrameDurationLimits,
      libcamera::Span<const int64_t, 2>(frame_duration_limits));
    if (fixed_exposure_us_ > 0) controls.set(libcamera::controls::ExposureTime, fixed_exposure_us_);
    if (fixed_gain_ > 0.0) controls.set(libcamera::controls::AnalogueGain, fixed_gain_);
    if (camera_->start(&controls) != 0) throw std::runtime_error("failed to start camera");
    for (auto & request : requests_) if (camera_->queueRequest(request.get()) < 0) throw std::runtime_error("failed to queue camera request");
    publish_diagnostic(diagnostic_msgs::msg::DiagnosticStatus::OK, "streaming");
  }

  void request_complete(libcamera::Request * request)
  {
    if (request->status() == libcamera::Request::RequestCancelled) return;
    const auto buffer = request->buffers().at(stream_);
    const auto metadata = buffer->metadata();
    const auto plane = metadata.planes()[0];
    const auto mapping = mappings_.at(buffer).at(0);
    sensor_msgs::msg::Image image;
    // libcamera timestamps use the monotonic clock while ROS timestamps may use a
    // different clock. Do not publish a timestamp in the wrong clock domain.
    image.header.stamp = now();
    image.header.frame_id = params_.frame_id;
    image.width = config_->at(0).size.width; image.height = config_->at(0).size.height;
    image.encoding = mower_camera::image_encoding_for_pixel_format(params_.pixel_format);
    image.is_bigendian = false; image.step = config_->at(0).stride;
    image.data.assign(static_cast<uint8_t *>(mapping.address), static_cast<uint8_t *>(mapping.address) + plane.bytesused);
    image_pub_->publish(image);
    auto info = info_manager_.getCameraInfo(); info.header = image.header; info.width = image.width; info.height = image.height; info_pub_->publish(info);
    ++frames_; request->reuse(libcamera::Request::ReuseBuffers); camera_->queueRequest(request);
  }

  struct Mapping { void * address; size_t length; };
  void publish_diagnostic(uint8_t level, const std::string & message)
  { diagnostic_msgs::msg::DiagnosticArray a; a.header.stamp = now(); diagnostic_msgs::msg::DiagnosticStatus s; s.name = "mower_camera/libcamera"; s.level = level; s.message = message; s.hardware_id = camera_id_; a.status.push_back(s); diagnostics_pub_->publish(a); }
  void stop()
  { if (camera_) { camera_->stop(); for (auto & item : mappings_) for (auto & mapping : item.second) munmap(mapping.address, mapping.length); allocator_.reset(); camera_->release(); } if (manager_) manager_->stop(); }

  mower_camera::CameraParameters params_; std::string camera_id_, camera_info_url_; int fixed_exposure_us_{}; double fixed_gain_{}; uint64_t frames_{};
  camera_info_manager::CameraInfoManager info_manager_; rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr image_pub_; rclcpp::Publisher<sensor_msgs::msg::CameraInfo>::SharedPtr info_pub_; rclcpp::Publisher<diagnostic_msgs::msg::DiagnosticArray>::SharedPtr diagnostics_pub_;
  std::unique_ptr<libcamera::CameraManager> manager_; std::shared_ptr<libcamera::Camera> camera_; std::unique_ptr<libcamera::CameraConfiguration> config_; libcamera::Stream * stream_{}; std::unique_ptr<libcamera::FrameBufferAllocator> allocator_; std::vector<std::unique_ptr<libcamera::Request>> requests_; std::unordered_map<const libcamera::FrameBuffer *, std::vector<Mapping>> mappings_;
};
}
int main(int argc, char ** argv) { rclcpp::init(argc, argv); try { rclcpp::spin(std::make_shared<LibcameraNode>()); } catch (const std::exception & e) { RCLCPP_FATAL(rclcpp::get_logger("libcamera_node"), "%s", e.what()); rclcpp::shutdown(); return 1; } rclcpp::shutdown(); return 0; }
