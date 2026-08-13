cmake_minimum_required(VERSION 3.16)
project(mower_imu LANGUAGES C CXX)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)

add_library(bmi270_sensor_api third_party/bmi270/bmi2.c third_party/bmi270/bmi270.c)
target_include_directories(bmi270_sensor_api PUBLIC third_party/bmi270)

add_executable(bmi270_raw_node src/bmi270_raw_node.cpp)
target_include_directories(bmi270_raw_node PRIVATE third_party/bmi270)
target_link_libraries(bmi270_raw_node bmi270_sensor_api)
ament_target_dependencies(bmi270_raw_node rclcpp sensor_msgs)
target_compile_features(bmi270_raw_node PUBLIC cxx_std_17)
target_compile_options(bmi270_raw_node PRIVATE -Wall -Wextra -Wpedantic)

install(TARGETS bmi270_raw_node
  RUNTIME DESTINATION lib/${PROJECT_NAME})

ament_package()
