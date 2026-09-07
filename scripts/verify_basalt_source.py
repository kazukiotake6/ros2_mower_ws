#!/usr/bin/env python3
"""Verify that a Basalt checkout matches the reviewed source baseline."""

import argparse
import json
from pathlib import Path
import subprocess
import sys


def git(checkout, *args):
    return subprocess.run(
        ["git", "-C", str(checkout), *args],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()


def dependency_names(manifest):
    return {
        dependency if isinstance(dependency, str) else dependency["name"]
        for dependency in manifest["dependencies"]
    }


def verify(checkout, baseline):
    expected = json.loads(Path(baseline).read_text(encoding="utf-8"))
    checkout = Path(checkout)
    head = git(checkout, "rev-parse", "HEAD")
    if head != expected["commit"]:
        raise ValueError(
            f"Basalt commit mismatch: expected {expected['commit']}, got {head}"
        )

    submodule = git(checkout, "rev-parse", "HEAD:thirdparty/vcpkg")
    if submodule != expected["vcpkg_submodule_commit"]:
        raise ValueError("vcpkg submodule commit does not match the baseline")

    manifest = json.loads((checkout / "vcpkg.json").read_text(encoding="utf-8"))
    if manifest.get("version-string") != expected["tag"]:
        raise ValueError("vcpkg manifest version does not match the tag")
    missing = set(expected["required_manifest_dependencies"]) - dependency_names(manifest)
    if missing:
        raise ValueError(f"required dependencies are missing: {sorted(missing)}")

    configuration = json.loads(
        (checkout / "vcpkg-configuration.json").read_text(encoding="utf-8")
    )
    actual_baseline = configuration["default-registry"]["baseline"]
    if actual_baseline != expected["vcpkg_registry_baseline"]:
        raise ValueError("vcpkg registry baseline does not match the baseline")

    cmake = (checkout / "CMakeLists.txt").read_text(encoding="utf-8")
    if "find_package(Pangolin CONFIG REQUIRED)" not in cmake:
        raise ValueError("Pangolin dependency changed; reassess headless build")
    vio_source = (checkout / "src" / "vio.cpp").read_text(encoding="utf-8")
    if 'add_option("--show-gui"' not in vio_source:
        raise ValueError("basalt_vio no longer exposes the --show-gui option")
    if not (checkout / "LICENSE").is_file():
        raise ValueError("Basalt LICENSE is missing")

    return {
        "commit": head,
        "tag": expected["tag"],
        "vcpkg_submodule_commit": submodule,
        "vcpkg_registry_baseline": actual_baseline,
        "pangolin_required_at_configure": True,
        "runtime_gui_can_be_disabled": True,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--checkout", required=True)
    parser.add_argument("--baseline", required=True)
    arguments = parser.parse_args()
    try:
        result = verify(arguments.checkout, arguments.baseline)
    except (OSError, KeyError, ValueError, subprocess.CalledProcessError) as error:
        print(f"Basalt source verification failed: {error}", file=sys.stderr)
        return 1
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    sys.exit(main())
