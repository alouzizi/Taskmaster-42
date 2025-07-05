#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include "Process.hpp"
#include "ConfigParser.hpp"
#include "Logger.hpp"

class TaskMaster {
public:
    explicit TaskMaster(const std::string& config_file);
    ~TaskMaster();
    
    void run();
    void shutdown();
    
    bool startProgram(const std::string& name);
    bool stopProgram(const std::string& name);
    bool restartProgram(const std::string& name);
    std::string getStatus(const std::string& name = "");
    
    bool reloadConfig();

private:
    void monitorProcesses();
    void checkProcessHealth();
    void restartFailedProcesses();
    
    std::string config_file;
    ConfigParser config_parser;
    std::map<std::string, std::unique_ptr<Process>> processes;
    
    std::atomic<bool> running;
    std::thread monitor_thread;
    std::mutex processes_mutex;
    std::condition_variable cv;
    
    static constexpr int MONITOR_INTERVAL_MS = 1000;
};
