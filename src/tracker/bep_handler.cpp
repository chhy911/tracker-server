#include "bep_handler.hpp"
#include "../utils/logger.hpp"
#include <sstream>
#include <algorithm>
#include <curl/curl.h>

BEPHandler::BEPHandler() {}

BEPHandler::~BEPHandler() {}

std::string BEPHandler::handle_request(const std::string& request, DBManager* db) {
    try {
        // Extract query string from HTTP request
        size_t query_pos = request.find('?');
        if (query_pos == std::string::npos) {
            return "HTTP/1.1 400 Bad Request\r\n\r\n";
        }

        size_t space_pos = request.find(' ', query_pos);
        std::string query_string = request.substr(query_pos + 1, space_pos - query_pos - 1);

        // Parse announce request
        PeerInfo peer = parse_announce_request(query_string);

        // Update peer info in database
        if (!db->update_peer(peer)) {
            LOG_ERROR("Failed to update peer in database");
            return "HTTP/1.1 500 Internal Server Error\r\n\r\n";
        }

        // Get peer list for response
        std::vector<PeerInfo> peers = db->get_peers(peer.peer_id, NUM_WANT);

        AnnounceResponse response;
        response.interval = ANNOUNCE_INTERVAL;
        response.min_interval = MIN_ANNOUNCE_INTERVAL;
        response.complete = db->get_complete_count();
        response.incomplete = db->get_incomplete_count();
        response.downloaded = db->get_downloaded_count();
        response.peers = peers;

        std::string bencoded = bencode_response(response);

        // Build HTTP response
        std::ostringstream http_response;
        http_response << "HTTP/1.1 200 OK\r\n";
        http_response << "Content-Type: application/x-www-form-urlencoded\r\n";
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

PeerInfo BEPHandler::parse_announce_request(const std::string& query) {
    PeerInfo peer;
    
    // Simple URL parameter parsing
    std::istringstream iss(query);
    std::string pair;

    while (std::getline(iss, pair, '&')) {
        size_t eq_pos = pair.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = pair.substr(0, eq_pos);
            std::string value = pair.substr(eq_pos + 1);

            // URL decode
            CURL* curl = curl_easy_init();
            int decode_len;
            char* decode_value = curl_easy_unescape(curl, value.c_str(), value.length(), &decode_len);
            value = std::string(decode_value);
            curl_free(decode_value);
            curl_easy_cleanup(curl);

            if (key == "peer_id") peer.peer_id = value;
            else if (key == "ip") peer.ip = value;
            else if (key == "port") peer.port = std::stoi(value);
            else if (key == "uploaded") peer.uploaded = std::stoll(value);
            else if (key == "downloaded") peer.downloaded = std::stoll(value);
            else if (key == "left") peer.left = std::stoll(value);
            else if (key == "event") peer.event = value;
        }
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
    
    // Encode peers as compact format
    oss << response.peers.size() * 6 << ":";
    for (const auto& peer : response.peers) {
        // Compact format: 4 bytes IP + 2 bytes port
        struct in_addr addr;
        inet_aton(peer.ip.c_str(), &addr);
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
    std::map<std::string, std::string> result;
    // Implementation for bencode parsing
    return result;
}