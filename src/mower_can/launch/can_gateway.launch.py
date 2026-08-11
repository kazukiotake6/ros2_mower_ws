from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import LifecycleNode
import os


def generate_launch_description():
    config = os.path.join(
        get_package_share_directory('mower_can'), 'config', 'mcp2515.yaml')
    return LaunchDescription([
        LifecycleNode(
            package='mower_can', executable='can_gateway_node',
            name='can_gateway_node', output='screen',
            parameters=[config],
        ),
    ])
