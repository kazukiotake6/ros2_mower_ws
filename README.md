# ROS 2 mower workspace

ROS 2 Jazzy development workspace for the autonomous mower described in
[`develop_plan.md`](develop_plan.md). The workspace runs on both arm64 (Raspberry
Pi 5) and x86_64 development machines; hardware interfaces are deliberately
kept behind ROS 2 packages.

## Quick start

The recommended setup is the included Dev Container (Docker must be available):

1. Open this repository in VS Code and choose **Reopen in Container**.
2. The container resolves ROS dependencies and builds the workspace automatically.
3. In a new terminal, source `install/setup.bash` before using ROS commands.

For a native Ubuntu 24.04 + ROS 2 Jazzy installation:

```bash
source /opt/ros/jazzy/setup.bash
rosdep update
rosdep install --from-paths src --ignore-src -r -y
colcon build --symlink-install
source install/setup.bash
colcon test
colcon test-result --verbose
```

## Package boundaries

| Package | Responsibility |
| --- | --- |
| `mower_description` | Robot model, frames, and sensor placement |
| `mower_bringup` | Launch/configuration and lifecycle orchestration |
| `mower_camera` / `mower_imu` | CSI camera and BMI270 interfaces |
| `mower_can` | SocketCAN-to-ROS gateway and CAN protocol validation |
| `mower_control` | Velocity commands, watchdog behavior, and wheel control |
| `mower_localization` | VIO, wheel odometry fusion, and localization |
| `mower_navigation` | Coverage planning and navigation |
| `mower_simulation` | Gazebo/vcan/HIL simulation support |

Hardware drivers, CAN IDs, and safety-stop implementation are intentionally not
provided by this scaffold: they require the reviewed electrical and MCU protocol
specification described in the plan. Do not use this repository to bypass an
MCU-controlled safety stop or an independent E-stop.

## Quality checks

`colcon build`, `colcon test`, and `colcon test-result --verbose` are the local
quality gate and are also run in GitHub Actions. Build outputs and rosbag2 data
are ignored by Git.
