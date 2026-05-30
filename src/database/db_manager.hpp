#ifndef DB_MANAGER_HPP
#define DB_MANAGER_HPP

#include <string>
#include <vector>
#include <memory>
#include <mysql/mysql.h>
#include "connection_pool.hpp"

struct TorrentInfo {
    std::string info_hash;
    int complete;
    int incomplete;
    int downloaded;
};

struct PeerRecord {
    std::string peer_id;
    std::string info_hash;
    std::string ip;
    int port;
    long long uploaded;
    long long downloaded;
    long long left;
    std::string event;
    long long timestamp;
};

// Forward declaration
struct PeerInfo;

class DBManager {
public:
    DBManager();
    ~DBManager();

    void configure(const std::string& host, int port, const std::string& user,
                   const std::string& password, const std::string& database,
                   int pool_size = DEFAULT_POOL_SIZE);

    bool connect();
    void disconnect();
    
    // Peer management
    bool update_peer(const PeerInfo& peer);
    bool add_peer(const PeerInfo& peer, const std::string& info_hash);
    bool remove_peer(const std::string& peer_id, const std::string& info_hash);
    std::vector<PeerInfo> get_peers(const std::string& exclude_peer_id, int limit);
    
    // Torrent statistics
    bool update_torrent_stats(const std::string& info_hash, int complete, int incomplete);
    TorrentInfo get_torrent_info(const std::string& info_hash);
    
    // Statistics
    int get_complete_count();
    int get_incomplete_count();
    int get_downloaded_count();
    
    // Database maintenance
    bool cleanup_old_peers(int max_age_seconds = 3600);
    bool create_tables();

private:
    std::unique_ptr<ConnectionPool> connection_pool_;
    std::string host_;
    int port_;
    std::string user_;
    std::string password_;
    std::string database_;
    int pool_size_;

    static const int DEFAULT_POOL_SIZE = 50;
};

#endif // DB_MANAGER_HPP