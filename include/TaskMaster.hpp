#pragma once

#include <string>
#include <map>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <fstream>
#include "Process.hpp"
#include "ConfigParser.hpp"
#include "ProcessMetrics.hpp"
#include <iostream>
#include <iomanip>
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
    
    const std::map<std::string, std::unique_ptr<Process>>& getProcesses() const { return processes; }

private:
    void monitorProcesses();
    void checkProcessHealth();
    void restartFailedProcesses();
    bool shouldRestartProcess(const std::string& name, const std::unique_ptr<Process>& process);
    void handleProcessNotRestarting(const std::string& name, const std::unique_ptr<Process>& process);
    void attemptProcessRestart(const std::string& name, const std::unique_ptr<Process>& process);
    void startAutostartProcesses();
    void processCommands();
    std::string trimString(const std::string& str);
    bool executeCommand(const std::string& command);
    bool handleStatusCommand(std::istringstream& iss);
    bool handleStartCommand(std::istringstream& iss);
    bool handleStopCommand(std::istringstream& iss);
    bool handleRestartCommand(std::istringstream& iss);
    bool handleReloadCommand();
    bool handleStatsCommand();
    bool handleLogsCommand(std::istringstream& iss);
    bool handleHelpCommand();
    
    void printDetailedStatus(const std::string& filter = "");
    void printProcessDetails(const std::string& name, const std::unique_ptr<Process>& process);
    void printProcessStats();
    void showProcessLogs(const std::string& process_name, int lines = 10);
    void showLogFile(const std::string& log_file, int lines);
    std::string getStatusColor(ProcessState status);
    
    void removeObsoleteProcesses(const std::map<std::string, ProcessConfig>& new_configs);
    void updateProcessConfigurations(const std::map<std::string, ProcessConfig>& new_configs);
    void addNewProcess(const std::string& instance_name, const ProcessConfig& config);
    void updateExistingProcess(const std::string& instance_name, const ProcessConfig& new_config, 
                              std::unique_ptr<Process>& process);
    bool hasConfigurationChanged(const ProcessConfig& old_config, const ProcessConfig& new_config);
    std::string createInstanceName(const std::string& base_name, int numprocs, int instance_index);
    std::string extractBaseName(const std::string& instance_name);
    std::string config_file;
    ConfigParser config_parser;
    std::map<std::string, std::unique_ptr<Process>> processes;
    
    std::atomic<bool> running;
    std::thread monitor_thread;
    std::mutex processes_mutex;
    std::condition_variable cv;
    
    static constexpr int MONITOR_INTERVAL_MS = 1000;
};
