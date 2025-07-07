#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <iomanip>
#include <sstream>
#include <iostream>

enum class LogLevel {
    INFO,
    WARNING,
    ERROR,
    DEBUG
};

class Logger {
public:
    static Logger& getInstance();
    
    void setLogFile(const std::string& log_file);
    void log(LogLevel level, const std::string& message);
    
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);
    void debug(const std::string& message);
    
    void logProcessStarted(const std::string& process_name, pid_t pid);
    void logProcessStopped(const std::string& process_name, pid_t pid, int exit_status);
    void logProcessDiedUnexpectedly(const std::string& process_name, pid_t pid);
    void logProcessRestart(const std::string& process_name, int attempt, int max_attempts);
    void logConfigReloaded();
    void logTaskMasterStartup();
    void logTaskMasterShutdown();

private:
    Logger() = default;
    ~Logger();
    
    std::string getCurrentTimestamp();
    std::string logLevelToString(LogLevel level);
    void writeToFile(const std::string& formatted_message);
    
    std::ofstream log_file;
    std::mutex log_mutex;
    std::string log_file_path;
};

#define LOG_INFO(msg) Logger::getInstance().info(msg)
#define LOG_WARNING(msg) Logger::getInstance().warning(msg)
#define LOG_ERROR(msg) Logger::getInstance().error(msg)
#define LOG_DEBUG(msg) Logger::getInstance().debug(msg)
