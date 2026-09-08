"""Launch the libcamera ROS 2 image and CameraInfo publisher."""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue


def generate_launch_description():
    config = os.path.join(
        get_package_share_directory('mower_camera'), 'config', 'libcamera.yaml')
    parameter_arguments = [
        ("width", "1280", int), ("height", "800", int),
        ("frame_rate", "60.0", float), ("pixel_format", "YUYV", str),
        ("frame_id", "camera_optical_frame", str), ("camera_id", "", str),
        ("camera_info_url", "package://mower_camera/config/ov9281_1280x800.yaml", str),
        ("exposure_time_us", "0", int), ("analogue_gain", "0.0", float),
        ("diagnostic_period_s", "1.0", float),
        ("diagnostic_minimum_fps_warn", "-1.0", float),
        ("diagnostic_minimum_fps_error", "-1.0", float),
        ("diagnostic_maximum_queue_latency_ms_warn", "-1.0", float),
        ("diagnostic_maximum_queue_latency_ms_error", "-1.0", float),
        ("diagnostic_missing_frames_warn", "-1", int),
        ("diagnostic_missing_frames_error", "-1", int),
        ("diagnostic_request_failures_warn", "-1", int),
        ("diagnostic_request_failures_error", "-1", int),
    ]
    declarations = [DeclareLaunchArgument(name, default_value=default) for name, default, _ in parameter_arguments]
    overrides = {name: ParameterValue(LaunchConfiguration(name), value_type=value_type) for name, _, value_type in parameter_arguments}
    return LaunchDescription(declarations + [Node(package='mower_camera', executable='libcamera_node', name='libcamera_node', output='screen', parameters=[config, overrides])])
