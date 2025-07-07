#include "../include/Process.hpp"
#include "../include/Logger.hpp"

Process::Process(const ProcessConfig& config) 
    : config(config), state(ProcessState::STOPPED), pid(-1), restart_count(0), last_exit_status(0) {
}

Process::~Process() {
    if (state == ProcessState::RUNNING) {
        stop();
    }
}

bool Process::start() {
    if (state == ProcessState::RUNNING) {
        return true;
    }
    
    setState(ProcessState::STARTING);
    
    if (!executeCommand()) {
        setState(ProcessState::FATAL);
        return false;
    }
    
    setState(ProcessState::RUNNING);
    start_time = std::chrono::steady_clock::now();
    return true;
}

bool Process::stop() {
    if (state != ProcessState::RUNNING) {
        return true;
    }
    
    setState(ProcessState::STOPPING);
    
    if (killProcess(config.stopsignal)) {
        for (int i = 0; i < config.stoptime; ++i) {
            if (!isAlive()) {
                setState(ProcessState::STOPPED);
                pid = -1;
                return true;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        if (isAlive()) {
            Logger::getInstance().warning("Process " + config.name + " did not stop gracefully, force killing...");
            if (killProcess("KILL")) {
                setState(ProcessState::STOPPED);
                pid = -1;
                return true;
            }
        }
    }
    
    setState(ProcessState::FATAL);
    return false;
}

bool Process::restart() {
    if (state == ProcessState::RUNNING) {
        if (!stop()) {
            return false;
        }
    }
    
    restart_count++;
    last_restart = std::chrono::steady_clock::now();
    
    return start();
}

std::string Process::getStateString() const {
    switch (state.load()) {
        case ProcessState::STOPPED: return "STOPPED";
        case ProcessState::STARTING: return "STARTING";
        case ProcessState::RUNNING: return "RUNNING";
        case ProcessState::BACKOFF: return "BACKOFF";
        case ProcessState::STOPPING: return "STOPPING";
        case ProcessState::EXITED: return "EXITED";
        case ProcessState::FATAL: return "FATAL";
        case ProcessState::UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}

bool Process::isAlive() {
    if (pid <= 0) {
        return false;
    }
    
    int status;
    pid_t result = waitpid(pid, &status, WNOHANG);
    
    if (result == pid) {
        int exit_status = WEXITSTATUS(status);
        last_exit_status = exit_status;
        pid_t exited_pid = pid;
        Logger::getInstance().logProcessStopped(config.name, exited_pid, exit_status);
        pid = -1;
        setState(ProcessState::EXITED);
        return false;
    } else if (result == 0) {
        return true;
    } else if (result == -1) {
        if (errno == ECHILD) {
            pid = -1;
            setState(ProcessState::EXITED);
            return false;
        }
        if (kill(pid, 0) != 0) {
            pid = -1;
            return false;
        }
        return true;
    }
    
    return false;
}

std::chrono::seconds Process::getUptime() const {
    if (state == ProcessState::STOPPED || state == ProcessState::FATAL) {
        return std::chrono::seconds(0);
    }
    
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
}

bool Process::isExpectedExitCode(int exit_code) const {
    const auto& expected_codes = config.autorestart_exit_codes;
    return std::find(expected_codes.begin(), expected_codes.end(), exit_code) != expected_codes.end();
}

bool Process::executeCommand() {
    pid_t child_pid = fork();
    
    if (child_pid == -1) {
        std::cerr << "Failed to fork process for " << config.name << ": " << strerror(errno) << std::endl;
        return false;
    }
    
    if (child_pid == 0) {
        setupChildProcess();
        
        auto tokens = parseCommand();
        if (tokens.empty()) {
            std::cerr << "Empty command for process " << config.name << std::endl;
            exit(1);
        }
        
        std::vector<char*> args;
        for (const auto& token : tokens) {
            args.push_back(const_cast<char*>(token.c_str()));
        }
        args.push_back(nullptr);
        
        execvp(args[0], args.data());
        
        std::cerr << "Failed to execute " << config.command << ": " << strerror(errno) << std::endl;
        exit(1);
    } else {
        pid = child_pid;
        return true;
    }
}

void Process::setupChildProcess() {
    if (!config.stdout_logfile.empty()) {
        int stdout_fd;
        if (config.stdout_logfile == "/dev/null") {
            stdout_fd = open("/dev/null", O_WRONLY);
        } else {
            stdout_fd = open(config.stdout_logfile.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
        }
        if (stdout_fd != -1) {
            dup2(stdout_fd, STDOUT_FILENO);
            close(stdout_fd);
        }
    }
    
    if (!config.stderr_logfile.empty()) {
        int stderr_fd;
        if (config.stderr_logfile == "/dev/null") {
            stderr_fd = open("/dev/null", O_WRONLY);
        } else {
            stderr_fd = open(config.stderr_logfile.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
        }
        if (stderr_fd != -1) {
            dup2(stderr_fd, STDERR_FILENO);
            close(stderr_fd);
        }
    }
    
    if (chdir(config.workingdir.c_str()) != 0) {
        std::cerr << "Failed to change directory to " << config.workingdir << std::endl;
        exit(1);
    }
    
    umask(config.umask);
    
    for (const auto& [key, value] : config.environment) {
        setenv(key.c_str(), value.c_str(), 1);
    }
}

std::vector<std::string> Process::parseCommand() const {
    std::vector<std::string> tokens;
    std::string current_token;
    bool in_quotes = false;
    
    for (size_t i = 0; i < config.command.length(); ++i) {
        char c = config.command[i];
        
        if (c == '"' && (i == 0 || config.command[i-1] != '\\')) {
            in_quotes = !in_quotes;
        } else if (c == ' ' && !in_quotes) {
            if (!current_token.empty()) {
                tokens.push_back(current_token);
                current_token.clear();
            }
        } else {
            current_token += c;
        }
    }
    
    if (!current_token.empty()) {
        tokens.push_back(current_token);
    }
    
    return tokens;
}

void Process::setState(ProcessState state) {
    this->state = state;
}

bool Process::killProcess(const std::string& signal) {
    if (pid <= 0) {
        return false;
    }
    
    int sig = SIGTERM;
    
    if (signal == "TERM") {
        sig = SIGTERM;
    } else if (signal == "KILL") {
        sig = SIGKILL;
    } else if (signal == "INT") {
        sig = SIGINT;
    } else if (signal == "QUIT") {
        sig = SIGQUIT;
    } else if (signal == "HUP") {
        sig = SIGHUP;
    } else if (signal == "USR1") {
        sig = SIGUSR1;
    } else if (signal == "USR2") {
        sig = SIGUSR2;
    }
    
    if (kill(pid, sig) == 0) {
        return true;
    } else {
        std::cerr << "Failed to send signal " << signal << " to process " << pid 
                  << ": " << strerror(errno) << std::endl;
        return false;
    }
}
