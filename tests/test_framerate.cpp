#include <doctest/doctest.h>

#include "framerate_provider.hpp"
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>

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

    TEST_CASE("manual setFps works") {
        hwmon::FramerateProvider provider("/tmp/nonexistent_hwmon_fps");
        provider.setFps(87);
        CHECK(provider.getFps() == 87);
    }
}
