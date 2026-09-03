#!/usr/bin/env python3
# Copyright 2026 Mower maintainers
# SPDX-License-Identifier: Apache-2.0

import argparse

from mower_localization.vio_test_bag import generate_bag


def main():
    parser = argparse.ArgumentParser(description="Generate a deterministic VIO rosbag2")
    parser.add_argument("--catalog", required=True)
    parser.add_argument("--scenario", required=True)
    parser.add_argument("--output", required=True)
    arguments = parser.parse_args()
    generate_bag(arguments.output, arguments.catalog, arguments.scenario)


if __name__ == "__main__":
    main()
