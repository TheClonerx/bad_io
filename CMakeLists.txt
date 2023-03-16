cmake_minimum_required(VERSION 3.20)
project(event_loop)

set(WITH_URING_DEFAULT OFF)
set(WITH_EPOLL_DEFAULT OFF)
set(WITH_POLL_DEFAULT ON)
set(WITH_SELECT_DEFAULT ON)
set(WITH_IOCP_DEFAULT OFF)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set(WITH_EPOLL_DEFAULT ON)
    if (${CMAKE_SYSTEM_VERSION} VERSION_GREATER_EQUAL "3.10")
        set(WITH_URING_DEFAULT ON)
    endif()
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    set(WITH_IOCP_DEFAULT ON)
endif()

option(WITH_URING     "Enable io_uring"            ${WITH_URING_DEFAULT} )
option(WITH_EPOLL     "Enable epoll"               ${WITH_EPOLL_DEFAULT} )
option(WITH_POLL      "Enable poll"                ${WITH_POLL_DEFAULT}  )
option(WITH_SELECT    "Enable select"              ${WITH_SELECT_DEFAULT})
option(WITH_IOCP      "Enable IO Completion Ports" ${WITH_IOCP_DEFAULT}  )
option(ENABLE_TESTING "Enable tesing"              OFF)

if (WITH_URING AND NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    message(FATAL_ERROR "io_uring is only avaible on Linux")
endif()

if (WITH_EPOLL AND NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    message(FATAL_ERROR "epoll is only avaible on Linux")
endif()

if (WITH_IOCP AND NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    message(FATAL_ERROR "IO Completion Ports is only avaible on Windows")
endif()

if (WITH_URING)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(liburing REQUIRED IMPORTED_TARGET liburing)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)

add_library(${PROJECT_NAME})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)
target_sources(${PROJECT_NAME} PRIVATE src/execution_context.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if (WITH_URING)
    target_link_libraries(${PROJECT_NAME} PUBLIC PkgConfig::liburing)
    target_sources(${PROJECT_NAME} PRIVATE src/ioring_service.cpp)
endif()
if (WITH_EPOLL)
    target_sources(${PROJECT_NAME} PRIVATE src/epoll_service.cpp)
endif()


if (ENABLE_TESTING)
    add_executable(main)
    target_link_libraries(main PRIVATE ${PROJECT_NAME})
    target_sources(main PRIVATE src/main.cpp)
endif()
