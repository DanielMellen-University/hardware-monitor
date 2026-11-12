#include "gpu_monitor.hpp"

#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sys/wait.h>

namespace hwmon {

// Minimal NVML definitions (avoids needing CUDA SDK at build time)
#define NVML_SUCCESS                    0
#define NVML_TEMPERATURE_GPU            0
#define NVML_CLOCK_GRAPHICS             0

typedef int nvmlReturn_t;
typedef void* nvmlDevice_t;
typedef struct {
    unsigned int gpu;
    unsigned int memory;
} nvmlUtilization_t;

struct GpuMonitor::NvmlFuncs {
    nvmlReturn_t (*nvmlInit_v2)(void) = nullptr;
    nvmlReturn_t (*nvmlShutdown)(void) = nullptr;
    nvmlReturn_t (*nvmlDeviceGetCount_v2)(unsigned int*) = nullptr;
    nvmlReturn_t (*nvmlDeviceGetHandleByIndex_v2)(unsigned int, nvmlDevice_t*) = nullptr;
    nvmlReturn_t (*nvmlDeviceGetTemperature)(nvmlDevice_t, unsigned int, unsigned int*) = nullptr;
    nvmlReturn_t (*nvmlDeviceGetClockInfo)(nvmlDevice_t, unsigned int, unsigned int*) = nullptr;
    nvmlReturn_t (*nvmlDeviceGetUtilizationRates)(nvmlDevice_t, nvmlUtilization_t*) = nullptr;
};

GpuMonitor::GpuMonitor() {
    nvml_ = std::make_unique<NvmlFuncs>();
    if (tryInitNvml()) {
        nvml_available_ = true;
    }
}

GpuMonitor::~GpuMonitor() {
    shutdownNvml();
}

bool GpuMonitor::tryInitNvml() {
    // Try common locations for the NVIDIA driver library
    const char* candidates[] = {
        "libnvidia-ml.so.1",
        "libnvidia-ml.so",
        "/usr/lib/x86_64-linux-gnu/libnvidia-ml.so.1",
        "/usr/lib/libnvidia-ml.so.1",
        nullptr
    };

    for (int i = 0; candidates[i]; ++i) {
        nvml_lib_ = dlopen(candidates[i], RTLD_LAZY | RTLD_LOCAL);
        if (nvml_lib_) break;
    }

    if (!nvml_lib_) {
        return false;
    }

    // Resolve symbols
    nvml_->nvmlInit_v2 = (decltype(nvml_->nvmlInit_v2))dlsym(nvml_lib_, "nvmlInit_v2");
    if (!nvml_->nvmlInit_v2) {
        // Some older drivers expose nvmlInit
        nvml_->nvmlInit_v2 = (decltype(nvml_->nvmlInit_v2))dlsym(nvml_lib_, "nvmlInit");
    }

    nvml_->nvmlShutdown = (decltype(nvml_->nvmlShutdown))dlsym(nvml_lib_, "nvmlShutdown");
    nvml_->nvmlDeviceGetCount_v2 = (decltype(nvml_->nvmlDeviceGetCount_v2))dlsym(nvml_lib_, "nvmlDeviceGetCount_v2");
    if (!nvml_->nvmlDeviceGetCount_v2) {
        nvml_->nvmlDeviceGetCount_v2 = (decltype(nvml_->nvmlDeviceGetCount_v2))dlsym(nvml_lib_, "nvmlDeviceGetCount");
    }
    nvml_->nvmlDeviceGetHandleByIndex_v2 = (decltype(nvml_->nvmlDeviceGetHandleByIndex_v2))dlsym(nvml_lib_, "nvmlDeviceGetHandleByIndex_v2");
    if (!nvml_->nvmlDeviceGetHandleByIndex_v2) {
        nvml_->nvmlDeviceGetHandleByIndex_v2 = (decltype(nvml_->nvmlDeviceGetHandleByIndex_v2))dlsym(nvml_lib_, "nvmlDeviceGetHandleByIndex");
    }
    nvml_->nvmlDeviceGetTemperature = (decltype(nvml_->nvmlDeviceGetTemperature))dlsym(nvml_lib_, "nvmlDeviceGetTemperature");
    nvml_->nvmlDeviceGetClockInfo = (decltype(nvml_->nvmlDeviceGetClockInfo))dlsym(nvml_lib_, "nvmlDeviceGetClockInfo");
    nvml_->nvmlDeviceGetUtilizationRates = (decltype(nvml_->nvmlDeviceGetUtilizationRates))dlsym(nvml_lib_, "nvmlDeviceGetUtilizationRates");

    if (!nvml_->nvmlInit_v2 || !nvml_->nvmlDeviceGetHandleByIndex_v2) {
        dlclose(nvml_lib_);
        nvml_lib_ = nullptr;
        return false;
    }

    nvmlReturn_t ret = nvml_->nvmlInit_v2();
    if (ret != NVML_SUCCESS) {
        dlclose(nvml_lib_);
        nvml_lib_ = nullptr;
        return false;
    }

    return true;
}

void GpuMonitor::shutdownNvml() {
    if (nvml_available_ && nvml_->nvmlShutdown) {
        nvml_->nvmlShutdown();
    }
    if (nvml_lib_) {
        dlclose(nvml_lib_);
        nvml_lib_ = nullptr;
    }
    nvml_available_ = false;
}

GpuStats GpuMonitor::getStats() {
    if (nvml_available_) {
        GpuStats s = readViaNvml();
        if (s.available) return s;
    }
    // Fallback
    return readViaNvidiaSmi();
}

GpuStats GpuMonitor::readViaNvml() {
    GpuStats stats;

    if (!nvml_->nvmlDeviceGetCount_v2 || !nvml_->nvmlDeviceGetHandleByIndex_v2) {
        return stats;
    }

    unsigned int count = 0;
    if (nvml_->nvmlDeviceGetCount_v2(&count) != NVML_SUCCESS || count == 0) {
        return stats;
    }

    nvmlDevice_t device = nullptr;
    if (nvml_->nvmlDeviceGetHandleByIndex_v2(0, &device) != NVML_SUCCESS) {
        return stats;
    }

    // Temperature
    unsigned int temp = 0;
    if (nvml_->nvmlDeviceGetTemperature &&
        nvml_->nvmlDeviceGetTemperature(device, NVML_TEMPERATURE_GPU, &temp) == NVML_SUCCESS) {
        stats.temperature_c = (temp > 127) ? 127 : static_cast<int8_t>(temp);
    }

    // Clock
    unsigned int clock = 0;
    if (nvml_->nvmlDeviceGetClockInfo &&
        nvml_->nvmlDeviceGetClockInfo(device, NVML_CLOCK_GRAPHICS, &clock) == NVML_SUCCESS) {
        stats.clock_mhz = (clock > 65535) ? 65535 : static_cast<uint16_t>(clock);
    }

    // Utilization
    nvmlUtilization_t util{};
    if (nvml_->nvmlDeviceGetUtilizationRates &&
        nvml_->nvmlDeviceGetUtilizationRates(device, &util) == NVML_SUCCESS) {
        stats.utilization = (util.gpu > 100) ? 100 : static_cast<uint8_t>(util.gpu);
    }

    stats.available = (stats.clock_mhz > 0 || stats.temperature_c != 0 || stats.utilization > 0);
    if (!stats.available) {
        // Still mark as available if we at least got a handle (some GPUs report 0 clocks when idle)
        stats.available = true;
    }

    return stats;
}

GpuStats GpuMonitor::readViaNvidiaSmi() {
    GpuStats stats;

    // Use nvidia-smi query (works even without NVML symbols in some cases)
    FILE* pipe = popen(
        "nvidia-smi --query-gpu=clocks.current.graphics,utilization.gpu,temperature.gpu "
        "--format=csv,noheader,nounits 2>/dev/null", "r");

    if (!pipe) return stats;

    char buffer[256] = {};
    std::string output;
    while (fgets(buffer, sizeof(buffer), pipe)) output += buffer;

    const int status = pclose(pipe);
    const bool succeeded = status != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    parse_nvidia_smi_result(output, succeeded, stats);
    return stats;
}

} // namespace hwmon
