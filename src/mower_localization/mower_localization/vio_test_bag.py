# Copyright 2026 Mower maintainers
# SPDX-License-Identifier: Apache-2.0

"""Deterministic synthetic rosbag2 generation for VIO input tests."""

from dataclasses import dataclass
import json
from pathlib import Path
import random
from typing import Any

import rosbag2_py
from rclpy.serialization import serialize_message
from sensor_msgs.msg import CameraInfo, Image, Imu
import yaml


TOPICS = {
    "/camera_info": "sensor_msgs/msg/CameraInfo",
    "/image_raw": "sensor_msgs/msg/Image",
    "/imu/data_raw": "sensor_msgs/msg/Imu",
}


@dataclass(frozen=True)
class Scenario:
    name: str
    fault: str
    seed: int
    frame_count: int
    camera_period_ns: int
    imu_period_ns: int
    width: int
    height: int
    fault_index: int


def load_scenario(catalog_path: str | Path, scenario_name: str) -> Scenario:
    with Path(catalog_path).open(encoding="utf-8") as stream:
        catalog = yaml.safe_load(stream)
    if catalog.get("version") != 1:
        raise ValueError("unsupported VIO test scenario catalog version")
    defaults = catalog["defaults"]
    selected = next(
        (item for item in catalog["scenarios"] if item["name"] == scenario_name),
        None,
    )
    if selected is None:
        raise ValueError(f"unknown VIO test scenario: {scenario_name}")
    values = {**defaults, **selected}
    scenario = Scenario(
        name=values["name"],
        fault=values["fault"],
        seed=int(values["seed"]),
        frame_count=int(values["frame_count"]),
        camera_period_ns=int(values["camera_period_ns"]),
        imu_period_ns=int(values["imu_period_ns"]),
        width=int(values["width"]),
        height=int(values["height"]),
        fault_index=int(values.get("fault_index", -1)),
    )
    if min(
        scenario.frame_count,
        scenario.camera_period_ns,
        scenario.imu_period_ns,
        scenario.width,
        scenario.height,
    ) <= 0:
        raise ValueError("scenario dimensions, counts, and periods must be positive")
    return scenario


def _stamp(message: Any, nanoseconds: int) -> None:
    message.header.stamp.sec = nanoseconds // 1_000_000_000
    message.header.stamp.nanosec = nanoseconds % 1_000_000_000


def _camera_info(scenario: Scenario, index: int, stamp_ns: int) -> CameraInfo:
    message = CameraInfo()
    _stamp(message, stamp_ns)
    message.header.frame_id = "camera_optical_frame"
    message.width = scenario.width
    message.height = scenario.height
    if scenario.fault == "camera_info_mismatch" and index == scenario.fault_index:
        message.width = scenario.width // 2
    message.k[0] = 500.0
    message.k[4] = 500.0
    message.k[8] = 1.0
    message.d = [0.01]
    return message


def _image(
    scenario: Scenario, index: int, stamp_ns: int, random_source: random.Random
) -> Image:
    message = Image()
    if scenario.fault == "image_time_jump" and index == scenario.fault_index:
        stamp_ns -= 2 * scenario.camera_period_ns
    _stamp(message, stamp_ns)
    message.header.frame_id = "camera_optical_frame"
    message.width = scenario.width
    message.height = scenario.height
    message.encoding = "mono8"
    message.step = scenario.width
    message.data = [
        random_source.randrange(0, 256)
        for _ in range(scenario.width * scenario.height)
    ]
    return message


def _imu(stamp_ns: int) -> Imu:
    message = Imu()
    _stamp(message, stamp_ns)
    message.header.frame_id = "imu_link"
    message.angular_velocity.x = 0.1
    message.linear_acceleration.z = 9.80665
    return message


def _events(scenario: Scenario) -> list[tuple[int, int, str, Any]]:
    base_ns = 1_000_000_000
    random_source = random.Random(scenario.seed)
    events: list[tuple[int, int, str, Any]] = []
    camera_limit = scenario.frame_count
    if scenario.fault == "camera_stop":
        camera_limit = scenario.fault_index
    for index in range(camera_limit):
        record_ns = base_ns + index * scenario.camera_period_ns
        events.append(
            (record_ns, 0, "/camera_info", _camera_info(scenario, index, record_ns))
        )
        events.append(
            (
                record_ns,
                1,
                "/image_raw",
                _image(scenario, index, record_ns, random_source),
            )
        )

    duration_ns = (scenario.frame_count - 1) * scenario.camera_period_ns
    imu_count = duration_ns // scenario.imu_period_ns + 1
    gap_start = scenario.fault_index
    gap_end = gap_start + 5
    for index in range(imu_count):
        if scenario.fault == "imu_gap" and gap_start <= index < gap_end:
            continue
        record_ns = base_ns + index * scenario.imu_period_ns
        header_ns = record_ns
        if scenario.fault == "imu_duplicate" and index == scenario.fault_index:
            header_ns -= scenario.imu_period_ns
        events.append((record_ns, 2, "/imu/data_raw", _imu(header_ns)))
    return sorted(events, key=lambda item: (item[0], item[1]))


def generate_bag(
    output_uri: str | Path, catalog_path: str | Path, scenario_name: str
) -> dict[str, Any]:
    output_path = Path(output_uri)
    if output_path.exists():
        raise FileExistsError(f"output bag already exists: {output_path}")
    scenario = load_scenario(catalog_path, scenario_name)

    writer = rosbag2_py.SequentialWriter()
    writer.open(
        rosbag2_py.StorageOptions(uri=str(output_path), storage_id="sqlite3"),
        rosbag2_py.ConverterOptions(
            input_serialization_format="cdr",
            output_serialization_format="cdr",
        ),
    )
    for topic, message_type in TOPICS.items():
        writer.create_topic(
            rosbag2_py.TopicMetadata(
                id=0,
                name=topic,
                type=message_type,
                serialization_format="cdr",
            )
        )

    counts = {topic: 0 for topic in TOPICS}
    for record_ns, _, topic, message in _events(scenario):
        writer.write(topic, serialize_message(message), record_ns)
        counts[topic] += 1
    del writer

    manifest = {
        "catalog_version": 1,
        "scenario": scenario.name,
        "fault": scenario.fault,
        "seed": scenario.seed,
        "counts": counts,
        "expected_states": _expected_states(scenario.fault),
    }
    with (output_path / "vio_test_manifest.json").open("w", encoding="utf-8") as stream:
        json.dump(manifest, stream, indent=2, sort_keys=True)
        stream.write("\n")
    return manifest


def _expected_states(fault: str) -> list[str]:
    if fault in {
        "camera_info_mismatch",
        "image_time_jump",
        "imu_gap",
        "imu_duplicate",
    }:
        return ["DEGRADED"]
    if fault == "camera_stop":
        return ["LOST"]
    return ["INITIALIZING"]
