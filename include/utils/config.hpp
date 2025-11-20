#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <iostream>

namespace trading {
namespace utils {

/**
 * Configuration manager for loading settings from files.
 * Simple key-value configuration (can be extended to JSON/YAML).
 */
class Config {
public:
    static Config& getInstance() {
        static Config instance;
        return instance;
    }

    /**
     * Load configuration from file.
     * Format: key=value (one per line, # for comments)
     */
    bool loadFromFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Failed to open config file: " << filename << std::endl;
            return false;
        }

        std::string line;
        int lineNum = 0;

        while (std::getline(file, line)) {
            lineNum++;
            
            // Skip empty lines and comments
            if (line.empty() || line[0] == '#') continue;

            // Parse key=value
            size_t pos = line.find('=');
            if (pos == std::string::npos) {
                std::cerr << "Invalid config line " << lineNum << ": " << line << std::endl;
                continue;
            }

            std::string key = trim(line.substr(0, pos));
            std::string value = trim(line.substr(pos + 1));

            settings_[key] = value;
        }

        return true;
    }

    /**
     * Get string value.
     */
    std::string getString(const std::string& key, const std::string& defaultValue = "") const {
        auto it = settings_.find(key);
        return (it != settings_.end()) ? it->second : defaultValue;
    }

    /**
     * Get integer value.
     */
    int getInt(const std::string& key, int defaultValue = 0) const {
        auto it = settings_.find(key);
        if (it == settings_.end()) return defaultValue;
        
        try {
            return std::stoi(it->second);
        } catch (...) {
            return defaultValue;
        }
    }

    /**
     * Get double value.
     */
    double getDouble(const std::string& key, double defaultValue = 0.0) const {
        auto it = settings_.find(key);
        if (it == settings_.end()) return defaultValue;
        
        try {
            return std::stod(it->second);
        } catch (...) {
            return defaultValue;
        }
    }

    /**
     * Get boolean value.
     */
    bool getBool(const std::string& key, bool defaultValue = false) const {
        std::string value = getString(key);
        if (value.empty()) return defaultValue;
        
        // Case-insensitive comparison
        for (char& c : value) c = tolower(c);
        return (value == "true" || value == "1" || value == "yes" || value == "on");
    }

    /**
     * Set value.
     */
    void set(const std::string& key, const std::string& value) {
        settings_[key] = value;
    }

    /**
     * Check if key exists.
     */
    bool has(const std::string& key) const {
        return settings_.find(key) != settings_.end();
    }

    /**
     * Save configuration to file.
     */
    bool saveToFile(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) {
            return false;
        }

        file << "# Trading System Configuration\n";
        file << "# Generated automatically\n\n";

        for (const auto& [key, value] : settings_) {
            file << key << "=" << value << "\n";
        }

        return true;
    }

    /**
     * Print all settings.
     */
    void print() const {
        std::cout << "\n=== Configuration ===" << std::endl;
        for (const auto& [key, value] : settings_) {
            std::cout << "  " << key << " = " << value << std::endl;
        }
        std::cout << "=====================\n" << std::endl;
    }

private:
    Config() = default;
    std::unordered_map<std::string, std::string> settings_;

    static std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, last - first + 1);
    }
};

} // namespace utils
} // namespace trading

#endif // CONFIG_HPP