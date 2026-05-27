#include "rest_api.hpp"
#include <sstream>
#include <iomanip>

RESTApi::RESTApi(DBManager* db) : db_manager_(db) {}

RESTApi::~RESTApi() {}

std::string RESTApi::handle_request(const std::string& method, const std::string& path, const std::string& body) {
    if (method != "GET") {
        return "{\"error\": \"Method not allowed\"}";
    }

    // Parse path
    size_t query_pos = path.find('?');
    std::string endpoint = path.substr(0, query_pos != std::string::npos ? query_pos : path.length());
    std::string query = query_pos != std::string::npos ? path.substr(query_pos + 1) : "";

    if (endpoint == "/api/stats") {
        return get_stats();
    } else if (endpoint == "/api/health") {
        return get_health();
    } else if (endpoint.substr(0, 21) == "/api/torrent/") {
        std::string info_hash = endpoint.substr(21);
        return get_torrent_stats(info_hash);
    } else if (endpoint.substr(0, 16) == "/api/peers/") {
        std::string info_hash = endpoint.substr(16);
        auto params = parse_query_string(query);
        int limit = 50;
        if (params.find("limit") != params.end()) {
            limit = std::stoi(params["limit"]);
        }
        return get_peers_list(info_hash, limit);
    }

    return "{\"error\": \"Not found\"}";
}

std::string RESTApi::get_stats() {
    std::ostringstream json;
    json << "{"
         << "\"complete\":" << db_manager_->get_complete_count() << ","
         << "\"incomplete\":" << db_manager_->get_incomplete_count() << ","
         << "\"downloaded\":" << db_manager_->get_downloaded_count()
         << "}";
    return json.str();
}

std::string RESTApi::get_torrent_stats(const std::string& info_hash) {
    TorrentInfo info = db_manager_->get_torrent_info(info_hash);
    
    std::ostringstream json;
    json << "{"
         << "\"info_hash\":\"" << json_escape(info.info_hash) << "\","
         << "\"complete\":" << info.complete << ","
         << "\"incomplete\":" << info.incomplete << ","
         << "\"downloaded\":" << info.downloaded
         << "}";
    return json.str();
}

std::string RESTApi::get_peers_list(const std::string& info_hash, int limit) {
    // Get some peers (note: this is a simplified example)
    std::vector<PeerInfo> peers = db_manager_->get_peers("", limit);

    std::ostringstream json;
    json << "{\"peers\":[";
    
    for (size_t i = 0; i < peers.size(); ++i) {
        if (i > 0) json << ",";
        json << "{"
             << "\"peer_id\":\"" << json_escape(peers[i].peer_id) << "\","
             << "\"ip\":\"" << json_escape(peers[i].ip) << "\","
             << "\"port\":" << peers[i].port
             << "}";
    }

    json << "]}";
    return json.str();
}

std::string RESTApi::get_health() {
    return "{\"status\":\"ok\"}";
}

std::string RESTApi::json_escape(const std::string& str) {
    std::ostringstream oss;
    for (char c : str) {
        switch (c) {
            case '"': oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b"; break;
            case '\f': oss << "\\f"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:
                if (c < 32) {
                    oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                } else {
                    oss << c;
                }
        }
    }
    return oss.str();
}

std::map<std::string, std::string> RESTApi::parse_query_string(const std::string& query) {
    std::map<std::string, std::string> params;
    std::istringstream iss(query);
    std::string pair;

    while (std::getline(iss, pair, '&')) {
        size_t eq_pos = pair.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = pair.substr(0, eq_pos);
            std::string value = pair.substr(eq_pos + 1);
            params[key] = value;
        }
    }

    return params;
}