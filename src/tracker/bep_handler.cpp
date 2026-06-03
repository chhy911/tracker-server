#include "bep_handler.hpp"
#include "../utils/logger.hpp"
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

}  // namespace

BEPHandler::BEPHandler() {}

BEPHandler::~BEPHandler() {}

std::string BEPHandler::bytes_to_hex(const std::string& raw) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (unsigned char c : raw) {
        oss << std::setw(2) << static_cast<int>(c);
    }
    return oss.str();
}

std::string BEPHandler::sql_escape(const std::string& value) {
    std::string out;
    out.reserve(value.size());
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

std::string BEPHandler::handle_request(const std::string& request, DBManager* db,
                                       const std::string& client_ip) {
    try {
        size_t query_pos = request.find('?');
        if (query_pos == std::string::npos) {
            return "HTTP/1.1 400 Bad Request\r\n\r\n";
        }

        size_t space_pos = request.find(' ', query_pos);
        std::string query_string = request.substr(query_pos + 1, space_pos - query_pos - 1);

        PeerInfo peer = parse_announce_request(query_string, client_ip);
        if (peer.info_hash.empty() || peer.peer_id.empty()) {
            LOG_ERROR("Invalid announce: missing info_hash or peer_id");
            return "HTTP/1.1 400 Bad Request\r\n\r\n";
        }

        if (peer.event == "stopped") {
            db->remove_peer(peer.peer_id, peer.info_hash);
        } else {
            if (!db->update_peer(peer)) {
                LOG_ERROR("Failed to update peer in database");
                return "HTTP/1.1 500 Internal Server Error\r\n\r\n";
            }
        }

        std::vector<PeerInfo> peers = db->get_peers(peer.info_hash, peer.peer_id, NUM_WANT);

        TorrentInfo torrent = db->get_torrent_info(peer.info_hash);
        AnnounceResponse response;
        response.interval = ANNOUNCE_INTERVAL;
        response.min_interval = MIN_ANNOUNCE_INTERVAL;
        response.complete = torrent.complete;
        response.incomplete = torrent.incomplete;
        response.downloaded = torrent.downloaded;
        response.peers = peers;

        std::string bencoded = bencode_response(response);

        std::ostringstream http_response;
        http_response << "HTTP/1.1 200 OK\r\n";
        http_response << "Content-Type: text/plain\r\n";
        http_response << "Content-Length: " << bencoded.length() << "\r\n";
        http_response << "Connection: close\r\n";
        http_response << "\r\n";
        http_response << bencoded;

        return http_response.str();

    } catch (const std::exception& e) {
        LOG_ERROR("Exception in handle_request: %s", e.what());
        return "HTTP/1.1 500 Internal Server Error\r\n\r\n";
    }
}

PeerInfo BEPHandler::parse_announce_request(const std::string& query,
                                            const std::string& client_ip) {
    PeerInfo peer;
    peer.ip = client_ip;
    peer.port = 0;

    std::string raw_info_hash;
    std::string raw_peer_id;

    std::istringstream iss(query);
    std::string pair;

    while (std::getline(iss, pair, '&')) {
        size_t eq_pos = pair.find('=');
        if (eq_pos == std::string::npos) {
            continue;
        }

        std::string key = pair.substr(0, eq_pos);
        std::string value = pair.substr(eq_pos + 1);

        value = url_decode(value);

        if (key == "info_hash") {
            raw_info_hash = value;
        } else if (key == "peer_id") {
            raw_peer_id = value;
        } else if (key == "ip") {
            peer.ip = value;
        } else if (key == "port") {
            peer.port = std::stoi(value);
        } else if (key == "uploaded") {
            peer.uploaded = std::stoll(value);
        } else if (key == "downloaded") {
            peer.downloaded = std::stoll(value);
        } else if (key == "left") {
            peer.left = std::stoll(value);
        } else if (key == "event") {
            peer.event = value;
        }
    }

    if (!raw_info_hash.empty()) {
        peer.info_hash = bytes_to_hex(raw_info_hash);
    }
    if (!raw_peer_id.empty()) {
        peer.peer_id = bytes_to_hex(raw_peer_id);
    }

    return peer;
}

std::string BEPHandler::bencode_response(const AnnounceResponse& response) {
    std::ostringstream oss;

    oss << "d";
    oss << "8:completei" << response.complete << "e";
    oss << "10:incompletei" << response.incomplete << "e";
    oss << "8:intervali" << response.interval << "e";
    oss << "12:min intervali" << response.min_interval << "e";
    oss << "5:peers";

    oss << response.peers.size() * 6 << ":";
    for (const auto& peer : response.peers) {
        struct in_addr addr {};
        if (inet_aton(peer.ip.c_str(), &addr) == 0) {
            continue;
        }
        oss << static_cast<char>((addr.s_addr >> 0) & 0xFF);
        oss << static_cast<char>((addr.s_addr >> 8) & 0xFF);
        oss << static_cast<char>((addr.s_addr >> 16) & 0xFF);
        oss << static_cast<char>((addr.s_addr >> 24) & 0xFF);
        oss << static_cast<char>((peer.port >> 8) & 0xFF);
        oss << static_cast<char>((peer.port >> 0) & 0xFF);
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
