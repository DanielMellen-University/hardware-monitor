# Tests for hardware-monitor-daemon

# Main test executable
add_executable(hwmon_tests
    test_protocol.cpp
    test_cpu_calc.cpp
    test_framerate.cpp
    test_serial_port.cpp
    test_thermal_monitor.cpp
    test_config.cpp
    test_gpu_parser.cpp
    test_smoke.cpp
)

find_package(Threads REQUIRED)

target_link_libraries(hwmon_tests
    PRIVATE
        doctest::doctest
        Threads::Threads
)

if(UNIX AND NOT APPLE)
    find_library(UTIL_LIBRARY util)
    if(UTIL_LIBRARY)
        target_link_libraries(hwmon_tests PRIVATE ${UTIL_LIBRARY})
    endif()
endif()

target_include_directories(hwmon_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

# Make the sources we want to test available
target_sources(hwmon_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/protocol.cpp
        ${CMAKE_SOURCE_DIR}/src/cpu_monitor.cpp
        ${CMAKE_SOURCE_DIR}/src/thermal_monitor.cpp
        ${CMAKE_SOURCE_DIR}/src/framerate_provider.cpp
        ${CMAKE_SOURCE_DIR}/src/serial_port.cpp
        ${CMAKE_SOURCE_DIR}/src/config.cpp
        ${CMAKE_SOURCE_DIR}/src/gpu_parser.cpp
        test_smoke.cpp
)

# Enable doctest CTest integration
include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(hwmon_tests)

add_test(NAME daemon_help COMMAND $<TARGET_FILE:hwmon-daemon> --help)
set_tests_properties(daemon_help PROPERTIES PASS_REGULAR_EXPRESSION "Usage: .*\\[options\\]")

# CTest registers each doctest case; running hwmon_tests directly executes the suite once.
