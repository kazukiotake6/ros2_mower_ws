cmake_minimum_required(VERSION 3.16)
project(mower_camera LANGUAGES CXX)

find_package(ament_cmake REQUIRED)
find_package(camera_info_manager REQUIRED)
find_package(diagnostic_msgs REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
pkg_check_modules(LIBCAMERA REQUIRED IMPORTED_TARGET libcamera)

add_library(camera_parameters src/camera_parameters.cpp)
target_include_directories(camera_parameters PUBLIC include)
ament_target_dependencies(camera_parameters sensor_msgs)
target_compile_features(camera_parameters PUBLIC cxx_std_17)
target_compile_options(camera_parameters PRIVATE -Wall -Wextra -Wpedantic)

add_executable(libcamera_node src/libcamera_node.cpp)
target_link_libraries(libcamera_node camera_parameters PkgConfig::LIBCAMERA)
target_include_directories(libcamera_node PRIVATE include)
ament_target_dependencies(libcamera_node camera_info_manager diagnostic_msgs rclcpp sensor_msgs)
target_compile_features(libcamera_node PUBLIC cxx_std_17)
target_compile_options(libcamera_node PRIVATE -Wall -Wextra -Wpedantic)

install(TARGETS camera_parameters libcamera_node
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION lib/${PROJECT_NAME})
install(PROGRAMS scripts/headless_camera_calibration.py
  DESTINATION lib/${PROJECT_NAME}
  RENAME headless_camera_calibration)
install(DIRECTORY launch config DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_camera_parameters test/test_camera_parameters.cpp)
  target_link_libraries(test_camera_parameters camera_parameters)
  target_include_directories(test_camera_parameters PRIVATE include)
  ament_target_dependencies(test_camera_parameters sensor_msgs)
endif()

ament_package()
