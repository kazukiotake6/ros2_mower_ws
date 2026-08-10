ARG ROS_DISTRO=jazzy
FROM ros:${ROS_DISTRO}-ros-base-noble

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

ARG ROS_DISTRO
ENV DEBIAN_FRONTEND=noninteractive \
    ROS_DISTRO=${ROS_DISTRO}

RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential \
      can-utils \
      git \
      python3-colcon-common-extensions \
      python3-rosdep \
      python3-vcstool \
      ros-${ROS_DISTRO}-ament-lint-auto \
      ros-${ROS_DISTRO}-ament-cmake-gtest \
    && rm -rf /var/lib/apt/lists/*

RUN rosdep init 2>/dev/null || true

WORKDIR /workspaces/ros2_mower_ws
COPY docker/entrypoint.sh /ros_entrypoint_mower.sh
RUN chmod +x /ros_entrypoint_mower.sh
ENTRYPOINT ["/ros_entrypoint_mower.sh"]
CMD ["bash"]
