# Tests for hardware-monitor-daemon

# Main test executable
add_executable(hwmon_tests
    test_protocol.cpp
    test_cpu_calc.cpp
    test_framerate.cpp
    # test_smoke.cpp          # optional real-hardware smoke tests
)

find_package(Threads REQUIRED)

target_link_libraries(hwmon_tests
    PRIVATE
        doctest::doctest
        Threads::Threads
)

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
        test_smoke.cpp
)

# Enable doctest CTest integration
include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(hwmon_tests)

# Also allow running the binary directly
add_test(NAME hwmon_all_tests COMMAND hwmon_tests)
