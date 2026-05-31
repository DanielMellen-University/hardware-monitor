cmake_minimum_required(VERSION 3.16)
project(hardware-monitor-daemon
    VERSION 0.1.0
    DESCRIPTION "Hardware monitor daemon that streams system stats over serial"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable useful warnings
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror=return-type)
endif()

# Main executable
add_executable(hwmon-daemon
    src/main.cpp
    src/protocol.cpp
    src/cpu_monitor.cpp
    src/thermal_monitor.cpp
    src/gpu_monitor.cpp
    src/serial_port.cpp
    src/framerate_provider.cpp
)

target_include_directories(hwmon-daemon
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link pthread for threads (used by sleep/chrono in some paths)
find_package(Threads REQUIRED)
target_link_libraries(hwmon-daemon
    PRIVATE
        Threads::Threads
        ${CMAKE_DL_LIBS}   # for dlopen (NVML)
)

# Installation
include(GNUInstallDirs)
install(TARGETS hwmon-daemon
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install systemd unit (optional, user can copy)
install(FILES systemd/hardware-monitor.service
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system
    OPTIONAL
)

# Print summary
message(STATUS "hardware-monitor-daemon configured")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")

# -----------------------------
# Testing (doctest via FetchContent)
# -----------------------------
option(HWMON_BUILD_TESTS "Build unit and integration tests" ON)

if(HWMON_BUILD_TESTS)
    enable_testing()

    include(FetchContent)
    FetchContent_Declare(
        doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG        v2.4.11
    )
    FetchContent_MakeAvailable(doctest)

    add_subdirectory(tests)
endif()
