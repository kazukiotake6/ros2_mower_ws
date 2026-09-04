# Copyright 2026 Mower maintainers
# SPDX-License-Identifier: Apache-2.0

import json
from pathlib import Path

import pytest
import rosbag2_py
from rclpy.serialization import deserialize_message
from rosidl_runtime_py.utilities import get_message

from mower_localization.vio_test_bag import generate_bag, load_scenario


CATALOG = (
    Path(__file__).parents[1] / "config" / "vio_test_scenarios.yaml"
)


def read_bag(uri):
    reader = rosbag2_py.SequentialReader()
    reader.open(
        rosbag2_py.StorageOptions(uri=str(uri), storage_id="sqlite3"),
        rosbag2_py.ConverterOptions(
            input_serialization_format="cdr",
            output_serialization_format="cdr",
        ),
    )
    topic_types = {
        item.name: get_message(item.type)
        for item in reader.get_all_topics_and_types()
    }
    records = []
    while reader.has_next():
        topic, serialized, record_ns = reader.read_next()
        records.append(
            (topic, record_ns, deserialize_message(serialized, topic_types[topic]))
        )
    return records


@pytest.mark.parametrize(
    "scenario_name",
    [
        "normal",
        "camera_info_mismatch",
        "imu_gap",
        "imu_duplicate",
        "image_time_jump",
        "camera_stop",
    ],
)
def test_generated_bag_has_manifest_and_expected_topics(tmp_path, scenario_name):
    output = tmp_path / scenario_name
    manifest = generate_bag(output, CATALOG, scenario_name)
    records = read_bag(output)

    assert records
    assert manifest["scenario"] == scenario_name
    assert sum(manifest["counts"].values()) == len(records)
    assert {record[0] for record in records} == {
        "/camera_info",
        "/image_raw",
        "/imu/data_raw",
    }
    assert (output / "metadata.yaml").is_file()
    with (output / "vio_test_manifest.json").open(encoding="utf-8") as stream:
        assert json.load(stream) == manifest


def test_same_seed_produces_identical_logical_stream(tmp_path):
    first = read_bag(generate_and_return_path(tmp_path / "first", "normal"))
    second = read_bag(generate_and_return_path(tmp_path / "second", "normal"))

    assert len(first) == len(second)
    for left, right in zip(first, second):
        assert left[0:2] == right[0:2]
        assert left[2] == right[2]


def generate_and_return_path(path, scenario):
    generate_bag(path, CATALOG, scenario)
    return path


def test_faults_are_encoded_in_header_stamps_and_shapes(tmp_path):
    mismatch_records = read_bag(
        generate_and_return_path(tmp_path / "mismatch", "camera_info_mismatch")
    )
    infos = [
        message for topic, _, message in mismatch_records
        if topic == "/camera_info"
    ]
    assert any(message.width == 8 for message in infos)

    jump_records = read_bag(
        generate_and_return_path(tmp_path / "jump", "image_time_jump")
    )
    image_stamps = [
        message.header.stamp.sec * 1_000_000_000 + message.header.stamp.nanosec
        for topic, _, message in jump_records if topic == "/image_raw"
    ]
    assert any(
        current <= previous
        for previous, current in zip(image_stamps, image_stamps[1:])
    )

    duplicate_records = read_bag(
        generate_and_return_path(tmp_path / "duplicate", "imu_duplicate")
    )
    imu_stamps = [
        message.header.stamp.sec * 1_000_000_000 + message.header.stamp.nanosec
        for topic, _, message in duplicate_records if topic == "/imu/data_raw"
    ]
    assert any(
        current == previous
        for previous, current in zip(imu_stamps, imu_stamps[1:])
    )


def test_invalid_catalog_and_existing_output_are_rejected(tmp_path):
    invalid_catalog = tmp_path / "invalid.yaml"
    invalid_catalog.write_text("version: 2\ndefaults: {}\nscenarios: []\n")
    with pytest.raises(ValueError, match="unsupported"):
        load_scenario(invalid_catalog, "normal")

    output = tmp_path / "bag"
    generate_bag(output, CATALOG, "normal")
    with pytest.raises(FileExistsError):
        generate_bag(output, CATALOG, "normal")
