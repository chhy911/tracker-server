#include "sql_util.hpp"
#include <cctype>

namespace sql_util {

std::string escape(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (char c : value) {
        if (c == '\'') {
            out += "\\'";
        } else if (c == '\\') {
            out += "\\\\";
        } else {
            out += c;
        }
    }
    return out;
}

static bool is_hex_string(const std::string& s, size_t expected_len) {
    if (s.size() != expected_len) {
        return false;
    }
    for (char c : s) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    return true;
}

bool is_valid_info_hash(const std::string& hash) {
    return is_hex_string(hash, 40);
}

bool is_valid_peer_id(const std::string& peer_id) {
    return is_hex_string(peer_id, 40);
}

}  // namespace sql_util
