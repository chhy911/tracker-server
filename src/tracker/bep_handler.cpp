#include "bep_handler.hpp"
#include "../utils/logger.hpp"
#include "../utils/sql_util.hpp"
#include <sstream>
#include <algorithm>
#include <iomanip>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

namespace {

int hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string url_decode(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '%' && i + 2 < in.size()) {
            int hi = hex_nibble(in[i + 1]);
            int lo = hex_nibble(in[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        if (in[i] == '+') {
            out.push_back(' ');
        } else {
            out.push_back(in[i]);
        }
    }
    return out;
}

// Split query string into key=value pairs
std::map<std::string, std::vector<std::string>> parse_qs_multi(const std::string& qs) {
    std::map<std::string, std::vector<std::string>> result;
    std::istringstream iss(qs);
    std::string pair;
    while (std::getline(iss, pair, '&')) {
        size_t eq = pair.find('=');
        if (eq == std::string::npos) continue;
        std::string key = url_decode(pair.substr(0, eq));
        std::string val = url_decode(pair.substr(eq + 1));
        result[key].push_back(val);
    }
    return result;
}

}  // namespace

BEPHandler::BEPHandler() {}
BEPHandler::~BEPHandler() {}

void BEPHandler::configure(int announce_interval, int min_announce_interval,
                            int num_want, int max_peer_age, int cleanup_interval) {
    announce_interval_ = announce_interval;
    min_announce_interval_ = min_announce_interval;
    num_want_ = num_want;
    max_peer_age_ = max_peer_age;
    cleanup_interval_ = cleanup_interval;
}

std::string BEPHandler::bytes_to_hex(const std::string& raw) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (unsigned char c : raw) {
        oss << std::setw(2) << static_cast<int>(c);
    }
    return oss.str();
}

std::string BEPHandler::sql_escape(const std::string& value) {
    return sql_util::escape(value);
}

std::string BEPHandler::handle_request(const std::string& request, DBManager* db,
                                       const std::string& client_ip) {
    try {
        // Extract first line to determine path
        size_t first_space = request.find(' ');
        size_t second_space = request.find(' ', first_space + 1);
        if (first_space == std::string::npos || second_space == std::string::npos) {
            return "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        }
        std::string path = request.substr(first_space + 1, second_space - first_space - 1);

        // Split path and query string
        size_t q = path.find('?');
        std::string route = (q == std::string::npos) ? path : path.substr(0, q);
        std::string qs    = (q == std::string::npos) ? "" : path.substr(q + 1);

        if (route == "/scrape") {
            std::string body = handle_scrape(qs, db);
            std::ostringstream resp;
            resp << "HTTP/1.1 200 OK\r\n"
                 << "Content-Type: text/plain\r\n"
                 << "Content-Length: " << body.size() << "\r\n"
                 << "Connection: close\r\n\r\n"
                 << body;
            return resp.str();
        }

        // Default: /announce
        if (qs.empty()) {
            return "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        }

        PeerInfo peer = parse_announce_request(qs, client_ip);
        if (peer.info_hash.empty() || peer.peer_id.empty()) {
            LOG_ERROR("Invalid announce: missing info_hash or peer_id");
            return "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        }

        if (peer.event == "stopped") {
            db->remove_peer(peer.peer_id, peer.info_hash);
        } else {
            if (peer.event == "completed") {
                db->increment_downloaded(peer.info_hash);
            }
            if (!db->update_peer(peer)) {
                LOG_ERROR("Failed to update peer in database");
                return "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
            }
        }

        std::vector<PeerInfo> peers = db->get_peers(peer.info_hash, peer.peer_id, num_want_);

        TorrentInfo torrent = db->get_torrent_info(peer.info_hash);
        AnnounceResponse response;
        response.interval = announce_interval_;
        response.min_interval = min_announce_interval_;
        response.complete = torrent.complete;
        response.incomplete = torrent.incomplete;
        response.downloaded = torrent.downloaded;
        response.peers = peers;

        std::string bencoded = bencode_response(response);

        std::ostringstream http_response;
        http_response << "HTTP/1.1 200 OK\r\n"
                      << "Content-Type: text/plain\r\n"
                      << "Content-Length: " << bencoded.size() << "\r\n"
                      << "Connection: close\r\n\r\n"
                      << bencoded;
        return http_response.str();

    } catch (const std::exception& e) {
        LOG_ERROR("Exception in handle_request: %s", e.what());
        return "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    }
}

std::string BEPHandler::handle_scrape(const std::string& query_string, DBManager* db) {
    auto params = parse_qs_multi(query_string);
    auto it = params.find("info_hash");

    std::vector<std::string> hashes;
    if (it != params.end()) {
        for (const auto& raw : it->second) {
            std::string hex = bytes_to_hex(raw);
            if (sql_util::is_valid_info_hash(hex)) {
                hashes.push_back(hex);
            }
        }
    }

    // If no hashes requested, return empty files dict
    std::ostringstream oss;
    oss << "d5:filesd";
    for (const auto& hash : hashes) {
        TorrentInfo info = db->get_torrent_info(hash);
        // bencode key = 20-byte raw SHA1 (convert hex back to bytes)
        std::string raw;
        for (size_t i = 0; i + 1 < hash.size(); i += 2) {
            int hi = hex_nibble(hash[i]);
            int lo = hex_nibble(hash[i + 1]);
            if (hi >= 0 && lo >= 0) {
                raw.push_back(static_cast<char>((hi << 4) | lo));
            }
        }
        oss << "20:" << raw;
        oss << "d"
            << "8:completei"   << info.complete   << "e"
            << "10:incompletei" << info.incomplete << "e"
            << "10:downloadedi" << info.downloaded << "e"
            << "e";
    }
    oss << "ee";
    return oss.str();
}

PeerInfo BEPHandler::parse_announce_request(const std::string& query,
                                            const std::string& client_ip) {
    PeerInfo peer;
    peer.ip = client_ip;

    std::string raw_info_hash;
    std::string raw_peer_id;

    std::istringstream iss(query);
    std::string pair;

    while (std::getline(iss, pair, '&')) {
        size_t eq_pos = pair.find('=');
        if (eq_pos == std::string::npos) continue;

        std::string key = pair.substr(0, eq_pos);
        std::string value = url_decode(pair.substr(eq_pos + 1));

        if (key == "info_hash") {
            raw_info_hash = value;
        } else if (key == "peer_id") {
            raw_peer_id = value;
        } else if (key == "ip") {
            peer.ip = value;
        } else if (key == "port") {
            try { peer.port = std::stoi(value); } catch (...) {}
        } else if (key == "uploaded") {
            try { peer.uploaded = std::stoll(value); } catch (...) {}
        } else if (key == "downloaded") {
            try { peer.downloaded = std::stoll(value); } catch (...) {}
        } else if (key == "left") {
            try { peer.left = std::stoll(value); } catch (...) {}
        } else if (key == "event") {
            peer.event = value;
        }
    }

    if (!raw_info_hash.empty()) peer.info_hash = bytes_to_hex(raw_info_hash);
    if (!raw_peer_id.empty())   peer.peer_id   = bytes_to_hex(raw_peer_id);

    return peer;
}

std::string BEPHandler::bencode_response(const AnnounceResponse& response) {
    // Count valid compact peers first
    std::vector<const PeerInfo*> valid_peers;
    for (const auto& p : response.peers) {
        struct in_addr addr{};
        if (inet_aton(p.ip.c_str(), &addr) != 0) {
            valid_peers.push_back(&p);
        }
    }

    std::ostringstream oss;
    oss << "d";
    oss << "8:completei"    << response.complete   << "e";
    oss << "10:downloadedi" << response.downloaded << "e";
    oss << "10:incompletei" << response.incomplete << "e";
    oss << "8:intervali"    << response.interval   << "e";
    oss << "12:min intervali" << response.min_interval << "e";
    oss << "5:peers" << (valid_peers.size() * 6) << ":";

    for (const auto* p : valid_peers) {
        struct in_addr addr{};
        inet_aton(p->ip.c_str(), &addr);
        oss << static_cast<char>((addr.s_addr >>  0) & 0xFF)
            << static_cast<char>((addr.s_addr >>  8) & 0xFF)
            << static_cast<char>((addr.s_addr >> 16) & 0xFF)
            << static_cast<char>((addr.s_addr >> 24) & 0xFF)
            << static_cast<char>((p->port >> 8) & 0xFF)
            << static_cast<char>((p->port >> 0) & 0xFF);
    }
    oss << "e";
    return oss.str();
}

std::string BEPHandler::bencode_string(const std::string& str) {
    return std::to_string(str.length()) + ":" + str;
}

std::string BEPHandler::bencode_integer(long long num) {
    return "i" + std::to_string(num) + "e";
}

std::map<std::string, std::string> BEPHandler::parse_bencode_dict(const std::string& data) {
    (void)data;
    return {};
}
