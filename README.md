# TaskMaster - Process Supervisor

TaskMaster is a modern C++17 implementation of a process supervisor, similar to supervisord. It manages and monitors processes based on configuration files.

## Features

### Core Functionality ✅
- **Process Management**: Start, stop, restart processes
- **Configuration Parsing**: INI-style configuration file support
- **Auto-start**: Automatically start processes on TaskMaster startup
- **Auto-restart**: Configurable restart policies (true/false/unexpected)
- **Process Monitoring**: Health checking and automatic restart on failure
- **Signal Handling**: Graceful shutdown with SIGINT/SIGTERM
- **Logging**: Stdout/stderr redirection to log files
- **Working Directory**: Process-specific working directories
- **Environment Variables**: Custom environment for each process
- **Resource Limits**: umask support

### Advanced Features ✅
- **Interactive Shell**: Command-line interface for real-time management
- **Status Reporting**: Detailed process status with PID and uptime
- **Configuration Reload**: Hot-reload configuration without restart
- **Retry Logic**: Configurable start retry attempts
- **Graceful Shutdown**: Proper process termination with timeout
- **Multi-threading**: Concurrent process monitoring
- **Modern C++**: C++17 features, RAII, smart pointers

## Build Requirements

- C++17 compatible compiler (GCC 7+, Clang 5+)
- POSIX-compliant system (Linux, macOS)
- Make build system

## Building

```bash
# Clean build
make clean && make

# Debug build
make debug

# Install (optional)
sudo make install
```

## Configuration

TaskMaster uses INI-style configuration files. Example `taskmaster.conf`:

```ini
[program:my_app]
command=/usr/bin/my_application --config /etc/my_app.conf
autostart=true
autorestart=unexpected
startretries=3
starttime=2
stopsignal=TERM
stoptime=10
stdout_logfile=/var/log/my_app.log
stderr_logfile=/var/log/my_app_err.log
directory=/opt/my_app
umask=022
environment=PATH="/usr/local/bin:/usr/bin",HOME="/opt/my_app"
```

### Configuration Options

| Option | Description | Default |
|--------|-------------|---------|
| `command` | Command to execute | Required |
| `autostart` | Start on TaskMaster startup | `true` |
| `autorestart` | Restart policy: true/false/unexpected | `true` |
| `startretries` | Number of restart attempts | `3` |
| `starttime` | Seconds to wait before considering started | `1` |
| `stopsignal` | Signal for graceful shutdown | `TERM` |
| `stoptime` | Seconds to wait before force kill | `10` |
| `stdout_logfile` | Path for stdout logging | `/dev/null` |
| `stderr_logfile` | Path for stderr logging | `/dev/null` |
| `directory` | Working directory | `/tmp` |
| `umask` | File creation mask (octal) | `022` |
| `environment` | Environment variables | Empty |

## Usage

### Starting TaskMaster

```bash
# Use default config file (taskmaster.conf)
./taskmaster

# Use custom config file
./taskmaster /path/to/config.conf
```

### Interactive Commands

Once TaskMaster is running, you can use these commands:

- `status` - Show status of all processes
- `start <name>` - Start a specific process
- `stop <name>` - Stop a specific process  
- `restart <name>` - Restart a specific process
- `reload` - Reload configuration file
- `help` - Show available commands
- `quit` / `exit` - Exit TaskMaster

### Example Session

```
taskmaster> status
Process Status:
=====================================
web_server: RUNNING (PID: 1234, Uptime: 300s)
worker: STOPPED
database: RUNNING (PID: 1235, Uptime: 299s)

taskmaster> start worker
Started worker

taskmaster> restart web_server
Restarted web_server

taskmaster> quit
```

## Architecture

### Components

1. **TaskMaster**: Main controller class
   - Configuration management
   - Process lifecycle coordination
   - Command interface
   - Monitoring thread management

2. **Process**: Individual process management
   - Process execution (fork/exec)
   - State tracking
   - Signal handling
   - Log redirection

3. **ConfigParser**: Configuration file parsing
   - INI format parsing
   - Type conversion
   - Validation

### Process States

- `STOPPED`: Process is not running
- `STARTING`: Process is being started
- `RUNNING`: Process is running normally
- `STOPPING`: Process is being stopped
- `EXITED`: Process has exited
- `FATAL`: Process failed to start/restart
- `BACKOFF`: Process is in retry backoff (future)

### Thread Safety

TaskMaster is fully thread-safe using:
- `std::mutex` for process map protection
- `std::atomic` for process state and PID
- `std::condition_variable` for monitoring coordination

## Development

### Code Style

- Modern C++17 features
- RAII for resource management
- Smart pointers over raw pointers
- STL containers and algorithms
- Exception safety guarantees
