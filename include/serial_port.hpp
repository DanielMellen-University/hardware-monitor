#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <optional>

namespace hwmon {

class SerialPort {
public:
    SerialPort() = default;
    ~SerialPort();

    // Opens and configures the serial port.
    // Returns true on success.
    bool open(const std::string& device, int baud_rate);

    void close();

    bool isOpen() const { return fd_ >= 0; }

    // Writes raw bytes. Returns number of bytes written (or -1 on error).
    ssize_t write(const std::vector<uint8_t>& data);

    // Convenience
    ssize_t write(const uint8_t* data, size_t len);

    std::string lastError() const { return last_error_; }

private:
    bool configure(int baud_rate);
    int baudToSpeed(int baud_rate) const;

    int fd_ = -1;
    std::string device_;
    std::string last_error_;
};

} // namespace hwmon
