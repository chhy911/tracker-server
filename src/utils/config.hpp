#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <map>
#include <memory>

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
            if (std::is_same<T, std::string>::value) {
                return (T)it->second;
            } else if (std::is_same<T, int>::value) {
                return (T)std::stoi(it->second);
            } else if (std::is_same<T, double>::value) {
                return (T)std::stod(it->second);
            } else if (std::is_same<T, bool>::value) {
                return (T)(it->second == "true" || it->second == "1");
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