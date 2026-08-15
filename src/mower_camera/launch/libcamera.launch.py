"""Launch the libcamera ROS 2 image and CameraInfo publisher."""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    config = os.path.join(
        get_package_share_directory('mower_camera'), 'config', 'libcamera.yaml')
    return LaunchDescription([
        Node(
            package='mower_camera',
            executable='libcamera_node',
            name='libcamera_node',
            output='screen',
            parameters=[config],
        ),
    ])
