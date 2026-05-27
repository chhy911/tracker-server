#include "config.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

Config::Config() {}

Config::~Config() {}

bool Config::load(const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    std::string current_section;

    while (std::getline(file, line)) {
        // Remove comments
        size_t comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }

        // Trim whitespace
        line = trim(line);

        // Skip empty lines
        if (line.empty()) {
            continue;
        }

        // Check for section header [section]
        if (line[0] == '[' && line[line.length() - 1] == ']') {
            current_section = line.substr(1, line.length() - 2);
            current_section = trim(current_section);
            std::transform(current_section.begin(), current_section.end(), 
                         current_section.begin(), ::tolower);
            continue;
        }

        // Parse key=value
        size_t eq_pos = line.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = trim(line.substr(0, eq_pos));
            std::string value = trim(line.substr(eq_pos + 1));

            // Convert key to lowercase
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);

            // Create full key with section prefix
            std::string full_key = current_section + "." + key;

            config_map_[full_key] = value;
        }
    }

    file.close();
    return true;
}

std::string Config::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";

    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, (end - start + 1));
}

std::string Config::get_section_prefix(const std::string& line) {
    if (line[0] == '[' && line[line.length() - 1] == ']') {
        return line.substr(1, line.length() - 2);
    }
    return "";
}