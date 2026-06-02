#ifndef REST_API_HPP
#define REST_API_HPP

#include <string>
#include <map>
#include <memory>
#include "../database/db_manager.hpp"

class RESTApi {
public:
    RESTApi(DBManager* db);
    ~RESTApi();

    // Handle HTTP requests
    std::string handle_request(const std::string& method, const std::string& path, const std::string& body);

    // API endpoints
    std::string get_stats();
    std::string get_torrents();
    std::string get_torrent_stats(const std::string& info_hash);
    std::string get_peers_list(const std::string& info_hash, int limit = 50);
    std::string get_health();

private:
    DBManager* db_manager_;

    std::string json_escape(const std::string& str);
    std::map<std::string, std::string> parse_query_string(const std::string& query);
};

#endif // REST_API_HPP