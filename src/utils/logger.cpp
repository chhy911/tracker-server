#include "logger.hpp"
#include <iostream>
#include <iomanip>
#include <cstring>
#include <algorithm>

Logger::Logger() : min_level_(LOG_LEVEL_INFO) {}

Logger::~Logger() {
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

void Logger::init(const std::string& log_file, const std::string& level_str) {
    std::lock_guard<std::mutex> lock(mutex_);

    min_level_ = parse_level(level_str);

    // Create logs directory if needed
    log_file_.open(log_file, std::ios::app);
    if (!log_file_.is_open()) {
        std::cerr << "Failed to open log file: " << log_file << std::endl;
    }
}

void Logger::log(LogLevel level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    log(level, format, args);
    va_end(args);
}

void Logger::log(LogLevel level, const char* format, va_list args) {
    if (level < min_level_) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Format message
    char buffer[4096];
    vsnprintf(buffer, sizeof(buffer), format, args);

    std::string timestamp = get_timestamp();
    std::string level_name = get_level_name(level);

    std::string log_message = "[" + timestamp + "] [" + level_name + "] " + buffer;

    // Write to file
    if (log_file_.is_open()) {
        log_file_ << log_message << std::endl;
        log_file_.flush();
    }

    // Also write to console for ERROR and CRITICAL
    if (level >= LOG_LEVEL_ERROR) {
        std::cerr << log_message << std::endl;
    } else if (level >= LOG_LEVEL_WARN) {
        std::cout << log_message << std::endl;
    }
}

std::string Logger::get_level_name(LogLevel level) const {
    switch (level) {
        case LOG_LEVEL_DEBUG: return "DEBUG";
        case LOG_LEVEL_INFO: return "INFO";
        case LOG_LEVEL_WARN: return "WARN";
        case LOG_LEVEL_ERROR: return "ERROR";
        case LOG_LEVEL_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

std::string Logger::get_timestamp() const {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

LogLevel Logger::parse_level(const std::string& level_str) {
    std::string lower = level_str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "debug") return LOG_LEVEL_DEBUG;
    if (lower == "info") return LOG_LEVEL_INFO;
    if (lower == "warn") return LOG_LEVEL_WARN;
    if (lower == "error") return LOG_LEVEL_ERROR;
    if (lower == "critical") return LOG_LEVEL_CRITICAL;
    
    return LOG_LEVEL_INFO; // Default
}