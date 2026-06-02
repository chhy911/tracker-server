#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <map>
#include <memory>
#include <type_traits>

class Config {
public:
    Config();
    ~Config();

    bool load(const std::string& config_file);

    template<typename T>
    T get(const std::string& key, const T& default_value) const {
        auto it = config_map_.find(key);
        if (it == config_map_.end()) {
            return default_value;
        }
        
        try {
            if constexpr (std::is_same_v<T, std::string>) {
                return it->second;
            } else if constexpr (std::is_same_v<T, int>) {
                return std::stoi(it->second);
            } else if constexpr (std::is_same_v<T, double>) {
                return std::stod(it->second);
            } else if constexpr (std::is_same_v<T, bool>) {
                return (it->second == "true" || it->second == "1");
            }
        } catch (...) {
            return default_value;
        }
        
        return default_value;
    }

    std::string get(const std::string& key, const std::string& default_value = "") const {
        auto it = config_map_.find(key);
        if (it == config_map_.end()) {
            return default_value;
        }
        return it->second;
    }

private:
    std::map<std::string, std::string> config_map_;

    std::string trim(const std::string& str);
    std::string get_section_prefix(const std::string& line);
};

#endif // CONFIG_HPP