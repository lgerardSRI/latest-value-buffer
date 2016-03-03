#INTERFACE requires 3+ and CXX_STANDARD requires 3.1+
cmake_minimum_required(VERSION 3.1)

project(latest-value-buffer)

set(L1CACHE_LINE_SIZE 64 CACHE STRING "The L1 cache line size of the target
platform")

# Require C++11 and C11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)


# The library is just a header
add_library(liblvb INTERFACE)
target_include_directories(liblvb INTERFACE include)
target_compile_definitions(liblvb INTERFACE
        L1CACHE_LINE_SIZE=${L1CACHE_LINE_SIZE})


# Tests
enable_testing()
find_package(GTest REQUIRED)

add_executable(test1 test/test1.cpp)
target_include_directories(test1 PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test1 ${GTEST_BOTH_LIBRARIES} liblvb)
GTEST_ADD_TESTS(test1 "" test/test1.cpp)


# Benchmarks
add_executable(benchmark test/benchmark.cpp test/benchmark.cpp)
target_link_libraries(benchmark liblvb)