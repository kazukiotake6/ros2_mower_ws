cmake_minimum_required(VERSION 3.16)
project(mower_can)

find_package(ament_cmake REQUIRED)
find_package(diagnostic_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(std_msgs REQUIRED)

add_executable(can_gateway_node src/can_gateway_node.cpp)
target_include_directories(can_gateway_node PRIVATE include)
ament_target_dependencies(can_gateway_node
  diagnostic_msgs geometry_msgs rclcpp rclcpp_lifecycle std_msgs)

install(TARGETS can_gateway_node DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY config launch DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_can_protocol test/test_can_protocol.cpp)
  target_include_directories(test_can_protocol PRIVATE include)
endif()

ament_package()
