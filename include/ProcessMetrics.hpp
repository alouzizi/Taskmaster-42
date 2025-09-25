#pragma once

#include <chrono>
#include <string>
#include <sstream>
#include <iomanip>

struct ProcessMetrics {
    double cpu_percentage = 0.0;
    size_t memory_usage_mb = 0;
    size_t memory_peak_mb = 0;
    int file_descriptors = 0;
    
    ProcessMetrics() = default;
};

class MetricsCollector {
public:
    ProcessMetrics collectMetrics(pid_t pid);
    std::string formatUptime(const std::chrono::steady_clock::time_point& start_time);
    std::string formatBytes(size_t bytes);
    
private:
    size_t readMemoryUsage(pid_t pid);
    size_t readMemoryPeak(pid_t pid);
    int countFileDescriptors(pid_t pid);
    double calculateCpuPercentage(pid_t pid);
};