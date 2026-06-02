#include "rest_api.hpp"
#include "../tracker/bep_handler.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>

RESTApi::RESTApi(DBManager* db) : db_manager_(db) {}

RESTApi::~RESTApi() {}

namespace {
    bool starts_with(const std::string& str, const std::string& prefix) {
        return str.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), str.begin());
    }
}

std::string RESTApi::handle_request(const std::string& method, const std::string& path, const std::string& body) {
    if (method != "GET") {
        return "{\"error\": \"Method not allowed\"}";
    }

    // Parse path
    size_t query_pos = path.find('?');
    std::string endpoint = path.substr(0, query_pos != std::string::npos ? query_pos : path.length());
    std::string query = query_pos != std::string::npos ? path.substr(query_pos + 1) : "";

    try {
        if (endpoint == "/api/stats") {
            return get_stats();
        } else if (endpoint == "/api/torrents") {
            return get_torrents();
        } else if (endpoint == "/api/health") {
            return get_health();
        } else if (starts_with(endpoint, "/api/torrent/")) {
            std::string info_hash = endpoint.substr(13); // "/api/torrent/" is 13 chars
            return get_torrent_stats(info_hash);
        } else if (starts_with(endpoint, "/api/peers/")) {
            std::string info_hash = endpoint.substr(11); // "/api/peers/" is 11 chars
            auto params = parse_query_string(query);
            int limit = 50;
            if (params.find("limit") != params.end()) {
                try {
                    limit = std::stoi(params["limit"]);
                } catch (const std::exception& e) {
                    // If limit is invalid, fallback to default or return error
                    limit = 50;
                }
            }
            return get_peers_list(info_hash, limit);
        }
    } catch (const std::exception& e) {
        return "{\"error\": \"Internal server error: " + json_escape(e.what()) + "\"}";
    }

    return "{\"error\": \"Not found\"}";
}

std::string RESTApi::get_stats() {
    std::ostringstream json;
    json << "{"
         << "\"complete\":" << db_manager_->get_complete_count() << ","
         << "\"incomplete\":" << db_manager_->get_incomplete_count() << ","
         << "\"downloaded\":" << db_manager_->get_downloaded_count() << ","
         << "\"torrent_count\":" << db_manager_->get_torrent_count() << ","
         << "\"peer_count\":" << db_manager_->get_peer_count() << ","
         << "\"active_seeders\":" << db_manager_->get_active_seeder_count() << ","
         << "\"active_leechers\":" << db_manager_->get_active_leecher_count()
         << "}";
    return json.str();
}

std::string RESTApi::get_torrents() {
    return db_manager_->get_torrents_list_json();
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
    std::vector<PeerInfo> peers = db_manager_->get_peers(info_hash, "", limit);

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
