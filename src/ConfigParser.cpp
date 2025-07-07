#include "../include/ConfigParser.hpp"
#include "../include/ini.h"

int ConfigParser::iniHandler(void* user, const char* section, const char* name, const char* value) {
    IniParserData* data = static_cast<IniParserData*>(user);
    
    std::string section_str(section);
    std::string name_str(name);
    std::string value_str(value);
    
    data->sections[section_str][name_str] = value_str;
    
    return 1;
}

bool ConfigParser::parseFile(const std::string& filename) {
    IniParserData data;
    
    if (ini_parse(filename.c_str(), iniHandler, &data) < 0) {
        std::cerr << "Could not open config file: " << filename << std::endl;
        return false;
    }
    
    process_configs.clear();
    
    for (const auto& [section_name, section_data] : data.sections) {
        if (section_name.substr(0, 8) == "program:") {
            parseProgramSection(section_name, section_data);
        }
    }
    
    return true;
}

std::map<std::string, ProcessConfig> ConfigParser::getProcessConfigs() const {
    return process_configs;
}

void ConfigParser::parseProgramSection(const std::string& section_name, 
                                     const std::map<std::string, std::string>& section_data) {
    
    if (section_name.length() <= 8) return;
    
    std::string prog_name = section_name.substr(8);
    ProcessConfig config;
    config.name = prog_name;
    
    for (const auto& [key, value] : section_data) {
        try {
            if (key == "command") {
                config.command = value;
            } else if (key == "numprocs") {
                config.numprocs = std::stoi(value);
            } else if (key == "priority") {
                config.priority = std::stoi(value);
            } else if (key == "autostart") {
                config.autostart = parseAutoStart(value);
            } else if (key == "autorestart") {
                config.autorestart = parseAutoRestart(value);
            } else if (key == "autorestart_exit_codes" || key == "exitcodes") {
                config.autorestart_exit_codes = parseExitCodes(value);
            } else if (key == "startretries") {
                config.startretries = std::stoi(value);
            } else if (key == "starttime") {
                config.starttime = std::stoi(value);
            } else if (key == "stopsignal") {
                config.stopsignal = value;
            } else if (key == "stoptime") {
                config.stoptime = std::stoi(value);
            } else if (key == "stdout_logfile") {
                config.stdout_logfile = value;
            } else if (key == "stderr_logfile") {
                config.stderr_logfile = value;
            } else if (key == "directory") {
                config.workingdir = value;
            } else if (key == "environment") {
                config.environment = parseEnvironment(value);
            } else if (key == "umask") {
                config.umask = std::stoi(value, nullptr, 8);
            }
        } catch (const std::exception& e) {
            std::cerr << "Warning: Invalid value for " << key << " in program " << prog_name 
                      << ": " << value << " (" << e.what() << ")" << std::endl;
        }
    }
    
    if (config.command.empty()) {
        std::cerr << "Warning: Program " << prog_name << " has no command specified" << std::endl;
        return;
    }
    
    process_configs[prog_name] = config;
}

std::map<std::string, std::string> ConfigParser::parseEnvironment(const std::string& env_str) {
    std::map<std::string, std::string> env_map;
    
    std::istringstream iss(env_str);
    std::string token;
    
    while (std::getline(iss, token, ',')) {
        token.erase(0, token.find_first_not_of(" \t"));
        token.erase(token.find_last_not_of(" \t") + 1);
        
        size_t eq_pos = token.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = token.substr(0, eq_pos);
            std::string value = token.substr(eq_pos + 1);
            
            if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.length() - 2);
            }
            
            env_map[key] = value;
        }
    }
    
    return env_map;
}

std::vector<int> ConfigParser::parseExitCodes(const std::string& codes_str) {
    std::vector<int> codes;
    std::istringstream iss(codes_str);
    std::string token;
    
    while (std::getline(iss, token, ',')) {
        token.erase(0, token.find_first_not_of(" \t"));
        token.erase(token.find_last_not_of(" \t") + 1);
        
        if (!token.empty()) {
            codes.push_back(std::stoi(token));
        }
    }
    
    return codes;
}

AutoStart ConfigParser::parseAutoStart(const std::string& value) {
    std::string lower_value = value;
    std::transform(lower_value.begin(), lower_value.end(), lower_value.begin(), ::tolower);
    
    if (lower_value == "true") {
        return AutoStart::TRUE;
    } else if (lower_value == "false") {
        return AutoStart::FALSE;
    } else if (lower_value == "unexpected") {
        return AutoStart::UNEXPECTED;
    }
    
    return AutoStart::TRUE;
}

AutoRestart ConfigParser::parseAutoRestart(const std::string& value) {
    std::string lower_value = value;
    std::transform(lower_value.begin(), lower_value.end(), lower_value.begin(), ::tolower);
    
    if (lower_value == "true") {
        return AutoRestart::TRUE;
    } else if (lower_value == "false") {
        return AutoRestart::FALSE;
    } else if (lower_value == "unexpected") {
        return AutoRestart::UNEXPECTED;
    }
    
    return AutoRestart::TRUE;
}
