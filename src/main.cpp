#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>
#include <getopt.h>

#include "protocol.hpp"
#include "serial_port.hpp"
#include "cpu_monitor.hpp"
#include "thermal_monitor.hpp"
#include "gpu_monitor.hpp"
#include "framerate_provider.hpp"

using namespace hwmon;

namespace {

std::atomic<bool> g_running{true};

void signalHandler(int) {
    g_running = false;
}

void printUsage(const char* prog) {
    std::cout <<
        "Usage: " << prog << " [options]\n"
        "\n"
        "Hardware monitor daemon - streams system stats over serial every 2 seconds.\n"
        "\n"
        "Options:\n"
        "  -s, --serial DEVICE     Serial device (default: /dev/ttyUSB0)\n"
        "  -b, --baud RATE         Baud rate (default: 115200)\n"
        "  -i, --interval MS       Update interval in milliseconds (default: 2000)\n"
        "  -f, --fps-file PATH     Path to file containing current FPS (default: /run/hwmon-daemon/current_fps)\n"
        "  -h, --help              Show this help\n"
        "\n"
        "The daemon writes a compact binary packet (see README.md for protocol).\n"
        "FPS can be updated at runtime by writing a number to the fps-file, e.g.:\n"
        "    echo 144 > /run/hwmon-daemon/current_fps\n"
        "\n";
}

struct Config {
    std::string serial_device = "/dev/ttyUSB0";
    int baud = 115200;
    int interval_ms = 2000;
    std::string fps_file = "/run/hwmon-daemon/current_fps";
};

Config parseArgs(int argc, char** argv) {
    Config cfg;

    static const struct option long_opts[] = {
        {"serial",   required_argument, nullptr, 's'},
        {"baud",     required_argument, nullptr, 'b'},
        {"interval", required_argument, nullptr, 'i'},
        {"fps-file", required_argument, nullptr, 'f'},
        {"help",     no_argument,       nullptr, 'h'},
        {nullptr, 0, nullptr, 0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "s:b:i:f:h", long_opts, nullptr)) != -1) {
        switch (c) {
            case 's': cfg.serial_device = optarg; break;
            case 'b': cfg.baud = std::atoi(optarg); break;
            case 'i': cfg.interval_ms = std::atoi(optarg); break;
            case 'f': cfg.fps_file = optarg; break;
            case 'h':
            default:
                printUsage(argv[0]);
                std::exit(0);
        }
    }
    return cfg;
}

void log(const std::string& msg) {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::cerr << "[" << std::put_time(std::localtime(&t), "%F %T") << "] " << msg << std::endl;
}

} // anonymous namespace

int main(int argc, char** argv) {
    Config cfg = parseArgs(argc, argv);

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    log("Starting hardware-monitor-daemon");
    log("Serial: " + cfg.serial_device + " @ " + std::to_string(cfg.baud) + " baud");
    log("Interval: " + std::to_string(cfg.interval_ms) + " ms");
    log("FPS source: " + cfg.fps_file);

    // Initialize components
    SerialPort serial;
    if (!serial.open(cfg.serial_device, cfg.baud)) {
        log("ERROR: " + serial.lastError());
        log("Continuing without serial output (stats will still be computed)");
    } else {
        log("Serial port opened successfully");
    }

    CpuMonitor cpu;
    if (!cpu.initialize()) {
        log("ERROR: Failed to initialize CPU monitor (cannot read /proc/stat)");
        return 1;
    }
    log("Detected " + std::to_string(cpu.getCoreCount()) + " CPU cores");

    ThermalMonitor thermal;
    GpuMonitor gpu;
    FramerateProvider fps_provider(cfg.fps_file);

    // Main loop
    auto last_send = std::chrono::steady_clock::now();

    while (g_running) {
        uint8_t overall_cpu = 0;
        std::vector<uint8_t> per_core;
        if (!cpu.getLoads(overall_cpu, per_core)) {
            log("Warning: CPU load read failed");
        }

        int8_t cpu_temp = 0;
        if (auto t = thermal.readCpuTempC()) {
            cpu_temp = static_cast<int8_t>(*t);
        }

        GpuStats gpu_stats = gpu.getStats();
        uint16_t gpu_clock = gpu_stats.clock_mhz;
        uint8_t  gpu_util  = gpu_stats.utilization;
        int8_t   gpu_temp  = gpu_stats.temperature_c;

        uint8_t fps = fps_provider.getFps();

        // Build and send packet
        auto packet = protocol::buildPacket(
            overall_cpu,
            per_core,
            cpu_temp,
            gpu_clock,
            gpu_util,
            gpu_temp,
            fps
        );

        if (serial.isOpen()) {
            ssize_t written = serial.write(packet);
            if (written < 0) {
                log("Serial write error: " + serial.lastError());
            }
        }

        // Nice console output (visible via journalctl -u hardware-monitor)
        std::string core_str;
        for (size_t i = 0; i < per_core.size() && i < 8; ++i) {
            core_str += std::to_string(per_core[i]) + " ";
        }
        if (per_core.size() > 8) core_str += "...";

        log("CPU: " + std::to_string(overall_cpu) + "% (cores: " + core_str + ") | "
            "CPU temp: " + std::to_string(cpu_temp) + "C | "
            "GPU: " + std::to_string(gpu_clock) + "MHz " + std::to_string(gpu_util) + "% " + std::to_string(gpu_temp) + "C | "
            "FPS: " + std::to_string(fps));

        // Sleep until next interval
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_send).count();
        auto sleep_ms = cfg.interval_ms - elapsed;
        if (sleep_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
        }
        last_send = std::chrono::steady_clock::now();
    }

    log("Shutting down gracefully...");
    serial.close();
    return 0;
}
