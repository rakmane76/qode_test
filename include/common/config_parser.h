#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include <string>
#include <map>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace mdfh {

class ConfigParser {
public:
    ConfigParser() = default;
    
    // Load configuration from file
    bool load(const std::string& filepath);
    
    // Get string value
    std::string get_string(const std::string& key, const std::string& default_value = "") const;
    
    // Get integer value
    int get_int(const std::string& key, int default_value = 0) const;
    
    // Get double value
    double get_double(const std::string& key, double default_value = 0.0) const;
    
    // Get boolean value
    bool get_bool(const std::string& key, bool default_value = false) const;
    
    // Check if key exists
    bool has_key(const std::string& key) const;
    
private:
    std::map<std::string, std::string> config_map_;
    
    // Trim whitespace
    static std::string trim(const std::string& str);
};

} // namespace mdfh

#endif // CONFIG_PARSER_H
