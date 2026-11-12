#include <doctest/doctest.h>

#include "framerate_provider.hpp"
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <memory>

namespace fs = std::filesystem;

namespace {

class TempDirectory {
public:
    TempDirectory() {
        char pattern[] = "/tmp/hwmon-fps-test-XXXXXX";
        char* created = mkdtemp(pattern);
        if (created) path_ = created;
    }

    ~TempDirectory() {
        if (!path_.empty()) fs::remove_all(path_);
    }

    const fs::path& path() const { return path_; }

private:
    fs::path path_;
};

// Polls instead of sleeping a fixed time, so a slow machine does not miss an update.
bool waitForFps(const hwmon::FramerateProvider& provider, uint8_t expected) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (provider.getFps() != expected) {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return true;
}

void writeFile(const fs::path& path, const std::string& contents) {
    std::ofstream f(path);
    f << contents;
}

} // namespace

TEST_SUITE("Framerate Provider") {

    TEST_CASE("returns 0 when file does not exist") {
        TempDirectory temp;
        REQUIRE_FALSE(temp.path().empty());
        hwmon::FramerateProvider provider((temp.path() / "missing" / "fps").string());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        CHECK(provider.getFps() == 0);
    }

    TEST_CASE("reads valid fps value from file") {
        TempDirectory temp;
        REQUIRE_FALSE(temp.path().empty());
        fs::path tmp_dir = temp.path();
        fs::create_directories(tmp_dir);
        fs::path fps_file = tmp_dir / "current_fps";

        {
            std::ofstream f(fps_file);
            f << "144";
        }

        hwmon::FramerateProvider provider(fps_file.string());
        std::this_thread::sleep_for(std::chrono::milliseconds(600));  // watcher polls ~500ms

        CHECK(provider.getFps() == 144);

    }

    TEST_CASE("clamps out-of-range values") {
        TempDirectory temp;
        REQUIRE_FALSE(temp.path().empty());
        fs::path tmp_dir = temp.path();
        fs::create_directories(tmp_dir);
        fs::path fps_file = tmp_dir / "current_fps";

        {
            std::ofstream f(fps_file);
            f << "300";
        }

        hwmon::FramerateProvider provider(fps_file.string());
        std::this_thread::sleep_for(std::chrono::milliseconds(600));

        CHECK(provider.getFps() == 255);

    }

    TEST_CASE("clamps negative values to zero and accepts zero as a value") {
        TempDirectory temp;
        REQUIRE_FALSE(temp.path().empty());
        const fs::path fps_file = temp.path() / "current_fps";
        writeFile(fps_file, "60");
        hwmon::FramerateProvider provider(fps_file.string());
        REQUIRE(waitForFps(provider, 60));

        writeFile(fps_file, "-12\n");
        CHECK(waitForFps(provider, 0));

        writeFile(fps_file, "75");
        REQUIRE(waitForFps(provider, 75));
        writeFile(fps_file, "0");
        CHECK(waitForFps(provider, 0));
    }

    TEST_CASE("rejects trailing text instead of accepting a numeric prefix") {
        TempDirectory temp;
        REQUIRE_FALSE(temp.path().empty());
        fs::path tmp_dir = temp.path();
        fs::create_directories(tmp_dir);
        fs::path fps_file = tmp_dir / "current_fps";
        {
            std::ofstream f(fps_file);
            f << "144fps";
        }

        hwmon::FramerateProvider provider(fps_file.string());
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
        CHECK(provider.getFps() == 0);
    }

    TEST_CASE("empty and whitespace-only contents use the zero fallback") {
        TempDirectory temp;
        REQUIRE_FALSE(temp.path().empty());
        const fs::path fps_file = temp.path() / "current_fps";
        writeFile(fps_file, "90");
        hwmon::FramerateProvider provider(fps_file.string());
        REQUIRE(waitForFps(provider, 90));
        for (const std::string& content : {std::string{}, std::string(" \n\t ")}) {
            writeFile(fps_file, "90");
            REQUIRE(waitForFps(provider, 90));
            writeFile(fps_file, content);
            CHECK(waitForFps(provider, 0));
        }
    }

    TEST_CASE("invalid or removed file resets a previously valid value to zero") {
        TempDirectory temp;
        REQUIRE_FALSE(temp.path().empty());
        const fs::path fps_file = temp.path() / "current_fps";
        writeFile(fps_file, "144");
        hwmon::FramerateProvider provider(fps_file.string());
        REQUIRE(waitForFps(provider, 144));

        writeFile(fps_file, "bad");
        CHECK(waitForFps(provider, 0));

        writeFile(fps_file, "60");
        REQUIRE(waitForFps(provider, 60));
        fs::remove(fps_file);
        CHECK(waitForFps(provider, 0));
    }

    TEST_CASE("destruction is safe while the watched file disappears") {
        TempDirectory temp;
        REQUIRE_FALSE(temp.path().empty());
        const fs::path fps_file = temp.path() / "current_fps";
        {
            std::ofstream f(fps_file);
            f << "30";
        }
        {
            hwmon::FramerateProvider provider(fps_file.string());
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            fs::remove(fps_file);
        }
        CHECK_FALSE(fs::exists(fps_file));
    }

    TEST_CASE("manual setFps works") {
        TempDirectory temp;
        REQUIRE_FALSE(temp.path().empty());
        hwmon::FramerateProvider provider((temp.path() / "missing" / "fps").string());
        provider.setFps(87);
        CHECK(provider.getFps() == 87);

        // The missing file must not overwrite the manual value on later polls.
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
        CHECK(provider.getFps() == 87);
    }

    TEST_CASE("destructor wakes the polling thread promptly") {
        TempDirectory temp;
        REQUIRE_FALSE(temp.path().empty());
        const fs::path fps_file = temp.path() / "current_fps";
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
    }
}
