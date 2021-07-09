cmake_minimum_required(VERSION 3.20)
project(event_loop)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_library(event_loop)
target_sources(event_loop PRIVATE src/dummy.cpp PUBLIC include/tcx/awaitable.hpp)
target_include_directories(event_loop PUBLIC include)


add_executable(main)
target_link_libraries(main PRIVATE event_loop)
target_sources(main PRIVATE src/main.cpp)