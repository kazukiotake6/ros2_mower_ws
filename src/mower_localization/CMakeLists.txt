cmake_minimum_required(VERSION 3.16)
project(mower_localization LANGUAGES CXX)
find_package(ament_cmake REQUIRED)
find_package(ament_cmake_python REQUIRED)
find_package(diagnostic_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(sensor_msgs REQUIRED)

add_library(vio_input_buffer src/vio_input_buffer.cpp src/vio_input_validator.cpp)
target_include_directories(vio_input_buffer PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_compile_features(vio_input_buffer PUBLIC cxx_std_17)
target_compile_options(vio_input_buffer PRIVATE -Wall -Wextra -Wpedantic)

add_library(basalt_vio_node_lib src/basalt_vio_node.cpp)
target_include_directories(basalt_vio_node_lib PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_link_libraries(basalt_vio_node_lib vio_input_buffer)
ament_target_dependencies(
  basalt_vio_node_lib
  diagnostic_msgs nav_msgs rclcpp rclcpp_lifecycle sensor_msgs)
target_compile_features(basalt_vio_node_lib PUBLIC cxx_std_17)
target_compile_options(basalt_vio_node_lib PRIVATE -Wall -Wextra -Wpedantic)

add_executable(basalt_vio_node src/basalt_vio_main.cpp)
target_link_libraries(basalt_vio_node basalt_vio_node_lib)
ament_target_dependencies(basalt_vio_node rclcpp)
target_compile_features(basalt_vio_node PUBLIC cxx_std_17)
target_compile_options(basalt_vio_node PRIVATE -Wall -Wextra -Wpedantic)

install(TARGETS vio_input_buffer basalt_vio_node_lib basalt_vio_node
  EXPORT export_mower_localization
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY launch config DESTINATION share/${PROJECT_NAME})
ament_python_install_package(${PROJECT_NAME})
install(PROGRAMS scripts/generate_vio_test_bag.py
  DESTINATION lib/${PROJECT_NAME}
  RENAME generate_vio_test_bag)

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  find_package(ament_cmake_pytest REQUIRED)
  ament_add_gtest(test_vio_input_buffer test/test_vio_input_buffer.cpp)
  target_link_libraries(test_vio_input_buffer vio_input_buffer)
  ament_add_gtest(test_vio_input_validator test/test_vio_input_validator.cpp)
  target_link_libraries(test_vio_input_validator vio_input_buffer)
  ament_add_gtest(test_vio_estimator_adapter test/test_vio_estimator_adapter.cpp)
  target_link_libraries(test_vio_estimator_adapter basalt_vio_node_lib)
  ament_target_dependencies(
    test_vio_estimator_adapter
    diagnostic_msgs nav_msgs rclcpp rclcpp_lifecycle sensor_msgs)
  find_package(launch_testing_ament_cmake REQUIRED)
  add_launch_test(test/test_basalt_vio_lifecycle.py TIMEOUT 30)
  add_launch_test(test/test_vio_rosbag_replay.py TIMEOUT 90)
  ament_add_pytest_test(test_vio_test_bag test/test_vio_test_bag.py TIMEOUT 30)
endif()

ament_export_targets(export_mower_localization HAS_LIBRARY_TARGET)
ament_export_dependencies(diagnostic_msgs nav_msgs rclcpp rclcpp_lifecycle sensor_msgs)
ament_package()
