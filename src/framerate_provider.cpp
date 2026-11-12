#include "framerate_provider.hpp"

#include <fstream>
#include <iostream>
#include <chrono>

namespace hwmon {

FramerateProvider::FramerateProvider(const std::string& file_path)
    : watch_path_(file_path)
{
    // Ensure directory exists
    try {
        std::filesystem::create_directories(std::filesystem::path(watch_path_).parent_path());
    } catch (...) {}

    // Start background watcher
    watcher_thread_ = std::thread(&FramerateProvider::watchLoop, this);
}

FramerateProvider::~FramerateProvider() {
    {
        // Update the flag under the wait mutex so the watcher cannot miss the wakeup
        // between checking the predicate and starting to wait.
        std::lock_guard<std::mutex> lock(watcher_mutex_);
        running_ = false;
    }
    watcher_cv_.notify_all();
    if (watcher_thread_.joinable()) {
        watcher_thread_.join();
    }
}

uint8_t FramerateProvider::getFps() const {
    return current_fps_.load();
}

void FramerateProvider::setFps(uint8_t fps) {
    current_fps_.store(fps);
}

void FramerateProvider::watchLoop() {
    // Only publish file results when they change, so a value set with setFps()
    // stays in place until the file is created, rewritten or removed.
    std::optional<uint8_t> last_file_value;
    while (running_) {
        const auto val = readFromFile();
        if (val != last_file_value) {
            current_fps_.store(val.value_or(0));
            last_file_value = val;
        }

        // Check every 500ms, but allow shutdown to wake the watcher immediately.
        std::unique_lock<std::mutex> lock(watcher_mutex_);
        watcher_cv_.wait_for(lock, std::chrono::milliseconds(500), [this] {
            return !running_.load();
        });
    }
}

std::optional<uint8_t> FramerateProvider::readFromFile() const {
    std::ifstream f(watch_path_);
    if (!f) return std::nullopt;

    int val = 0;
    f >> val;
    if (!f) return std::nullopt;

    char trailing = 0;
    if (f >> trailing) return std::nullopt;

    if (val < 0) val = 0;
    if (val > 255) val = 255;
    return static_cast<uint8_t>(val);
}

} // namespace hwmon
