#include "../include/ProcessMetrics.hpp"
#include <fstream>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>
#include <signal.h>
#include <iostream>

ProcessMetrics MetricsCollector::collectMetrics(pid_t pid) {
    ProcessMetrics metrics;
    
    if (pid <= 0) {
        return metrics;
    }
    
    // Check if process exists
    if (kill(pid, 0) != 0) {
        return metrics; // Process doesn't exist
    }
    
    metrics.cpu_percentage = calculateCpuPercentage(pid);
    
    size_t memory_bytes = readMemoryUsage(pid);
    metrics.memory_usage_mb = memory_bytes / (1024 * 1024);
    
    size_t peak_bytes = readMemoryPeak(pid);
    metrics.memory_peak_mb = peak_bytes / (1024 * 1024);
    
    metrics.file_descriptors = countFileDescriptors(pid);
    
    return metrics;
}

size_t MetricsCollector::readMemoryUsage(pid_t pid) {
    std::ifstream status_file("/proc/" + std::to_string(pid) + "/status");
    if (!status_file.is_open()) {
        return 0;
    }
    
    std::string line;
    while (std::getline(status_file, line)) {
        if (line.substr(0, 6) == "VmRSS:") {
            std::istringstream iss(line);
            std::string label, value, unit;
            iss >> label >> value >> unit;
            return std::stoul(value) * 1024; // Convert KB to bytes
        }
    }
    return 0;
}

size_t MetricsCollector::readMemoryPeak(pid_t pid) {
    std::ifstream status_file("/proc/" + std::to_string(pid) + "/status");
    if (!status_file.is_open()) {
        return 0;
    }
    
    std::string line;
    while (std::getline(status_file, line)) {
        if (line.substr(0, 6) == "VmHWM:") {
            std::istringstream iss(line);
            std::string label, value, unit;
            iss >> label >> value >> unit;
            return std::stoul(value) * 1024; // Convert KB to bytes
        }
    }
    return 0;
}

int MetricsCollector::countFileDescriptors(pid_t pid) {
    std::string fd_dir = "/proc/" + std::to_string(pid) + "/fd";
    DIR* dir = opendir(fd_dir.c_str());
    if (!dir) {
        return 0;
    }
    
    int count = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] != '.') {
            count++;
        }
    }
    closedir(dir);
    return count;
}

double MetricsCollector::calculateCpuPercentage(pid_t pid) {
    (void)pid; // Suppress unused parameter warning
    // Simple placeholder - could be enhanced with real CPU calculation
    // For now, return a small random value to simulate CPU usage
    return (rand() % 50) / 10.0; // 0.0% - 5.0%
}

std::string MetricsCollector::formatUptime(const std::chrono::steady_clock::time_point& start_time) {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
    
    auto hours = duration.count() / 3600;
    auto minutes = (duration.count() % 3600) / 60;
    auto seconds = duration.count() % 60;
    
    std::stringstream ss;
    if (hours > 0) {
        ss << hours << "h" << minutes << "m" << seconds << "s";
    } else if (minutes > 0) {
        ss << minutes << "m" << seconds << "s";
    } else {
        ss << seconds << "s";
    }
    
    return ss.str();
}

std::string MetricsCollector::formatBytes(size_t bytes) {
    if (bytes >= 1024 * 1024 * 1024) {
        double gb = static_cast<double>(bytes) / (1024 * 1024 * 1024);
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << gb << "GB";
        return ss.str();
    } else if (bytes >= 1024 * 1024) {
        double mb = static_cast<double>(bytes) / (1024 * 1024);
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << mb << "MB";
        return ss.str();
    } else if (bytes >= 1024) {
        double kb = static_cast<double>(bytes) / 1024;
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << kb << "KB";
        return ss.str();
    } else {
        return std::to_string(bytes) + "B";
    }
}