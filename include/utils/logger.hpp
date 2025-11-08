#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <iostream>
#include <fstream>
#include <sstream>
#include <mutex>
#include <chrono>
#include <iomanip>

namespace trading {
namespace utils {

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3
};

class Logger {
public:
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    void setLogLevel(LogLevel level) {
        logLevel_ = level;
    }

    void setOutputFile(const std::string& filename) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (fileStream_.is_open()) {
            fileStream_.close();
        }
        fileStream_.open(filename, std::ios::app);
    }

    template<typename... Args>
    void debug(Args&&... args) {
        log(LogLevel::DEBUG, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void info(Args&&... args) {
        log(LogLevel::INFO, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void warn(Args&&... args) {
        log(LogLevel::WARN, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void error(Args&&... args) {
        log(LogLevel::ERROR, std::forward<Args>(args)...);
    }

private:
    Logger() : logLevel_(LogLevel::INFO) {}
    ~Logger() {
        if (fileStream_.is_open()) {
            fileStream_.close();
        }
    }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    template<typename... Args>
    void log(LogLevel level, Args&&... args) {
        if (level < logLevel_) return;

        std::lock_guard<std::mutex> lock(mutex_);
        
        std::ostringstream oss;
        oss << getTimestamp() << " [" << levelToString(level) << "] ";
        
        // Concatenate all arguments
        (oss << ... << args);
        oss << std::endl;

        std::string message = oss.str();
        
        // Output to console
        std::cout << message;
        
        // Output to file if open
        if (fileStream_.is_open()) {
            fileStream_ << message;
            fileStream_.flush();
        }
    }

    std::string getTimestamp() const {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()
        ) % 1000;

        std::ostringstream oss;
        oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
            << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return oss.str();
    }

    const char* levelToString(LogLevel level) const {
        switch (level) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO:  return "INFO ";
            case LogLevel::WARN:  return "WARN ";
            case LogLevel::ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }

    LogLevel logLevel_;
    std::ofstream fileStream_;
    std::mutex mutex_;
};

// Convenience macros
#define LOG_DEBUG(...) trading::utils::Logger::getInstance().debug(__VA_ARGS__)
#define LOG_INFO(...)  trading::utils::Logger::getInstance().info(__VA_ARGS__)
#define LOG_WARN(...)  trading::utils::Logger::getInstance().warn(__VA_ARGS__)
#define LOG_ERROR(...) trading::utils::Logger::getInstance().error(__VA_ARGS__)

} // namespace utils
} // namespace trading

#endif // LOGGER_HPP