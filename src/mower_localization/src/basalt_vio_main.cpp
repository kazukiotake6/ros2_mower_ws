// Copyright 2026 Mower maintainers
// SPDX-License-Identifier: Apache-2.0

#include <memory>

#include "rclcpp/rclcpp.hpp"
#include "mower_localization/basalt_vio_node.hpp"

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  const auto node = std::make_shared<mower_localization::BasaltVioNode>();
  rclcpp::spin(node->get_node_base_interface());
  rclcpp::shutdown();
  return 0;
}
