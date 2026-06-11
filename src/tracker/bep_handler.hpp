#ifndef BEP_HANDLER_HPP
#define BEP_HANDLER_HPP

#include <string>
#include <map>
#include <vector>
#include <memory>
#include "../database/db_manager.hpp"

struct PeerInfo {
    std::string info_hash;  // 40-char hex (20-byte SHA1)
    std::string peer_id;    // 40-char hex
    std::string ip;
    int port{0};
    long long uploaded{0};
    long long downloaded{0};
    long long left{0};
    std::string event; // "started", "stopped", "completed"
};

struct AnnounceResponse {
    int interval{1800};
    int min_interval{600};
    int complete{0};
    int incomplete{0};
    int downloaded{0};
    std::vector<PeerInfo> peers;
};

class BEPHandler {
public:
    BEPHandler();
    ~BEPHandler();

    /** 从 tracker.conf [tracker] 段读取的参数 */
    void configure(int announce_interval, int min_announce_interval,
                   int num_want, int max_peer_age, int cleanup_interval);

    /** Handle incoming announce/scrape request (raw HTTP request bytes) */
    std::string handle_request(const std::string& request, DBManager* db,
                               const std::string& client_ip);

    // Parse announce query string
    PeerInfo parse_announce_request(const std::string& query, const std::string& client_ip);

    // Generate bencoded announce response
    std::string bencode_response(const AnnounceResponse& response);

    // Generate bencoded scrape response for a set of info_hashes
    std::string handle_scrape(const std::string& query_string, DBManager* db);

    // Parse bencoded data (stub – kept for API compatibility)
    static std::map<std::string, std::string> parse_bencode_dict(const std::string& data);

    int announce_interval() const { return announce_interval_; }
    int min_announce_interval() const { return min_announce_interval_; }
    int num_want() const { return num_want_; }
    int max_peer_age() const { return max_peer_age_; }
    int cleanup_interval() const { return cleanup_interval_; }

private:
    static std::string bytes_to_hex(const std::string& raw);
    static std::string sql_escape(const std::string& value);
    std::string bencode_string(const std::string& str);
    std::string bencode_integer(long long num);

    int announce_interval_{1800};
    int min_announce_interval_{600};
    int num_want_{50};
    int max_peer_age_{3600};
    int cleanup_interval_{3600};
};

#endif // BEP_HANDLER_HPP
