#pragma once

#include <cstdint>
#include <string>
#include <atomic>
#include <thread>
#include <optional>
#include <filesystem>

namespace hwmon {

// Provides current FPS value.
// Primary mechanism: watches a plain text file containing a single integer (0-255).
// External tools (game launch scripts, MangoHud post-processing, etc.) can write to this file.
class FramerateProvider {
public:
    explicit FramerateProvider(const std::string& file_path = "/run/hwmon-daemon/current_fps");
    ~FramerateProvider();

    uint8_t getFps() const;

    // Allows manually setting FPS (useful for testing or when file mechanism not used).
    void setFps(uint8_t fps);

    const std::string& getWatchPath() const { return watch_path_; }

private:
    void watchLoop();
    uint8_t readFromFile() const;

    std::string watch_path_;
    std::atomic<uint8_t> current_fps_{0};
    std::thread watcher_thread_;
    std::atomic<bool> running_{true};
    std::filesystem::file_time_type last_write_time_{};
};

} // namespace hwmon
