#include <doctest/doctest.h>

#include "cpu_monitor.hpp"

using hwmon::CpuTimes;

TEST_SUITE("CPU Load Calculation") {

    TEST_CASE("idle system produces 0% load") {
        // Only idle + iowait increase. All busy counters stay exactly the same.
        CpuTimes prev{1200, 80, 350, 9500, 180, 15, 25, 8};
        CpuTimes curr{1200, 80, 350, 10200, 260, 15, 25, 8};

        uint8_t load = hwmon::calculate_cpu_load_percent(prev, curr);
        CHECK(load == 0);
    }

    TEST_CASE("fully busy system produces ~100% load") {
        CpuTimes prev{1000, 50, 300, 100, 10, 5, 5, 0};
        CpuTimes curr{2000, 100, 600, 100, 10, 5, 5, 0};  // almost no idle growth

        uint8_t load = hwmon::calculate_cpu_load_percent(prev, curr);
        CHECK(load >= 98);  // allow tiny rounding
        CHECK(load <= 100);
    }

    TEST_CASE("exactly 50% load") {
        CpuTimes prev{1000, 0, 0, 1000, 0, 0, 0, 0};
        CpuTimes curr{1500, 0, 0, 1500, 0, 0, 0, 0};

        uint8_t load = hwmon::calculate_cpu_load_percent(prev, curr);
        CHECK(load == 50);
    }

    TEST_CASE("handles zero total delta gracefully") {
        CpuTimes same{500, 10, 20, 1000, 5, 1, 2, 0};
        uint8_t load = hwmon::calculate_cpu_load_percent(same, same);
        CHECK(load == 0);
    }

    TEST_CASE("ignores steal time correctly in calculation") {
        CpuTimes prev{1000, 0, 200, 2000, 0, 0, 0, 50};
        CpuTimes curr{1100, 0, 250, 2050, 0, 0, 0, 150}; // steal increased a lot

        // steal counts as used time
        uint8_t load = hwmon::calculate_cpu_load_percent(prev, curr);
        CHECK(load > 0);
    }

    TEST_CASE("clamps output to 0-100 even with weird numbers") {
        CpuTimes prev{0,0,0,0,0,0,0,0};
        CpuTimes curr{100000, 0, 0, 0, 0, 0, 0, 0};

        uint8_t load = hwmon::calculate_cpu_load_percent(prev, curr);
        CHECK(load == 100);
    }

    TEST_CASE("typical mixed workload gives reasonable result") {
        CpuTimes prev{12000, 300, 4500, 35000, 800, 120, 90, 50};
        CpuTimes curr{12550, 305, 4570, 35550, 815, 122, 92, 52};

        uint8_t load = hwmon::calculate_cpu_load_percent(prev, curr);
        // Should be in a plausible low-to-mid range for this delta
        CHECK(load >= 15);
        CHECK(load <= 60);
    }
}
