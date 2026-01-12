#include "common/config_parser.h"
#include <iostream>
#include <algorithm>

namespace mdfh {

bool ConfigParser::load(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open config file: " << filepath << std::endl;
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // Parse key = value
        size_t eq_pos = line.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = trim(line.substr(0, eq_pos));
            std::string value = trim(line.substr(eq_pos + 1));
            config_map_[key] = value;
        }
    }
    
    return true;
}

std::string ConfigParser::get_string(const std::string& key, const std::string& default_value) const {
    auto it = config_map_.find(key);
    return (it != config_map_.end()) ? it->second : default_value;
}

int ConfigParser::get_int(const std::string& key, int default_value) const {
    auto it = config_map_.find(key);
    if (it != config_map_.end()) {
        try {
            return std::stoi(it->second);
        } catch (...) {
            return default_value;
        }
    }
    return default_value;
}

double ConfigParser::get_double(const std::string& key, double default_value) const {
    auto it = config_map_.find(key);
    if (it != config_map_.end()) {
        try {
            return std::stod(it->second);
        } catch (...) {
            return default_value;
        }
    }
    return default_value;
}

bool ConfigParser::get_bool(const std::string& key, bool default_value) const {
    auto it = config_map_.find(key);
    if (it != config_map_.end()) {
        std::string val = it->second;
        std::transform(val.begin(), val.end(), val.begin(), ::tolower);
        return (val == "true" || val == "1" || val == "yes");
    }
    return default_value;
}

bool ConfigParser::has_key(const std::string& key) const {
    return config_map_.find(key) != config_map_.end();
}

std::string ConfigParser::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

} // namespace mdfh
