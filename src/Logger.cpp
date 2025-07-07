#include "../include/Logger.hpp"

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

Logger::~Logger() {
    if (log_file.is_open()) {
        log_file.close();
    }
}

void Logger::setLogFile(const std::string& log_file_name) {
    std::lock_guard<std::mutex> lock(log_mutex);
    
    if (log_file.is_open()) {
        log_file.close();
    }
    
    log_file_path = log_file_name;
    log_file.open(log_file_path, std::ios::app);
    
    if (!log_file.is_open()) {
        std::cerr << "Warning: Could not open log file: " << log_file_path << std::endl;
    }
}

std::string Logger::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count();
    
    return ss.str();
}

std::string Logger::logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR:   return "ERROR";
        case LogLevel::DEBUG:   return "DEBUG";
        default:                return "UNKNOWN";
    }
}

void Logger::writeToFile(const std::string& formatted_message) {
    if (log_file.is_open()) {
        log_file << formatted_message << std::endl;
        log_file.flush();
    }
}

void Logger::log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(log_mutex);
    
    std::string timestamp = getCurrentTimestamp();
    std::string level_str = logLevelToString(level);
    
    std::stringstream formatted_message;
    formatted_message << "[" << timestamp << "] [" << level_str << "] " << message;
    
    writeToFile(formatted_message.str());
}

void Logger::info(const std::string& message) {
    log(LogLevel::INFO, message);
}

void Logger::warning(const std::string& message) {
    log(LogLevel::WARNING, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::ERROR, message);
}

void Logger::debug(const std::string& message) {
    log(LogLevel::DEBUG, message);
}

void Logger::logProcessStarted(const std::string& process_name, pid_t pid) {
    std::stringstream ss;
    ss << "Started process " << process_name << " with PID " << pid;
    info(ss.str());
}

void Logger::logProcessStopped(const std::string& process_name, pid_t pid, int exit_status) {
    std::stringstream ss;
    ss << "Process " << process_name << " (PID: " << pid << ") exited with status " << exit_status;
    info(ss.str());
}

void Logger::logProcessDiedUnexpectedly(const std::string& process_name, pid_t pid) {
    std::stringstream ss;
    ss << "Process " << process_name << " (PID: " << pid << ") has died unexpectedly";
    warning(ss.str());
}

void Logger::logProcessRestart(const std::string& process_name, int attempt, int max_attempts) {
    std::stringstream ss;
    ss << "Attempting to restart " << process_name << " (attempt " << attempt << "/" << max_attempts << ")";
    info(ss.str());
}

void Logger::logConfigReloaded() {
    info("Configuration reloaded successfully");
}

void Logger::logTaskMasterStartup() {
    info("TaskMaster starting up");
}

void Logger::logTaskMasterShutdown() {
    info("TaskMaster shutting down");
}
