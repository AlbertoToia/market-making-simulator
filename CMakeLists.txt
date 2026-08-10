cmake_minimum_required(VERSION 3.20)
project(MarketMakingSimulator VERSION 1.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra -Wpedantic)

add_executable(simulator src/main.cpp)
target_include_directories(simulator PRIVATE src)

add_executable(tests tests/test_main.cpp)
target_include_directories(tests PRIVATE src)
