#!/usr/bin/env python3
# Copyright 2026 Mower maintainers
# SPDX-License-Identifier: Apache-2.0
"""Collect chessboard observations and write CameraInfo plus a calibration record."""

import argparse
import math
import pathlib
import sys
import time

import cv2
import numpy as np
import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy
from sensor_msgs.msg import Image


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image-topic", default="/image_raw")
    parser.add_argument("--size", default="8x6", help="interior corners, e.g. 8x6")
    parser.add_argument("--square", type=float, default=0.030, help="square side in metres")
    parser.add_argument("--samples", type=int, default=30)
    parser.add_argument("--min-interval", type=float, default=0.5)
    parser.add_argument("--timeout", type=float, default=300.0)
    parser.add_argument("--camera-name", default="ov9281_cam0")
    parser.add_argument("--camera-id", required=True, help="camera serial number or asset ID")
    parser.add_argument("--exposure-time-us", required=True, type=int)
    parser.add_argument("--analogue-gain", required=True, type=float)
    parser.add_argument("--rms-threshold-px", required=True, type=float)
    parser.add_argument("--software-revision", required=True, help="Git commit used for the run")
    parser.add_argument("--output", required=True)
    parser.add_argument("--record", required=True, help="Markdown test-record output path")
    args = parser.parse_args()
    try:
        columns, rows = (int(value) for value in args.size.lower().split("x", 1))
    except ValueError as error:
        parser.error("--size must be written as columnsxrows, for example 8x6")
        raise error
    if columns < 2 or rows < 2 or args.square <= 0.0 or args.samples < 10:
        parser.error("size must be at least 2x2, square positive, and samples at least 10")
    if args.exposure_time_us <= 0 or args.analogue_gain <= 0.0 or args.rms_threshold_px <= 0.0:
        parser.error("exposure, gain, and RMS threshold must be positive fixed values")
    args.pattern_size = (columns, rows)
    return args


def yaml_matrix(name, rows, columns, values):
    formatted = ", ".join(f"{float(value):.12g}" for value in values)
    return f"{name}:\n  rows: {rows}\n  cols: {columns}\n  data: [{formatted}]\n"


def write_camera_info(path, camera_name, image_size, camera_matrix, distortion):
    width, height = image_size
    projection = np.zeros((3, 4), dtype=np.float64)
    projection[:, :3] = camera_matrix
    contents = (
        f"image_width: {width}\n"
        f"image_height: {height}\n"
        f"camera_name: {camera_name}\n" +
        yaml_matrix("camera_matrix", 3, 3, camera_matrix.ravel()) +
        "distortion_model: plumb_bob\n" +
        yaml_matrix("distortion_coefficients", 1, len(distortion), distortion.ravel()) +
        yaml_matrix("rectification_matrix", 3, 3, np.eye(3).ravel()) +
        yaml_matrix("projection_matrix", 3, 4, projection.ravel()))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(contents, encoding="utf-8")


def write_record(path, args, image_size, rms, errors):
    """Write an auditable measured result, rather than a record template."""
    width, height = image_size
    verdict = "PASS" if rms <= args.rms_threshold_px else "FAIL"
    contents = f"""# OV9281 固定露光・固定ゲイン較正記録

| 項目 | 記録値 |
| --- | --- |
| カメラ個体ID | {args.camera_id} |
| ソフトウェア revision | {args.software_revision} |
| 画像 | {width}x{height}、`{args.image_topic}` |
| チェッカーボード | 内側コーナー{args.pattern_size[0]}x{args.pattern_size[1]}、square {args.square:.6f} m |
| 採用画像数 | {len(errors)} |
| 露光時間 | {args.exposure_time_us} us（固定） |
| アナログゲイン | {args.analogue_gain:g}（固定） |
| 再投影 RMS | {rms:.6f} px |
| RMS 閾値 | {args.rms_threshold_px:.6f} px |
| 判定 | **{verdict}** |
| 平均画像誤差 | {np.mean(errors):.6f} px |
| 最大画像誤差 | {np.max(errors):.6f} px |

この記録は単眼内部パラメータの受入れ結果であり、Camera-IMU外部パラメータ、時刻同期、
直線性および実走VIOの受入れを代替しない。
"""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(contents, encoding="utf-8")


class HeadlessCalibrator(Node):
    def __init__(self, args):
        super().__init__("headless_camera_calibration")
        self.args = args
        self.object_points = []
        self.image_points = []
        self.descriptors = []
        self.image_size = None
        self.last_sample_at = 0.0
        self.started_at = time.monotonic()
        self.last_detection_at = 0.0
        self.finished = False
        object_template = np.zeros((args.pattern_size[0] * args.pattern_size[1], 3), np.float32)
        object_template[:, :2] = np.mgrid[0:args.pattern_size[0], 0:args.pattern_size[1]].T.reshape(-1, 2)
        self.object_template = object_template * args.square
        qos = QoSProfile(depth=10, reliability=ReliabilityPolicy.BEST_EFFORT)
        self.subscription = self.create_subscription(Image, args.image_topic, self.on_image, qos)
        self.create_timer(1.0, self.on_timer)
        self.get_logger().info(
            f"waiting for {args.samples} chessboard samples on {args.image_topic}; "
            f"pattern={args.pattern_size[0]}x{args.pattern_size[1]}, square={args.square:.6f} m")

    @staticmethod
    def grayscale(message):
        if message.encoding.lower() in ("mono8", "8uc1"):
            return np.frombuffer(message.data, dtype=np.uint8).reshape(message.height, message.step)[:, :message.width]
        if message.encoding.lower() not in ("yuv422_yuy2", "yuyv", "yuyv8"):
            raise ValueError(f"unsupported image encoding: {message.encoding}")
        raw = np.frombuffer(message.data, dtype=np.uint8).reshape(message.height, message.step)
        return raw[:, :message.width * 2:2]

    def descriptor(self, corners, width, height):
        points = corners.reshape(-1, 2)
        centre = points.mean(axis=0)
        area = abs(cv2.contourArea(points.astype(np.float32))) / float(width * height)
        vector = points[-1] - points[0]
        angle = math.atan2(float(vector[1]), float(vector[0])) / math.pi
        return np.array([centre[0] / width, centre[1] / height, area, angle])

    def sufficiently_different(self, descriptor):
        if not self.descriptors:
            return True
        scales = np.array([0.15, 0.15, 0.04, 0.10])
        return min(np.linalg.norm((descriptor - item) / scales) for item in self.descriptors) >= 1.0

    def on_image(self, message):
        now = time.monotonic()
        if self.finished or now - self.last_detection_at < self.args.min_interval:
            return
        self.last_detection_at = now
        try:
            grayscale = self.grayscale(message)
        except ValueError as error:
            self.get_logger().error(str(error))
            self.finished = True
            return
        found, corners = cv2.findChessboardCorners(
            grayscale, self.args.pattern_size,
            cv2.CALIB_CB_ADAPTIVE_THRESH | cv2.CALIB_CB_NORMALIZE_IMAGE | cv2.CALIB_CB_FAST_CHECK)
        if found:
            corners = cv2.cornerSubPix(
                grayscale, corners, (11, 11), (-1, -1),
                (cv2.TERM_CRITERIA_EPS | cv2.TERM_CRITERIA_MAX_ITER, 30, 0.001))
        if not found:
            return
        descriptor = self.descriptor(corners, message.width, message.height)
        if not self.sufficiently_different(descriptor):
            return
        self.image_size = (message.width, message.height)
        self.object_points.append(self.object_template.copy())
        self.image_points.append(corners.astype(np.float32))
        self.descriptors.append(descriptor)
        self.last_sample_at = time.monotonic()
        self.get_logger().info(
            f"accepted sample {len(self.image_points)}/{self.args.samples}: "
            f"centre=({descriptor[0]:.2f},{descriptor[1]:.2f}), area={descriptor[2]:.3f}, angle={descriptor[3]:.2f}")
        if len(self.image_points) >= self.args.samples:
            self.calibrate()

    def on_timer(self):
        if not self.finished and time.monotonic() - self.started_at > self.args.timeout:
            self.get_logger().error(
                f"timed out with {len(self.image_points)}/{self.args.samples} accepted samples")
            self.finished = True

    def calibrate(self):
        self.finished = True
        rms, camera_matrix, distortion, rotations, translations = cv2.calibrateCamera(
            self.object_points, self.image_points, self.image_size, None, None)
        errors = []
        for object_points, image_points, rotation, translation in zip(
                self.object_points, self.image_points, rotations, translations):
            projected, _ = cv2.projectPoints(object_points, rotation, translation, camera_matrix, distortion)
            errors.append(float(cv2.norm(image_points, projected, cv2.NORM_L2) / len(projected)))
        output = pathlib.Path(self.args.output).expanduser().resolve()
        record = pathlib.Path(self.args.record).expanduser().resolve()
        write_camera_info(output, self.args.camera_name, self.image_size, camera_matrix, distortion)
        write_record(record, self.args, self.image_size, rms, errors)
        self.get_logger().info(
            f"wrote {output} and {record}; samples={len(self.image_points)}, rms={rms:.4f}px, "
            f"mean={np.mean(errors):.4f}px, max={np.max(errors):.4f}px")


def main():
    args = arguments()
    rclpy.init(args=None)
    node = HeadlessCalibrator(args)
    try:
        while rclpy.ok() and not node.finished:
            rclpy.spin_once(node, timeout_sec=0.2)
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
