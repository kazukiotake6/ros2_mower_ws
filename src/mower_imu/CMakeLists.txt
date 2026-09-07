cmake_minimum_required(VERSION 3.16)
project(mower_imu LANGUAGES C CXX)

find_package(ament_cmake REQUIRED)
find_package(ament_cmake_gtest REQUIRED)
find_package(diagnostic_msgs REQUIRED)
find_package(rclcpp REQUIRED)

add_library(bmi270_sensor_api third_party/bmi270/bmi2.c third_party/bmi270/bmi270.c)
target_include_directories(bmi270_sensor_api PUBLIC third_party/bmi270)

add_library(bmi270_driver src/bmi270_driver.cpp src/linux_spi_transport.cpp)
target_include_directories(bmi270_driver PUBLIC include PRIVATE third_party/bmi270)
target_link_libraries(bmi270_driver PUBLIC bmi270_sensor_api)
target_compile_features(bmi270_driver PUBLIC cxx_std_17)
target_compile_options(bmi270_driver PRIVATE -Wall -Wextra -Wpedantic)

add_executable(bmi270_node src/bmi270_node.cpp)
target_link_libraries(bmi270_node bmi270_driver)
ament_target_dependencies(bmi270_node diagnostic_msgs rclcpp)
target_compile_features(bmi270_node PUBLIC cxx_std_17)
target_compile_options(bmi270_node PRIVATE -Wall -Wextra -Wpedantic)

install(TARGETS bmi270_sensor_api bmi270_driver bmi270_node
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY include/ DESTINATION include)

if(BUILD_TESTING)
  ament_add_gtest(test_bmi270_driver test/test_bmi270_driver.cpp)
  target_link_libraries(test_bmi270_driver bmi270_driver)
  target_include_directories(test_bmi270_driver PRIVATE third_party/bmi270)
endif()

ament_package()
