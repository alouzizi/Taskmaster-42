#include <iostream>
#include <memory>
#include <signal.h>
#include "../include/TaskMaster.hpp"
#include "../include/Logger.hpp"

std::unique_ptr<TaskMaster> g_taskmaster;

void signalHandler(int signum) {
    if (g_taskmaster) {
        Logger::getInstance().info("Received signal " + std::to_string(signum) + ". Shutting down TaskMaster...");
        g_taskmaster->shutdown();
    }
    exit(signum);
}

int main(int argc, char* argv[]) {
    std::string config_file = "taskmaster.conf";
    
    if (argc > 1) {
        config_file = argv[1];
    }
    
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    try {
        g_taskmaster = std::make_unique<TaskMaster>(config_file);
        
        std::cout << "TaskMaster starting..." << std::endl;
        g_taskmaster->run();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
