#ifndef BEP_HANDLER_HPP
#define BEP_HANDLER_HPP

#include <string>
#include <map>
#include <memory>
#include "../database/db_manager.hpp"

struct PeerInfo {
    std::string peer_id;
    std::string ip;
    int port;
    long long uploaded;
    long long downloaded;
    long long left;
    std::string event; // "started", "stopped", "completed"
};

struct AnnounceResponse {
    int interval;
    int min_interval;
    int complete;
    int incomplete;
    int downloaded;
    std::vector<PeerInfo> peers;
};

class BEPHandler {
public:
    BEPHandler();
    ~BEPHandler();

    // Handle incoming BEP request
    std::string handle_request(const std::string& request, DBManager* db);

    // Parse announce query
    PeerInfo parse_announce_request(const std::string& query);

    // Generate bencoded response
    std::string bencode_response(const AnnounceResponse& response);

    // Parse bencoded data
    static std::map<std::string, std::string> parse_bencode_dict(const std::string& data);

private:
    std::string bencode_string(const std::string& str);
    std::string bencode_integer(long long num);
    std::string bencode_list(const std::vector<PeerInfo>& peers);

    static const int ANNOUNCE_INTERVAL = 1800;  // 30 minutes
    static const int MIN_ANNOUNCE_INTERVAL = 600; // 10 minutes
    static const int NUM_WANT = 50;
};

#endif // BEP_HANDLER_HPP