
cmake_minimum_required(VERSION 2.8)

project(latest-value-buffer)

# Require C++11 and C11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)


# The library
add_library(latest_value_buffer STATIC src/implementation.cpp)
target_include_directories(latest_value_buffer PUBLIC include)


# Tests
enable_testing()
find_package(GTest REQUIRED)

add_executable(test1 test/test1.cpp)
target_include_directories(test1 PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test1 ${GTEST_BOTH_LIBRARIES} latest_value_buffer)



