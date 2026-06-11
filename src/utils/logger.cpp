#include "logger.hpp"
#include <iostream>
#include <iomanip>
#include <cstring>
#include <algorithm>
#include <cstdio>   // rename, remove
#include <sstream>
#include <sys/stat.h>

Logger::Logger() : min_level_(LOG_LEVEL_INFO) {}

Logger::~Logger() {
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

void Logger::init(const std::string& log_file, const std::string& level_str,
                  long max_size_mb, int backup_count) {
    std::lock_guard<std::mutex> lock(mutex_);
    min_level_      = parse_level(level_str);
    log_file_path_  = log_file;
    max_size_bytes_ = max_size_mb * 1024L * 1024L;
    backup_count_   = std::max(0, backup_count);

    log_file_.open(log_file_path_, std::ios::app);
    if (!log_file_.is_open()) {
        std::cerr << "Failed to open log file: " << log_file_path_ << std::endl;
    }
}

void Logger::rotate_if_needed() {
    // Caller must hold mutex_
    if (!log_file_.is_open() || max_size_bytes_ <= 0) return;

    // Check current file size
    struct stat st{};
    if (stat(log_file_path_.c_str(), &st) != 0) return;
    if (st.st_size < max_size_bytes_) return;

    log_file_.close();

    // Rotate: delete oldest, shift others
    for (int i = backup_count_; i >= 1; --i) {
        std::string older = log_file_path_ + "." + std::to_string(i);
        std::string newer = (i == 1) ? log_file_path_
                                     : log_file_path_ + "." + std::to_string(i - 1);
        if (i == backup_count_) {
            std::remove(older.c_str());
        }
        std::rename(newer.c_str(), older.c_str());
    }

    log_file_.open(log_file_path_, std::ios::trunc);
    if (!log_file_.is_open()) {
        std::cerr << "Failed to reopen log file after rotation: " << log_file_path_ << std::endl;
    }
}

void Logger::log(LogLevel level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    log(level, format, args);
    va_end(args);
}

void Logger::log(LogLevel level, const char* format, va_list args) {
    if (level < min_level_) return;

    std::lock_guard<std::mutex> lock(mutex_);

    char buffer[8192];
    vsnprintf(buffer, sizeof(buffer), format, args);

    std::string timestamp  = get_timestamp();
    std::string level_name = get_level_name(level);
    std::string message    = "[" + timestamp + "] [" + level_name + "] " + buffer;

    rotate_if_needed();

    if (log_file_.is_open()) {
        log_file_ << message << "\n";
        log_file_.flush();
    }

    if (level >= LOG_LEVEL_ERROR) {
        std::cerr << message << "\n";
    } else if (level >= LOG_LEVEL_WARN) {
        std::cout << message << "\n";
    }
}

std::string Logger::get_level_name(LogLevel level) const {
    switch (level) {
        case LOG_LEVEL_DEBUG:    return "DEBUG";
        case LOG_LEVEL_INFO:     return "INFO";
        case LOG_LEVEL_WARN:     return "WARN";
        case LOG_LEVEL_ERROR:    return "ERROR";
        case LOG_LEVEL_CRITICAL: return "CRITICAL";
        default:                 return "UNKNOWN";
    }
}

std::string Logger::get_timestamp() const {
    auto now = std::time(nullptr);
    auto tm  = *std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

LogLevel Logger::parse_level(const std::string& level_str) {
    std::string lower = level_str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower == "debug")    return LOG_LEVEL_DEBUG;
    if (lower == "info")     return LOG_LEVEL_INFO;
    if (lower == "warn")     return LOG_LEVEL_WARN;
    if (lower == "error")    return LOG_LEVEL_ERROR;
    if (lower == "critical") return LOG_LEVEL_CRITICAL;
    return LOG_LEVEL_INFO;
}
