#pragma once

#include <string>
#include <map>
#include "Process.hpp"

struct IniParserData {
    std::map<std::string, std::map<std::string, std::string>> sections;
};

class ConfigParser {
public:
    ConfigParser() = default;
    
    bool parseFile(const std::string& filename);
    std::map<std::string, ProcessConfig> getProcessConfigs() const;
    
    static int iniHandler(void* user, const char* section, const char* name, const char* value);
    
private:
    std::map<std::string, ProcessConfig> process_configs;
    
    void parseProgramSection(const std::string& section_name, const std::map<std::string, std::string>& section_data);
    std::map<std::string, std::string> parseEnvironment(const std::string& env_str);
    std::vector<int> parseExitCodes(const std::string& codes_str);
    AutoStart parseAutoStart(const std::string& value);
    AutoRestart parseAutoRestart(const std::string& value);
};
