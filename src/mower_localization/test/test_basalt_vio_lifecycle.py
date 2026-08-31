# Copyright 2026 Mower maintainers
# SPDX-License-Identifier: Apache-2.0

import time
import unittest

import launch
import launch_testing
import launch_testing.actions
import pytest
import rclpy
from diagnostic_msgs.msg import DiagnosticArray, DiagnosticStatus
from lifecycle_msgs.msg import Transition
from lifecycle_msgs.srv import ChangeState
from nav_msgs.msg import Odometry
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import CameraInfo, Image, Imu

from launch_ros.actions import LifecycleNode


@pytest.mark.launch_test
def generate_test_description():
    valid_node = LifecycleNode(
        package="mower_localization",
        executable="basalt_vio_node",
        namespace="",
        name="basalt_vio_node",
        output="screen",
        parameters=[{
            "calibration_approved": True,
            "max_imu_gap_ms": 20,
            "input_timeout_ms": 300,
            "imu_queue_capacity": 32,
        }],
    )
    invalid_node = LifecycleNode(
        package="mower_localization",
        executable="basalt_vio_node",
        namespace="invalid",
        name="unapproved_vio_node",
        output="screen",
        parameters=[{
            "calibration_approved": False,
            "max_imu_gap_ms": 20,
            "input_timeout_ms": 300,
            "imu_queue_capacity": 32,
        }],
    )
    return launch.LaunchDescription([
        valid_node,
        invalid_node,
        launch_testing.actions.ReadyToTest(),
    ])


class TestBasaltVioLifecycle(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        rclpy.init()

    @classmethod
    def tearDownClass(cls):
        rclpy.shutdown()

    def setUp(self):
        self.node = rclpy.create_node("test_basalt_vio_lifecycle")
        self.statuses = []
        self.diagnostics = []
        self.odometry = []
        self.node.create_subscription(
            DiagnosticStatus, "/vio/status", self.statuses.append, 10)
        self.node.create_subscription(
            DiagnosticArray, "/diagnostics", self.diagnostics.append, 10)
        self.node.create_subscription(
            Odometry, "/vio/odometry", self.odometry.append, 10)
        self.imu_publisher = self.node.create_publisher(
            Imu, "/imu/data_raw", qos_profile_sensor_data)
        self.info_publisher = self.node.create_publisher(
            CameraInfo, "/camera_info", qos_profile_sensor_data)
        self.image_publisher = self.node.create_publisher(
            Image, "/image_raw", qos_profile_sensor_data)

    def tearDown(self):
        self.node.destroy_node()

    def _change_state(self, service_name, transition_id, expected=True):
        client = self.node.create_client(ChangeState, service_name)
        self.assertTrue(client.wait_for_service(timeout_sec=10.0))
        request = ChangeState.Request()
        request.transition.id = transition_id
        future = client.call_async(request)
        rclpy.spin_until_future_complete(self.node, future, timeout_sec=10.0)
        self.assertIsNotNone(future.result())
        self.assertEqual(future.result().success, expected)

    def _wait_for_state(self, state, timeout=5.0):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            rclpy.spin_once(self.node, timeout_sec=0.05)
            for status in reversed(self.statuses):
                values = {item.key: item.value for item in status.values}
                if values.get("state") == state:
                    return status
        self.fail(f"VIO state {state} was not received; statuses={self.statuses!r}")

    def _publish_mismatched_inputs(self):
        stamp = self.node.get_clock().now().to_msg()
        imu = Imu()
        imu.header.stamp = stamp
        imu.header.frame_id = "imu_link"
        imu.angular_velocity.x = 0.1
        imu.linear_acceleration.z = 9.8

        info = CameraInfo()
        info.header.stamp = stamp
        info.header.frame_id = "camera_optical_frame"
        info.width = 1280
        info.height = 800
        info.k[0] = 500.0
        info.k[4] = 500.0
        info.d = [0.01]

        image = Image()
        image.header.stamp = stamp
        image.header.frame_id = "camera_optical_frame"
        image.width = 640
        image.height = 800
        image.encoding = "mono8"
        image.step = 640
        image.data = [0] * 640

        for _ in range(5):
            self.imu_publisher.publish(imu)
            self.info_publisher.publish(info)
            self.image_publisher.publish(image)
            rclpy.spin_once(self.node, timeout_sec=0.05)

    def test_lifecycle_input_rejection_timeout_and_cleanup(self):
        service = "/basalt_vio_node/change_state"
        self._change_state(service, Transition.TRANSITION_CONFIGURE)
        self._change_state(service, Transition.TRANSITION_ACTIVATE)
        initial = self._wait_for_state("INITIALIZING")
        self.assertEqual(initial.level, DiagnosticStatus.OK)

        self._publish_mismatched_inputs()
        degraded = self._wait_for_state("DEGRADED")
        self.assertEqual(degraded.level, DiagnosticStatus.WARN)
        self.assertTrue(self.diagnostics)

        lost = self._wait_for_state("LOST")
        self.assertEqual(lost.message, "sensor input timeout")
        self.assertEqual(self.odometry, [])

        self._change_state(service, Transition.TRANSITION_DEACTIVATE)
        self._change_state(service, Transition.TRANSITION_CLEANUP)
        self._change_state(service, Transition.TRANSITION_CONFIGURE)
        self._change_state(service, Transition.TRANSITION_ACTIVATE)
        self._wait_for_state("INITIALIZING")

    def test_unapproved_calibration_rejects_configuration(self):
        self._change_state(
            "/invalid/unapproved_vio_node/change_state",
            Transition.TRANSITION_CONFIGURE,
            expected=False,
        )


@launch_testing.post_shutdown_test()
class TestProcessesExitCleanly(unittest.TestCase):

    def test_exit_codes(self, proc_info):
        launch_testing.asserts.assertExitCodes(proc_info)
