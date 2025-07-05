#include "TaskMaster.hpp"
#include <iostream>
#include <algorithm>
#include <thread>
#include <chrono>
#include <sstream>

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
    
    {
        std::lock_guard<std::mutex> lock(processes_mutex);
        for (const auto& [name, process] : processes) {
            if (process->getConfig().autostart == AutoStart::TRUE) {
                if (process->start()) {
                    Logger::getInstance().logProcessStarted(name, process->getPid());
                }
            }
        }
    }
    
    monitor_thread = std::thread(&TaskMaster::monitorProcesses, this);
    
    std::cout << "TaskMaster is running. Type 'help' for commands." << std::endl;
    
    std::string command;
    while (running) {
        std::cout << "taskmaster> ";
        if (!std::getline(std::cin, command)) {
            break;
        }
        
        command.erase(0, command.find_first_not_of(" \t"));
        command.erase(command.find_last_not_of(" \t") + 1);
        
        if (command.empty()) continue;
        
        std::istringstream iss(command);
        std::string cmd;
        iss >> cmd;
        if (cmd == "status") {
            std::cout << getStatus() << std::endl;
        } else if (cmd == "start") {
            std::string name;
            iss >> name;
            if (name.empty()) {
                std::cout << "Usage: start <program_name>" << std::endl;
            } else if (startProgram(name)) {
                std::cout << "Started " << name << std::endl;
            } else {
                std::cout << "Failed to start " << name << std::endl;
            }
        } else if (cmd == "stop") {
            std::string name;
            iss >> name;
            if (name.empty()) {
                std::cout << "Usage: stop <program_name>" << std::endl;
            } else if (stopProgram(name)) {
                std::cout << "Stopped " << name << std::endl;
            } else {
                std::cout << "Failed to stop " << name << std::endl;
            }
        } else if (cmd == "restart") {
            std::string name;
            iss >> name;
            if (name.empty()) {
                std::cout << "Usage: restart <program_name>" << std::endl;
            } else if (restartProgram(name)) {
                std::cout << "Restarted " << name << std::endl;
            } else {
                std::cout << "Failed to restart " << name << std::endl;
            }
        } else if (cmd == "reload") {
            if (reloadConfig()) {
                std::cout << "Configuration reloaded" << std::endl;
                Logger::getInstance().logConfigReloaded();
            } else {
                std::cout << "Failed to reload configuration" << std::endl;
                Logger::getInstance().error("Failed to reload configuration");
            }
        } else if (cmd == "quit" || cmd == "exit") {
            break;
        } else if (cmd == "help") {
            std::cout << "Available commands:" << std::endl;
            std::cout << "  status          - Show status of all processes" << std::endl;
            std::cout << "  start <name>    - Start a process" << std::endl;
            std::cout << "  stop <name>     - Stop a process" << std::endl;
            std::cout << "  restart <name>  - Restart a process" << std::endl;
            std::cout << "  reload          - Reload configuration" << std::endl;
            std::cout << "  quit/exit       - Exit TaskMaster" << std::endl;
        } else {
            std::cout << "Unknown command: " << cmd << ". Type 'help' for available commands." << std::endl;
        }
    }
    
    shutdown();
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
    
    for (auto it = processes.begin(); it != processes.end();) {
        if (new_configs.find(it->first) == new_configs.end()) {
            Logger::getInstance().info("Removing process " + it->first + " (no longer in configuration)");
            if (it->second->getState() == ProcessState::RUNNING) {
                it->second->stop();
            }
            it = processes.erase(it);
        } else {
            ++it;
        }
    }
    
    for (const auto& [name, new_config] : new_configs) {
        auto it = processes.find(name);
        if (it == processes.end()) {
            Logger::getInstance().info("Adding new process " + name + " from configuration");
            processes[name] = std::make_unique<Process>(new_config);
            if (new_config.autostart == AutoStart::TRUE) {
                if (processes[name]->start()) {
                    Logger::getInstance().logProcessStarted(name, processes[name]->getPid());
                }
            }
        } else {
            const auto& old_config = it->second->getConfig();
            bool config_changed = false;
            
            if (old_config.command != new_config.command ||
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
                old_config.umask != new_config.umask) {
                config_changed = true;
            }
            
            if (config_changed) {
                Logger::getInstance().info("Configuration changed for process " + name + ", restarting");
                
                if (it->second->getState() == ProcessState::RUNNING) {
                    it->second->stop();
                }
                
                processes[name] = std::make_unique<Process>(new_config);
                
                if (new_config.autostart == AutoStart::TRUE) {
                    if (processes[name]->start()) {
                        Logger::getInstance().logProcessStarted(name, processes[name]->getPid());
                    }
                }
            }
        }
    }
    
    return true;
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
        
        if (process->getState() == ProcessState::EXITED || process->getState() == ProcessState::BACKOFF) {
            bool should_restart = false;
            int last_exit_code = process->getLastExitStatus();
            
            if (process->getState() == ProcessState::BACKOFF) {
                should_restart = true;
                Logger::getInstance().info("Process " + name + " failed during startup, attempting restart");
            } else {
                if (config.autorestart == AutoRestart::TRUE) {
                    if (!config.autorestart_exit_codes.empty()) {
                        if (!process->isExpectedExitCode(last_exit_code)) {
                            should_restart = true;
                        }
                    } else {
                        should_restart = true;
                    }
                } else if (config.autorestart == AutoRestart::UNEXPECTED) {
                    if (!process->isExpectedExitCode(last_exit_code)) {
                        should_restart = true;
                    }
                }
            }
            
            if (!should_restart) {
                if (config.autorestart == AutoRestart::FALSE) {
                    Logger::getInstance().info("Process " + name + " exited with code " + std::to_string(last_exit_code) + ", not restarting (autorestart=false)");
                } else {
                    Logger::getInstance().info("Process " + name + " exited with expected exit code " + std::to_string(last_exit_code) + ", not restarting");
                }
                process->setState(ProcessState::STOPPED);
            } else if (process->getRestartCount() < config.startretries) {
                int next_attempt = process->getRestartCount() + 1;
                Logger::getInstance().logProcessRestart(name, next_attempt, config.startretries);
                
                if (process->getState() == ProcessState::BACKOFF) {
                    Logger::getInstance().info("Process " + name + " startup failed, restarting (attempt " + std::to_string(next_attempt) + "/" + std::to_string(config.startretries) + ")");
                } else {
                    Logger::getInstance().info("Process " + name + " exited with code " + std::to_string(last_exit_code) + ", restarting (attempt " + std::to_string(next_attempt) + "/" + std::to_string(config.startretries) + ")");
                }
                
                std::this_thread::sleep_for(std::chrono::seconds(1));
                if (process->restart()) {
                    Logger::getInstance().logProcessStarted(name, process->getPid());
                }
            } else {
                Logger::getInstance().error("Process " + name + " has exceeded maximum restart attempts and is in FATAL state");
                process->setState(ProcessState::FATAL);
            }
        }
    }
}
