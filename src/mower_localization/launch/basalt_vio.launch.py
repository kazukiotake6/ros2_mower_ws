# Copyright 2026 Mower maintainers
# SPDX-License-Identifier: Apache-2.0

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import LifecycleNode


def generate_launch_description():
    arguments = [
        DeclareLaunchArgument("calibration_approved", default_value="false"),
        DeclareLaunchArgument("max_imu_gap_ms", default_value="0"),
        DeclareLaunchArgument("input_timeout_ms", default_value="0"),
        DeclareLaunchArgument("imu_queue_capacity", default_value="0"),
    ]
    node = LifecycleNode(
        package="mower_localization",
        executable="basalt_vio_node",
        name="basalt_vio_node",
        output="screen",
        namespace="",
        parameters=[{
            "calibration_approved": LaunchConfiguration("calibration_approved"),
            "max_imu_gap_ms": LaunchConfiguration("max_imu_gap_ms"),
            "input_timeout_ms": LaunchConfiguration("input_timeout_ms"),
            "imu_queue_capacity": LaunchConfiguration("imu_queue_capacity"),
        }],
    )
    return LaunchDescription(arguments + [node])
