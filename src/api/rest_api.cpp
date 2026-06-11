#include "rest_api.hpp"
#include "../tracker/bep_handler.hpp"
#include "../utils/sql_util.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>

RESTApi::RESTApi(DBManager* db) : db_manager_(db) {}
RESTApi::~RESTApi() {}

namespace {
bool starts_with(const std::string& str, const std::string& prefix) {
    return str.size() >= prefix.size() &&
           std::equal(prefix.begin(), prefix.end(), str.begin());
}
}  // namespace

int RESTApi::param_int(const std::map<std::string, std::string>& params,
                       const std::string& key, int default_val, int min_val, int max_val) {
    auto it = params.find(key);
    if (it == params.end()) return default_val;
    try {
        int v = std::stoi(it->second);
        return std::max(min_val, std::min(max_val, v));
    } catch (...) {
        return default_val;
    }
}

std::string RESTApi::handle_request(const std::string& method,
                                    const std::string& path,
                                    const std::string& body) {
    (void)body;
    if (method != "GET") {
        return "{\"error\":\"Method not allowed\"}";
    }

    size_t query_pos = path.find('?');
    std::string endpoint = path.substr(0, query_pos != std::string::npos ? query_pos : path.size());
    std::string query    = query_pos != std::string::npos ? path.substr(query_pos + 1) : "";
    auto params = parse_query_string(query);

    try {
        if (endpoint == "/api/stats") {
            return get_stats();
        } else if (endpoint == "/api/torrents") {
            int limit  = param_int(params, "limit",  50, 1, 200);
            int page   = param_int(params, "page",    1, 1, 10000);
            int offset = (page - 1) * limit;
            // also support raw offset param
            if (params.count("offset")) {
                offset = param_int(params, "offset", 0, 0, 1000000);
            }
            return get_torrents(limit, offset);
        } else if (endpoint == "/api/health") {
            return get_health();
        } else if (starts_with(endpoint, "/api/torrent/")) {
            std::string info_hash = endpoint.substr(13);
            if (!sql_util::is_valid_info_hash(info_hash)) {
                return "{\"error\":\"Invalid info_hash\"}";
            }
            return get_torrent_stats(info_hash);
        } else if (starts_with(endpoint, "/api/peers/")) {
            std::string info_hash = endpoint.substr(11);
            if (!sql_util::is_valid_info_hash(info_hash)) {
                return "{\"error\":\"Invalid info_hash\"}";
            }
            int limit = param_int(params, "limit", 50, 1, 500);
            return get_peers_list(info_hash, limit);
        }
    } catch (const std::exception& e) {
        return "{\"error\":\"Internal server error: " + json_escape(e.what()) + "\"}";
    }

    return "{\"error\":\"Not found\"}";
}

std::string RESTApi::get_stats() {
    std::ostringstream json;
    int seeders  = db_manager_->get_active_seeder_count();
    int leechers = db_manager_->get_active_leecher_count();
    json << "{"
         << "\"complete\":"        << db_manager_->get_complete_count() << ","
         << "\"incomplete\":"      << db_manager_->get_incomplete_count() << ","
         << "\"downloaded\":"      << db_manager_->get_downloaded_count() << ","
         << "\"torrent_count\":"   << db_manager_->get_torrent_count() << ","
         << "\"peer_count\":"      << db_manager_->get_peer_count() << ","
         << "\"active_seeders\":"  << seeders << ","
         << "\"active_leechers\":" << leechers
         << "}";
    return json.str();
}

std::string RESTApi::get_torrents(int limit, int offset) {
    return db_manager_->get_torrents_list_json(limit, offset);
}

std::string RESTApi::get_torrent_stats(const std::string& info_hash) {
    TorrentInfo info = db_manager_->get_torrent_info(info_hash);
    std::ostringstream json;
    json << "{"
         << "\"info_hash\":\""  << json_escape(info.info_hash) << "\","
         << "\"name\":\""       << json_escape(info.name) << "\","
         << "\"complete\":"     << info.complete << ","
         << "\"incomplete\":"   << info.incomplete << ","
         << "\"downloaded\":"   << info.downloaded << ","
         << "\"updated_at\":\"" << json_escape(info.updated_at) << "\""
         << "}";
    return json.str();
}

std::string RESTApi::get_peers_list(const std::string& info_hash, int limit) {
    std::vector<PeerInfo> peers = db_manager_->get_peers(info_hash, "", limit);

    std::ostringstream json;
    json << "{\"info_hash\":\"" << json_escape(info_hash) << "\","
         << "\"count\":" << peers.size() << ","
         << "\"peers\":[";

    for (size_t i = 0; i < peers.size(); ++i) {
        if (i > 0) json << ",";
        json << "{"
             << "\"peer_id\":\"" << json_escape(peers[i].peer_id) << "\","
             << "\"ip\":\""      << json_escape(peers[i].ip) << "\","
             << "\"port\":"      << peers[i].port
             << "}";
    }
    json << "]}";
    return json.str();
}

std::string RESTApi::get_health() {
    bool db_ok = db_manager_->is_alive();
    std::ostringstream json;
    json << "{"
         << "\"status\":\""    << (db_ok ? "ok" : "degraded") << "\","
         << "\"db\":\""        << (db_ok ? "ok" : "error") << "\""
         << "}";
    return json.str();
}

std::string RESTApi::json_escape(const std::string& str) {
    std::ostringstream oss;
    for (char c : str) {
        switch (c) {
            case '"':  oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b";  break;
            case '\f': oss << "\\f";  break;
            case '\n': oss << "\\n";  break;
            case '\r': oss << "\\r";  break;
            case '\t': oss << "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 32) {
                    oss << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(static_cast<unsigned char>(c));
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
        size_t eq = pair.find('=');
        if (eq != std::string::npos) {
            params[pair.substr(0, eq)] = pair.substr(eq + 1);
        }
    }
    return params;
}
