# Copyright 2026 Mower maintainers
# SPDX-License-Identifier: Apache-2.0

"""Replay synthetic rosbag2 scenarios against the VIO lifecycle node."""

import subprocess
import tempfile
import time
import unittest
from pathlib import Path

from ament_index_python.packages import get_package_share_directory
from diagnostic_msgs.msg import DiagnosticStatus
import launch
import launch_testing
import launch_testing.actions
from lifecycle_msgs.msg import Transition
from lifecycle_msgs.srv import ChangeState
from nav_msgs.msg import Odometry
import pytest
import rclpy

from launch_ros.actions import LifecycleNode
from mower_localization.vio_test_bag import generate_bag


SCENARIOS = (
    "normal",
    "camera_info_mismatch",
    "imu_gap",
    "imu_duplicate",
    "image_time_jump",
    "camera_stop",
)
EXPECTED_STATE = {
    "normal": "INITIALIZING",
    "camera_info_mismatch": "DEGRADED",
    "imu_gap": "DEGRADED",
    "imu_duplicate": "DEGRADED",
    "image_time_jump": "DEGRADED",
    "camera_stop": "LOST",
}
_TEMP_DIRECTORY = tempfile.TemporaryDirectory(prefix="vio_replay_")
_BAG_PATHS = {}


@pytest.mark.launch_test
def generate_test_description():
    catalog = (
        Path(get_package_share_directory("mower_localization"))
        / "config"
        / "vio_test_scenarios.yaml"
    )
    _BAG_PATHS.clear()
    for scenario in SCENARIOS:
        bag_path = Path(_TEMP_DIRECTORY.name) / scenario
        generate_bag(bag_path, catalog, scenario)
        _BAG_PATHS[scenario] = str(bag_path)

    vio_node = LifecycleNode(
        package="mower_localization",
        executable="basalt_vio_node",
        namespace="",
        name="rosbag_vio_node",
        output="screen",
        parameters=[{
            "calibration_approved": True,
            "max_imu_gap_ms": 25,
            "input_timeout_ms": 1000,
            "imu_queue_capacity": 128,
        }],
    )
    return launch.LaunchDescription([
        vio_node,
        launch_testing.actions.ReadyToTest(),
    ])


class TestVioRosbagReplay(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        rclpy.init()

    @classmethod
    def tearDownClass(cls):
        rclpy.shutdown()
        _TEMP_DIRECTORY.cleanup()

    def setUp(self):
        self.node = rclpy.create_node("test_vio_rosbag_replay")
        self.statuses = []
        self.odometry = []
        self.node.create_subscription(
            DiagnosticStatus, "/vio/status", self.statuses.append, 10
        )
        self.node.create_subscription(
            Odometry, "/vio/odometry", self.odometry.append, 10
        )

    def tearDown(self):
        self.node.destroy_node()

    def _change_state(self, transition_id):
        client = self.node.create_client(
            ChangeState, "/rosbag_vio_node/change_state"
        )
        self.assertTrue(client.wait_for_service(timeout_sec=10.0))
        request = ChangeState.Request()
        request.transition.id = transition_id
        future = client.call_async(request)
        rclpy.spin_until_future_complete(self.node, future, timeout_sec=10.0)
        self.assertIsNotNone(future.result())
        self.assertTrue(future.result().success)

    def _states(self):
        return [
            {value.key: value.value for value in status.values}.get("state")
            for status in self.statuses
        ]

    def _run_scenario(self, bag_path, expected_state):
        self.statuses.clear()
        self.odometry.clear()
        self._change_state(Transition.TRANSITION_CONFIGURE)
        self._change_state(Transition.TRANSITION_ACTIVATE)
        time.sleep(0.2)
        player = subprocess.Popen(
            ["ros2", "bag", "play", bag_path, "--delay", "0.3"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
            text=True,
        )
        deadline = time.monotonic() + 8.0
        while time.monotonic() < deadline:
            rclpy.spin_once(self.node, timeout_sec=0.05)
            if player.poll() is not None and expected_state in self._states():
                break
        try:
            stderr = player.communicate(timeout=2.0)[1]
        except subprocess.TimeoutExpired:
            player.kill()
            stderr = player.communicate()[1]
            self.fail(f"ros2 bag play timed out: {stderr}")
        self.assertEqual(player.returncode, 0, stderr)
        self.assertIn(expected_state, self._states())
        self.assertEqual(self.odometry, [])
        self._change_state(Transition.TRANSITION_DEACTIVATE)
        self._change_state(Transition.TRANSITION_CLEANUP)


    def test_all_catalog_scenarios(self):
        for scenario in SCENARIOS:
            with self.subTest(scenario=scenario):
                self._run_scenario(
                    _BAG_PATHS[scenario], EXPECTED_STATE[scenario]
                )


@launch_testing.post_shutdown_test()
class TestProcessesExitCleanly(unittest.TestCase):

    def test_exit_codes(self, proc_info):
        launch_testing.asserts.assertExitCodes(proc_info)
