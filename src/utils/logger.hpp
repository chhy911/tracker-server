#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>
#include <fstream>
#include <mutex>
#include <ctime>
#include <cstdarg>

enum LogLevel {
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_CRITICAL
};

class Logger {
public:
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    void init(const std::string& log_file, const std::string& level_str,
              long max_size_mb = 100, int backup_count = 5);

    void log(LogLevel level, const char* format, va_list args);
    void log(LogLevel level, const char* format, ...);

    void set_level(LogLevel level) { min_level_ = level; }
    LogLevel get_level() const { return min_level_; }

private:
    Logger();
    ~Logger();

    void rotate_if_needed();
    std::string get_level_name(LogLevel level) const;
    std::string get_timestamp() const;
    LogLevel parse_level(const std::string& level_str);

    std::string   log_file_path_;
    std::ofstream log_file_;
    std::mutex    mutex_;
    LogLevel      min_level_;
    long          max_size_bytes_{100 * 1024 * 1024};
    int           backup_count_{5};
};

#define LOG_DEBUG(...)    Logger::getInstance().log(LOG_LEVEL_DEBUG,    __VA_ARGS__)
#define LOG_INFO(...)     Logger::getInstance().log(LOG_LEVEL_INFO,     __VA_ARGS__)
#define LOG_WARN(...)     Logger::getInstance().log(LOG_LEVEL_WARN,     __VA_ARGS__)
#define LOG_ERROR(...)    Logger::getInstance().log(LOG_LEVEL_ERROR,    __VA_ARGS__)
#define LOG_CRITICAL(...) Logger::getInstance().log(LOG_LEVEL_CRITICAL, __VA_ARGS__)

#endif // LOGGER_HPP
