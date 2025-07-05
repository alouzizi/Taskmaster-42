#pragma once

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <atomic>

enum class ProcessState {
    STOPPED,
    STARTING,
    RUNNING,
    BACKOFF,
    STOPPING,
    EXITED,
    FATAL,
    UNKNOWN
};

enum class AutoStart {
    FALSE,
    TRUE,
    UNEXPECTED
};

enum class AutoRestart {
    FALSE,
    TRUE,
    UNEXPECTED
};

struct ProcessConfig {
    std::string name;
    std::string command;
    int numprocs = 1;
    int priority = 999;
    AutoStart autostart = AutoStart::TRUE;
    AutoRestart autorestart = AutoRestart::TRUE;
    std::vector<int> autorestart_exit_codes;
    int startretries = 3;
    int starttime = 1;
    std::string stopsignal = "TERM";
    int stoptime = 10;
    std::string stdout_logfile = "";
    std::string stderr_logfile = "";
    std::string workingdir = "/tmp";
    std::map<std::string, std::string> environment;
    int umask = 022;
};

class Process {
public:
    explicit Process(const ProcessConfig& config);
    ~Process();
    
    bool start();
    bool stop();
    bool restart();
    
    ProcessState getState() const { return state; }
    std::string getStateString() const;
    pid_t getPid() const { return pid; }
    const ProcessConfig& getConfig() const { return config; }
    
    bool isAlive();
    
    std::chrono::seconds getUptime() const;
    
    int getRestartCount() const { return restart_count; }
    
    int getLastExitStatus() const { return last_exit_status; }
    
    bool isExpectedExitCode(int exit_code) const;
    
    void setState(ProcessState state);

private:
    ProcessConfig config;
    std::atomic<ProcessState> state;
    std::atomic<pid_t> pid;
    std::atomic<int> restart_count;
    std::atomic<int> last_exit_status;
    std::chrono::time_point<std::chrono::steady_clock> start_time;
    std::chrono::time_point<std::chrono::steady_clock> last_restart;
    
    bool executeCommand();
    bool killProcess(const std::string& signal = "TERM");
};
