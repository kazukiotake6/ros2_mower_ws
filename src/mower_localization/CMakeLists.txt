cmake_minimum_required(VERSION 3.16)
project(mower_localization LANGUAGES CXX)
find_package(ament_cmake REQUIRED)

add_library(vio_input_buffer src/vio_input_buffer.cpp)
target_include_directories(vio_input_buffer PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_compile_features(vio_input_buffer PUBLIC cxx_std_17)
target_compile_options(vio_input_buffer PRIVATE -Wall -Wextra -Wpedantic)

install(TARGETS vio_input_buffer
  EXPORT export_vio_input_buffer
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_vio_input_buffer test/test_vio_input_buffer.cpp)
  target_link_libraries(test_vio_input_buffer vio_input_buffer)
endif()

ament_export_targets(export_vio_input_buffer HAS_LIBRARY_TARGET)
ament_package()
