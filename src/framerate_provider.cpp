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
    running_ = false;
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
    while (running_) {
        uint8_t val = readFromFile();
        if (val != 0 || current_fps_.load() == 0) { // allow 0
            current_fps_.store(val);
        }

        // Check every 500ms - cheap
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

uint8_t FramerateProvider::readFromFile() const {
    std::ifstream f(watch_path_);
    if (!f) return 0;

    int val = 0;
    f >> val;
    if (val < 0) val = 0;
    if (val > 255) val = 255;
    return static_cast<uint8_t>(val);
}

} // namespace hwmon
