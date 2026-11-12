#include <doctest/doctest.h>

#include "framerate_provider.hpp"
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <memory>

namespace fs = std::filesystem;

TEST_SUITE("Framerate Provider") {

    TEST_CASE("returns 0 when file does not exist") {
        // Use a path that almost certainly doesn't exist
        hwmon::FramerateProvider provider("/tmp/this_file_should_not_exist_987654321/fps");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        CHECK(provider.getFps() == 0);
    }

    TEST_CASE("reads valid fps value from file") {
        fs::path tmp_dir = fs::temp_directory_path() / "hwmon_fps_test";
        fs::create_directories(tmp_dir);
        fs::path fps_file = tmp_dir / "current_fps";

        {
            std::ofstream f(fps_file);
            f << "144";
        }

        hwmon::FramerateProvider provider(fps_file.string());
        std::this_thread::sleep_for(std::chrono::milliseconds(600));  // watcher polls ~500ms

        CHECK(provider.getFps() == 144);

        fs::remove_all(tmp_dir);
    }

    TEST_CASE("clamps out-of-range values") {
        fs::path tmp_dir = fs::temp_directory_path() / "hwmon_fps_test2";
        fs::create_directories(tmp_dir);
        fs::path fps_file = tmp_dir / "current_fps";

        {
            std::ofstream f(fps_file);
            f << "300";
        }

        hwmon::FramerateProvider provider(fps_file.string());
        std::this_thread::sleep_for(std::chrono::milliseconds(600));

        CHECK(provider.getFps() == 255);

        fs::remove_all(tmp_dir);
    }

    TEST_CASE("rejects trailing text instead of accepting a numeric prefix") {
        fs::path tmp_dir = fs::temp_directory_path() / "hwmon_fps_invalid_test";
        fs::create_directories(tmp_dir);
        fs::path fps_file = tmp_dir / "current_fps";
        {
            std::ofstream f(fps_file);
            f << "144fps";
        }

        {
            hwmon::FramerateProvider provider(fps_file.string());
            std::this_thread::sleep_for(std::chrono::milliseconds(600));
            CHECK(provider.getFps() == 0);
        }
        fs::remove_all(tmp_dir);
    }

    TEST_CASE("manual setFps works") {
        hwmon::FramerateProvider provider("/tmp/nonexistent_hwmon_fps");
        provider.setFps(87);
        CHECK(provider.getFps() == 87);
    }

    TEST_CASE("destructor wakes the polling thread promptly") {
        const fs::path tmp_dir = fs::temp_directory_path() / "hwmon_shutdown_test";
        fs::create_directories(tmp_dir);
        const fs::path fps_file = tmp_dir / "current_fps";
        {
            std::ofstream f(fps_file);
            f << "42";
        }
        auto provider = std::make_unique<hwmon::FramerateProvider>(fps_file.string());

        // Wait until the watcher has completed a poll, then give it time to start its
        // 500 ms pause, so destruction has to interrupt an active wait.
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (provider->getFps() != 42 && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        REQUIRE(provider->getFps() == 42);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        const auto start = std::chrono::steady_clock::now();
        provider.reset();
        const auto elapsed = std::chrono::steady_clock::now() - start;
        CHECK(elapsed < std::chrono::milliseconds(250));

        fs::remove_all(tmp_dir);
    }
}
