#include "../include/TaskMaster.hpp"
#include <cstdlib>

TaskMaster::TaskMaster(const std::string& config_file) 
    : config_file(config_file), running(false) {
    
    Logger::getInstance().setLogFile("taskmaster.log");
    Logger::getInstance().logTaskMasterStartup();
    
    if (!config_parser.parseFile(config_file)) {
        Logger::getInstance().error("Failed to parse configuration file: " + config_file);
        throw std::runtime_error("Failed to parse configuration file: " + config_file);
    }
    
    auto configs = config_parser.getProcessConfigs();
    int total_processes = 0;
    for (const auto& [name, config] : configs) {
        for (int i = 0; i < config.numprocs; i++) {
            std::string instance_name;
            if (config.numprocs == 1) {
                instance_name = name;
            } else {
                instance_name = name + "_" + std::to_string(i);
            }
            processes[instance_name] = std::make_unique<Process>(config);
            total_processes++;
        }
    }
    
    std::cout << "TaskMaster initialized with " << configs.size() << " process configurations (" << total_processes << " total processes)." << std::endl;
    Logger::getInstance().info("TaskMaster initialized with " + std::to_string(configs.size()) + " process configurations (" + std::to_string(total_processes) + " total processes)");
}

TaskMaster::~TaskMaster() {
    Logger::getInstance().logTaskMasterShutdown();
    shutdown();
}

void TaskMaster::run() {
    running = true;
    
    startAutostartProcesses();
    
    monitor_thread = std::thread(&TaskMaster::monitorProcesses, this);
    
    std::cout << "TaskMaster is running. Type 'help' for commands." << std::endl;
    
    processCommands();
    
    shutdown();
}

void TaskMaster::startAutostartProcesses() {
    std::lock_guard<std::mutex> lock(processes_mutex);
    for (const auto& [name, process] : processes) {
        if (process->getConfig().autostart == AutoStart::TRUE) {
            if (process->start()) {
                Logger::getInstance().logProcessStarted(name, process->getPid());
            }
        }
    }
}

void TaskMaster::processCommands() {
    std::string command;
    while (running) {
        std::cout << "taskmaster> ";
        if (!std::getline(std::cin, command)) {
            break;
        }
        
        command = trimString(command);
        if (command.empty()) continue;
        
        if (!executeCommand(command)) {
            break;
        }
    }
}

std::string TaskMaster::trimString(const std::string& str) {
    size_t first = str.find_first_not_of(" \t");
    if (first == std::string::npos) return "";
    
    size_t last = str.find_last_not_of(" \t");
    return str.substr(first, (last - first + 1));
}

bool TaskMaster::executeCommand(const std::string& command) {
    std::istringstream iss(command);
    std::string cmd;
    iss >> cmd;
    
    if (cmd == "status") {
        return handleStatusCommand(iss);
    } else if (cmd == "start") {
        return handleStartCommand(iss);
    } else if (cmd == "stop") {
        return handleStopCommand(iss);
    } else if (cmd == "restart") {
        return handleRestartCommand(iss);
    } else if (cmd == "reload") {
        return handleReloadCommand();
    } else if (cmd == "stats") {
        return handleStatsCommand();
    } else if (cmd == "logs") {
        return handleLogsCommand(iss);
    } else if (cmd == "clear") {
        return handleClearCommand();
    } else if (cmd == "quit" || cmd == "exit") {
        return false;
    } else if (cmd == "help") {
        return handleHelpCommand();
    } else {
        std::cout << "Unknown command: " << cmd << ". Type 'help' for available commands." << std::endl;
        return true;
    }
}

bool TaskMaster::handleStatusCommand(std::istringstream& iss) {
    std::string arg;
    bool detailed = false;
    std::string filter;
    
    // Parse arguments
    while (iss >> arg) {
        if (arg == "--detailed") {
            detailed = true;
        } else {
            filter = arg;
        }
    }
    
    if (detailed) {
        printDetailedStatus(filter);
    } else {
        std::cout << getStatus(filter) << std::endl;
    }
    return true;
}

bool TaskMaster::handleStartCommand(std::istringstream& iss) {
    std::string name;
    iss >> name;
    if (name.empty()) {
        std::cout << "Usage: start <program_name>" << std::endl;
    } else if (startProgram(name)) {
        std::cout << "Started " << name << std::endl;
    } else {
        std::cout << "Failed to start " << name << std::endl;
    }
    return true;
}

bool TaskMaster::handleStopCommand(std::istringstream& iss) {
    std::string name;
    iss >> name;
    if (name.empty()) {
        std::cout << "Usage: stop <program_name>" << std::endl;
    } else if (stopProgram(name)) {
        std::cout << "Stopped " << name << std::endl;
    } else {
        std::cout << "Failed to stop " << name << std::endl;
    }
    return true;
}

bool TaskMaster::handleRestartCommand(std::istringstream& iss) {
    std::string name;
    iss >> name;
    if (name.empty()) {
        std::cout << "Usage: restart <program_name>" << std::endl;
    } else if (restartProgram(name)) {
        std::cout << "Restarted " << name << std::endl;
    } else {
        std::cout << "Failed to restart " << name << std::endl;
    }
    return true;
}

bool TaskMaster::handleReloadCommand() {
    if (reloadConfig()) {
        std::cout << "Configuration reloaded" << std::endl;
        Logger::getInstance().logConfigReloaded();
    } else {
        std::cout << "Failed to reload configuration" << std::endl;
        Logger::getInstance().error("Failed to reload configuration");
    }
    return true;
}

bool TaskMaster::handleStatsCommand() {
    printProcessStats();
    return true;
}

bool TaskMaster::handleLogsCommand(std::istringstream& iss) {
    std::string process_name;
    std::string lines_str;
    int lines = 10;  // default
    
    iss >> process_name;
    if (process_name.empty()) {
        std::cout << "Usage: logs <process_name> [lines]" << std::endl;
        std::cout << "Example: logs nginx 20" << std::endl;
        return true;
    }
    
    // Check if lines argument is provided
    if (iss >> lines_str) {
        try {
            lines = std::stoi(lines_str);
            if (lines <= 0) lines = 10;
        } catch (const std::exception&) {
            lines = 10;
        }
    }
    
    showProcessLogs(process_name, lines);
    return true;
}

bool TaskMaster::handleHelpCommand() {
    std::cout << "Available commands:" << std::endl;
    std::cout << "  status [name]           - Show status of all processes or specific process" << std::endl;
    std::cout << "  status --detailed       - Show detailed status with CPU, memory, and metrics" << std::endl;
    std::cout << "  status --detailed <name> - Show detailed status for specific process" << std::endl;
    std::cout << "  stats                   - Show process statistics and system health" << std::endl;
    std::cout << "  logs <name> [lines]     - Show process logs (default: 10 lines)" << std::endl;
    std::cout << "  start <name>            - Start a process" << std::endl;
    std::cout << "  stop <name>             - Stop a process" << std::endl;
    std::cout << "  restart <name>          - Restart a process" << std::endl;
    std::cout << "  reload                  - Reload configuration" << std::endl;
    std::cout << "  clear                   - Clear the terminal screen" << std::endl;
    std::cout << "  quit/exit               - Exit TaskMaster" << std::endl;
    return true;
}

bool TaskMaster::handleClearCommand() {
    int result = system("clear");
    (void)result; // Explicitly ignore the return value
    return true;
}

void TaskMaster::shutdown() {
    if (!running) return;
    
    running = false;
    cv.notify_all();
    
    if (monitor_thread.joinable()) {
        monitor_thread.join();
    }
    
    std::lock_guard<std::mutex> lock(processes_mutex);
    for (const auto& [name, process] : processes) {
        if (process->getState() == ProcessState::RUNNING) {
            pid_t pid = process->getPid();
            if (process->stop()) {
                Logger::getInstance().logProcessStopped(name, pid, 0);
            }
        }
    }
}

bool TaskMaster::startProgram(const std::string& name) {
    std::lock_guard<std::mutex> lock(processes_mutex);
    auto it = processes.find(name);
    if (it == processes.end()) {
        return false;
    }
    bool success = it->second->start();
    if (success) {
        Logger::getInstance().logProcessStarted(name, it->second->getPid());
    }
    return success;
}

bool TaskMaster::stopProgram(const std::string& name) {
    std::lock_guard<std::mutex> lock(processes_mutex);
    auto it = processes.find(name);
    if (it == processes.end()) {
        return false;
    }
    pid_t pid = it->second->getPid();
    bool success = it->second->stop();
    if (success) {
        Logger::getInstance().logProcessStopped(name, pid, 0);
    }
    return success;
}

bool TaskMaster::restartProgram(const std::string& name) {
    std::lock_guard<std::mutex> lock(processes_mutex);
    auto it = processes.find(name);
    if (it == processes.end()) {
        return false;
    }
    
    Logger::getInstance().info("Restarting process " + name);
    
    bool success = it->second->restart();
    if (success) {
        Logger::getInstance().logProcessStarted(name, it->second->getPid());
    }
    return success;
}

std::string TaskMaster::getStatus(const std::string& name) {
    std::lock_guard<std::mutex> lock(processes_mutex);
    std::string result;
    
    if (name.empty()) {
        result += "Process Status:\n";
        result += "=====================================\n";
        for (const auto& [proc_name, process] : processes) {
            result += proc_name + ": " + process->getStateString();
            if (process->getState() == ProcessState::RUNNING) {
                result += " (PID: " + std::to_string(process->getPid()) + 
                         ", Uptime: " + std::to_string(process->getUptime().count()) + "s)";
            }
            result += "\n";
        }
    } else {
        auto it = processes.find(name);
        if (it != processes.end()) {
            result = name + ": " + it->second->getStateString();
            if (it->second->getState() == ProcessState::RUNNING) {
                result += " (PID: " + std::to_string(it->second->getPid()) + 
                         ", Uptime: " + std::to_string(it->second->getUptime().count()) + "s)";
            }
        } else {
            result = "Process not found: " + name;
        }
    }
    
    return result;
}

bool TaskMaster::reloadConfig() {
    if (!config_parser.parseFile(config_file)) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(processes_mutex);
    
    auto new_configs = config_parser.getProcessConfigs();
    
    removeObsoleteProcesses(new_configs);
    
    updateProcessConfigurations(new_configs);
    
    return true;
}

void TaskMaster::removeObsoleteProcesses(const std::map<std::string, ProcessConfig>& new_configs) {
    for (auto it = processes.begin(); it != processes.end();) {
        const std::string& process_name = extractBaseName(it->first);
        
        if (new_configs.find(process_name) == new_configs.end()) {
            Logger::getInstance().info("Removing process " + it->first + " (no longer in configuration)");
            
            if (it->second->getState() == ProcessState::RUNNING) {
                it->second->stop();
            }
            
            it = processes.erase(it);
        } else {
            ++it;
        }
    }
}

void TaskMaster::updateProcessConfigurations(const std::map<std::string, ProcessConfig>& new_configs) {
    for (const auto& [name, new_config] : new_configs) {
        for (int i = 0; i < new_config.numprocs; i++) {
            std::string instance_name = createInstanceName(name, new_config.numprocs, i);
            
            auto it = processes.find(instance_name);
            if (it == processes.end()) {
                addNewProcess(instance_name, new_config);
            } else {
                updateExistingProcess(instance_name, new_config, it->second);
            }
        }
    }
}

void TaskMaster::addNewProcess(const std::string& instance_name, const ProcessConfig& config) {
    Logger::getInstance().info("Adding new process " + instance_name + " from configuration");
    
    processes[instance_name] = std::make_unique<Process>(config);
    
    if (config.autostart == AutoStart::TRUE) {
        if (processes[instance_name]->start()) {
            Logger::getInstance().logProcessStarted(instance_name, processes[instance_name]->getPid());
        }
    }
}

void TaskMaster::updateExistingProcess(const std::string& instance_name, const ProcessConfig& new_config, 
                                     std::unique_ptr<Process>& process) {
    if (hasConfigurationChanged(process->getConfig(), new_config)) {
        Logger::getInstance().info("Configuration changed for process " + instance_name + ", restarting");
        
        if (process->getState() == ProcessState::RUNNING) {
            process->stop();
        }
        
        processes[instance_name] = std::make_unique<Process>(new_config);
        
        if (new_config.autostart == AutoStart::TRUE) {
            if (processes[instance_name]->start()) {
                Logger::getInstance().logProcessStarted(instance_name, processes[instance_name]->getPid());
            }
        }
    }
}

bool TaskMaster::hasConfigurationChanged(const ProcessConfig& old_config, const ProcessConfig& new_config) {
    return old_config.command != new_config.command ||
           old_config.autostart != new_config.autostart ||
           old_config.autorestart != new_config.autorestart ||
           old_config.autorestart_exit_codes != new_config.autorestart_exit_codes ||
           old_config.startretries != new_config.startretries ||
           old_config.starttime != new_config.starttime ||
           old_config.stopsignal != new_config.stopsignal ||
           old_config.stoptime != new_config.stoptime ||
           old_config.stdout_logfile != new_config.stdout_logfile ||
           old_config.stderr_logfile != new_config.stderr_logfile ||
           old_config.workingdir != new_config.workingdir ||
           old_config.environment != new_config.environment ||
           old_config.umask != new_config.umask;
}

std::string TaskMaster::createInstanceName(const std::string& base_name, int numprocs, int instance_index) {
    if (numprocs == 1) {
        return base_name;
    }
    return base_name + "_" + std::to_string(instance_index);
}

std::string TaskMaster::extractBaseName(const std::string& instance_name) {
    size_t underscore_pos = instance_name.find_last_of('_');
    if (underscore_pos != std::string::npos) {
        std::string suffix = instance_name.substr(underscore_pos + 1);
        if (std::all_of(suffix.begin(), suffix.end(), ::isdigit)) {
            return instance_name.substr(0, underscore_pos);
        }
    }
    return instance_name;
}

void TaskMaster::monitorProcesses() {
    while (running) {
        std::unique_lock<std::mutex> lock(processes_mutex);
        cv.wait_for(lock, std::chrono::milliseconds(MONITOR_INTERVAL_MS), 
                    [this] { return !running; });
        
        if (!running) break;
        
        checkProcessHealth();
        restartFailedProcesses();
    }
}

void TaskMaster::checkProcessHealth() {
    for (const auto& [name, process] : processes) {
        if (process->getState() == ProcessState::RUNNING) {
            pid_t current_pid = process->getPid();
            
            if (!process->isAlive()) {
                int exit_code = process->getLastExitStatus();
                
                auto uptime = process->getUptime();
                int starttime_seconds = process->getConfig().starttime;
                
                if (uptime.count() < starttime_seconds) {
                    Logger::getInstance().error("Process " + name + " (PID: " + std::to_string(current_pid) + 
                        ") died during startup period (uptime: " + std::to_string(uptime.count()) + 
                        "s < starttime: " + std::to_string(starttime_seconds) + "s)");
                    process->setState(ProcessState::BACKOFF);
                } else {
                    if (process->isExpectedExitCode(exit_code) || 
                        process->getConfig().autorestart == AutoRestart::FALSE ||
                        (process->getConfig().autorestart == AutoRestart::TRUE && process->getConfig().autorestart_exit_codes.empty())) {
                        Logger::getInstance().info("Process " + name + " (PID: " + std::to_string(current_pid) + ") exited with expected status " + std::to_string(exit_code));
                    } else {
                        Logger::getInstance().logProcessDiedUnexpectedly(name, current_pid);
                    }
                    process->setState(ProcessState::EXITED);
                }
            }
        }
    }
}

void TaskMaster::restartFailedProcesses() {
    for (const auto& [name, process] : processes) {
        const auto& config = process->getConfig();
        ProcessState state = process->getState();
        
        if (state != ProcessState::EXITED && state != ProcessState::BACKOFF) {
            continue;
        }
        
        if (!shouldRestartProcess(name, process)) {
            handleProcessNotRestarting(name, process);
            continue;
        }
        
        if (process->getRestartCount() >= config.startretries) {
            Logger::getInstance().error("Process " + name + " has exceeded maximum restart attempts and is in FATAL state");
            process->setState(ProcessState::FATAL);
            continue;
        }
        
        attemptProcessRestart(name, process);
    }
}

bool TaskMaster::shouldRestartProcess(const std::string& name, const std::unique_ptr<Process>& process) {
    const auto& config = process->getConfig();
    int last_exit_code = process->getLastExitStatus();
    
    if (process->getState() == ProcessState::BACKOFF) {
        Logger::getInstance().info("Process " + name + " failed during startup, attempting restart");
        return true;
    }
    
    switch (config.autorestart) {
        case AutoRestart::TRUE:
            return config.autorestart_exit_codes.empty() || 
                   !process->isExpectedExitCode(last_exit_code);
        
        case AutoRestart::UNEXPECTED:
            return !process->isExpectedExitCode(last_exit_code);
        
        case AutoRestart::FALSE:
        default:
            return false;
    }
}

void TaskMaster::handleProcessNotRestarting(const std::string& name, const std::unique_ptr<Process>& process) {
    const auto& config = process->getConfig();
    int last_exit_code = process->getLastExitStatus();
    
    if (config.autorestart == AutoRestart::FALSE) {
        Logger::getInstance().info("Process " + name + " exited with code " + 
                                 std::to_string(last_exit_code) + ", not restarting (autorestart=false)");
    } else {
        Logger::getInstance().info("Process " + name + " exited with expected exit code " + 
                                 std::to_string(last_exit_code) + ", not restarting");
    }
    process->setState(ProcessState::STOPPED);
}

void TaskMaster::attemptProcessRestart(const std::string& name, const std::unique_ptr<Process>& process) {
    const auto& config = process->getConfig();
    int next_attempt = process->getRestartCount() + 1;
    int last_exit_code = process->getLastExitStatus();
    
    Logger::getInstance().logProcessRestart(name, next_attempt, config.startretries);
    
    if (process->getState() == ProcessState::BACKOFF) {
        Logger::getInstance().info("Process " + name + " startup failed, restarting (attempt " + 
                                 std::to_string(next_attempt) + "/" + std::to_string(config.startretries) + ")");
    } else {
        Logger::getInstance().info("Process " + name + " exited with code " + std::to_string(last_exit_code) + 
                                 ", restarting (attempt " + std::to_string(next_attempt) + "/" + 
                                 std::to_string(config.startretries) + ")");
    }
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (process->restart()) {
        Logger::getInstance().logProcessStarted(name, process->getPid());
    }
}

void TaskMaster::printDetailedStatus(const std::string& filter) {
    Logger::getInstance().logDetailedStatusRequest();
    
    std::cout << "\nProcess Status (Detailed):\n";
    std::cout << "==========================================\n";
    
    std::lock_guard<std::mutex> lock(processes_mutex);
    
    bool found_any = false;
    for (const auto& [name, process] : processes) {
        // Apply filter if provided
        if (!filter.empty() && name.find(filter) == std::string::npos) {
            continue;
        }
        
        printProcessDetails(name, process);
        std::cout << "\n";
        found_any = true;
    }
    
    if (!filter.empty() && !found_any) {
        std::cout << "No processes found matching: " << filter << "\n";
    }
}

void TaskMaster::printProcessDetails(const std::string& name, const std::unique_ptr<Process>& process) {
    std::string status_color = getStatusColor(process->getState());
    std::cout << status_color << name << ": " << process->getStateString() << "\033[0m";
    
    if (process->getState() == ProcessState::RUNNING) {
        pid_t pid = process->getPid();
        MetricsCollector collector;
        ProcessMetrics metrics = collector.collectMetrics(pid);
        std::string uptime = collector.formatUptime(process->getStartTime());
        
        std::cout << " (PID: " << pid << ", Uptime: " << uptime << ")\n";
        
        // CPU and Memory info
        std::cout << "  ├─ CPU: " << std::fixed << std::setprecision(1) 
                  << metrics.cpu_percentage << "% | Memory: " 
                  << collector.formatBytes(metrics.memory_usage_mb * 1024 * 1024);
        
        if (metrics.memory_peak_mb > 0) {
            std::cout << " (peak: " << collector.formatBytes(metrics.memory_peak_mb * 1024 * 1024) << ")";
        }
        std::cout << "\n";
        
        // File descriptors and restart count
        std::cout << "  ├─ FDs: " << metrics.file_descriptors << "/1024";
        std::cout << " | Restarts: " << process->getRestartCount() << "\n";
        
        // Health check status
        std::cout << "  └─ Last Health Check: \033[32mOK\033[0m (active)\n";
        
    } else if (process->getState() == ProcessState::FATAL) {
        std::cout << " (Last exit: " << process->getLastExitStatus() 
                  << ", Restarts: " << process->getRestartCount() << ")\n";
        std::cout << "  └─ Process failed to start or crashed\n";
    } else {
        std::cout << "\n";
    }
}

void TaskMaster::printProcessStats() {
    std::lock_guard<std::mutex> lock(processes_mutex);
    
    int total = 0;
    int running = 0, stopped = 0, starting = 0, stopping = 0, failed = 0, exited = 0, backoff = 0;
    int total_restarts = 0;
    std::chrono::seconds total_uptime(0);
    int running_count = 0;
    
    for (const auto& [name, process] : processes) {
        total++;
        ProcessState state = process->getState();
        
        switch (state) {
            case ProcessState::RUNNING:
                running++;
                total_uptime += process->getUptime();
                running_count++;
                break;
            case ProcessState::STOPPED:    stopped++; break;
            case ProcessState::STARTING:   starting++; break;
            case ProcessState::STOPPING:   stopping++; break;
            case ProcessState::FATAL:      failed++; break;
            case ProcessState::EXITED:     exited++; break;
            case ProcessState::BACKOFF:    backoff++; break;
            default: break;
        }
        
        total_restarts += process->getRestartCount();
    }
    
    // Calculate average uptime
    std::string avg_uptime = "0s";
    if (running_count > 0) {
        auto avg_seconds = total_uptime.count() / running_count;
        auto hours = avg_seconds / 3600;
        auto minutes = (avg_seconds % 3600) / 60;
        auto seconds = avg_seconds % 60;
        
        std::stringstream ss;
        if (hours > 0) {
            ss << hours << "h " << minutes << "m " << seconds << "s";
        } else if (minutes > 0) {
            ss << minutes << "m " << seconds << "s";
        } else {
            ss << seconds << "s";
        }
        avg_uptime = ss.str();
    }
    
    std::cout << "\n\033[1mProcess Statistics:\033[0m\n";
    std::cout << "==========================================\n";
    std::cout << "Total Processes:     " << total << "\n";
    std::cout << "\033[32mRunning:\033[0m             " << running;
    if (starting > 0) std::cout << " (+" << starting << " starting)";
    std::cout << "\n";
    std::cout << "\033[33mStopped:\033[0m             " << stopped;
    if (stopping > 0) std::cout << " (+" << stopping << " stopping)";
    std::cout << "\n";
    if (failed > 0) {
        std::cout << "\033[31mFailed:\033[0m              " << failed << "\n";
    }
    if (exited > 0) {
        std::cout << "\033[36mExited:\033[0m              " << exited << "\n";
    }
    if (backoff > 0) {
        std::cout << "\033[35mBackoff:\033[0m             " << backoff << "\n";
    }
    std::cout << "Total Restarts:      " << total_restarts << "\n";
    std::cout << "Average Uptime:      " << avg_uptime << "\n";
    
    // Health indicator
    double health_score = (running_count > 0) ? (double(running) / total) * 100.0 : 0.0;
    std::cout << "System Health:       ";
    if (health_score >= 80.0) {
        std::cout << "\033[32m" << std::fixed << std::setprecision(1) << health_score << "% (EXCELLENT)\033[0m\n";
    } else if (health_score >= 60.0) {
        std::cout << "\033[33m" << std::fixed << std::setprecision(1) << health_score << "% (GOOD)\033[0m\n";
    } else if (health_score >= 40.0) {
        std::cout << "\033[33m" << std::fixed << std::setprecision(1) << health_score << "% (WARNING)\033[0m\n";
    } else {
        std::cout << "\033[31m" << std::fixed << std::setprecision(1) << health_score << "% (CRITICAL)\033[0m\n";
    }
}

void TaskMaster::showProcessLogs(const std::string& process_name, int lines) {
    std::lock_guard<std::mutex> lock(processes_mutex);
    
    // Find the process
    auto it = processes.find(process_name);
    if (it == processes.end()) {
        std::cout << "Process not found: " << process_name << std::endl;
        return;
    }
    
    const auto& process = it->second;
    const auto& config = process->getConfig();
    
    std::cout << "\n\033[1mLogs for " << process_name << " (last " << lines << " lines):\033[0m\n";
    std::cout << "=========================================\n";
    
    // Show stdout logs
    if (!config.stdout_logfile.empty() && config.stdout_logfile != "/dev/null") {
        std::cout << "\033[32m[STDOUT]\033[0m " << config.stdout_logfile << ":\n";
        showLogFile(config.stdout_logfile, lines);
    }
    
    // Show stderr logs
    if (!config.stderr_logfile.empty() && config.stderr_logfile != "/dev/null") {
        std::cout << "\n\033[31m[STDERR]\033[0m " << config.stderr_logfile << ":\n";
        showLogFile(config.stderr_logfile, lines);
    }
    
    // If no log files configured
    if ((config.stdout_logfile.empty() || config.stdout_logfile == "/dev/null") &&
        (config.stderr_logfile.empty() || config.stderr_logfile == "/dev/null")) {
        std::cout << "\033[33mNo log files configured for this process.\033[0m\n";
        std::cout << "Output goes to console or /dev/null.\n";
    }
}

void TaskMaster::showLogFile(const std::string& log_file, int lines) {
    std::ifstream file(log_file);
    if (!file.is_open()) {
        std::cout << "\033[31mError: Could not open log file: " << log_file << "\033[0m\n";
        return;
    }
    
    // Read all lines into a vector
    std::vector<std::string> all_lines;
    std::string line;
    while (std::getline(file, line)) {
        all_lines.push_back(line);
    }
    file.close();
    
    if (all_lines.empty()) {
        std::cout << "\033[33m(Log file is empty)\033[0m\n";
        return;
    }
    
    // Show last N lines
    int start_line = std::max(0, static_cast<int>(all_lines.size()) - lines);
    int line_number = start_line + 1;
    
    for (size_t i = start_line; i < all_lines.size(); ++i) {
        std::cout << std::setw(4) << line_number++ << " | " << all_lines[i] << "\n";
    }
    
    if (start_line > 0) {
        std::cout << "\033[33m... (showing last " << lines << " of " 
                  << all_lines.size() << " total lines)\033[0m\n";
    }
}

std::string TaskMaster::getStatusColor(ProcessState status) {
    switch (status) {
        case ProcessState::RUNNING:  return "\033[32m";  // Green
        case ProcessState::STOPPED:  return "\033[33m";  // Yellow
        case ProcessState::FATAL:    return "\033[31m";  // Red
        case ProcessState::STARTING: return "\033[36m";  // Cyan
        case ProcessState::STOPPING: return "\033[35m";  // Magenta
        default:                      return "\033[0m";   // Reset
    }
}
