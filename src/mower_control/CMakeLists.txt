cmake_minimum_required(VERSION 3.16)
project(mower_control)
find_package(ament_cmake REQUIRED)

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_command_limits test/test_command_limits.cpp)
endif()

ament_package()
