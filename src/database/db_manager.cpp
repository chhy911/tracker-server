#include "db_manager.hpp"
#include "../tracker/bep_handler.hpp"
#include "../utils/logger.hpp"
#include "../utils/sql_util.hpp"
#include <algorithm>
#include <sstream>

DBManager::DBManager()
    : host_("localhost"),
      port_(3306),
      user_("tracker"),
      password_("tracker_password"),
      database_("tracker_db"),
      pool_size_(DEFAULT_POOL_SIZE) {
    connection_pool_ = std::make_unique<ConnectionPool>(pool_size_);
}

void DBManager::configure(const std::string& host, int port, const std::string& user,
                          const std::string& password, const std::string& database,
                          int pool_size) {
    host_ = host;
    port_ = port;
    user_ = user;
    password_ = password;
    database_ = database;
    pool_size_ = pool_size;
    connection_pool_ = std::make_unique<ConnectionPool>(pool_size_);
}

DBManager::~DBManager() {
    disconnect();
}

bool DBManager::connect() {
    if (!connection_pool_->initialize(host_, port_, user_, password_, database_)) {
        LOG_ERROR("Failed to initialize connection pool");
        return false;
    }

    // Create tables if needed
    if (!create_tables()) {
        LOG_ERROR("Failed to create database tables");
        return false;
    }

    LOG_INFO("Database connected successfully");
    return true;
}

void DBManager::disconnect() {
    if (connection_pool_) {
        connection_pool_->close_all();
    }
}

bool DBManager::is_alive() {
    auto conn = connection_pool_->get_connection();
    if (!conn) return false;
    bool ok = (mysql_ping(conn->get_mysql()) == 0);
    connection_pool_->return_connection(conn);
    return ok;
}

bool DBManager::create_tables() {
    auto conn = connection_pool_->get_connection();
    if (!conn) {
        LOG_ERROR("Failed to get database connection");
        return false;
    }

    const char* statements[] = {
        R"(CREATE TABLE IF NOT EXISTS torrents (
            id INT AUTO_INCREMENT PRIMARY KEY,
            info_hash VARCHAR(40) UNIQUE NOT NULL,
            name VARCHAR(255),
            complete INT DEFAULT 0,
            incomplete INT DEFAULT 0,
            downloaded INT DEFAULT 0,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
            INDEX idx_hash (info_hash)
        ))",
        R"(CREATE TABLE IF NOT EXISTS peers (
            id INT AUTO_INCREMENT PRIMARY KEY,
            peer_id VARCHAR(40) NOT NULL,
            torrent_id INT NOT NULL,
            info_hash VARCHAR(40) NOT NULL,
            ip VARCHAR(45) NOT NULL,
            port INT NOT NULL,
            uploaded BIGINT DEFAULT 0,
            downloaded BIGINT DEFAULT 0,
            left_to_download BIGINT DEFAULT 0,
            event VARCHAR(20),
            last_seen TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
            FOREIGN KEY (torrent_id) REFERENCES torrents(id) ON DELETE CASCADE,
            UNIQUE KEY unique_peer (torrent_id, peer_id),
            INDEX idx_hash (info_hash),
            INDEX idx_last_seen (last_seen)
        ))",
        R"(CREATE TABLE IF NOT EXISTS statistics (
            id INT AUTO_INCREMENT PRIMARY KEY,
            total_requests BIGINT DEFAULT 0,
            total_bytes_uploaded BIGINT DEFAULT 0,
            total_bytes_downloaded BIGINT DEFAULT 0,
            active_peers INT DEFAULT 0,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
        ))",
    };

    for (const char* sql : statements) {
        if (mysql_query(conn->get_mysql(), sql) != 0) {
            LOG_ERROR("Failed to create tables: %s", mysql_error(conn->get_mysql()));
            connection_pool_->return_connection(conn);
            return false;
        }
    }

    connection_pool_->return_connection(conn);
    return true;
}

bool DBManager::update_peer(const PeerInfo& peer) {
    if (!sql_util::is_valid_info_hash(peer.info_hash) ||
        !sql_util::is_valid_peer_id(peer.peer_id) ||
        peer.ip.empty() || peer.port <= 0) {
        LOG_ERROR("Invalid peer data for update");
        return false;
    }

    auto conn = connection_pool_->get_connection();
    if (!conn) return false;

    const std::string hash_esc = sql_util::escape(peer.info_hash);

    try {
        std::ostringstream torrent_sql;
        torrent_sql << "INSERT INTO torrents (info_hash) VALUES ('" << hash_esc << "') "
                    << "ON DUPLICATE KEY UPDATE updated_at=NOW()";
        if (mysql_query(conn->get_mysql(), torrent_sql.str().c_str()) != 0) {
            LOG_ERROR("Failed to upsert torrent: %s", mysql_error(conn->get_mysql()));
            connection_pool_->return_connection(conn);
            return false;
        }

        std::ostringstream id_sql;
        id_sql << "SELECT id FROM torrents WHERE info_hash='" << hash_esc << "' LIMIT 1";
        if (mysql_query(conn->get_mysql(), id_sql.str().c_str()) != 0) {
            LOG_ERROR("Failed to lookup torrent: %s", mysql_error(conn->get_mysql()));
            connection_pool_->return_connection(conn);
            return false;
        }

        MYSQL_RES* id_res = mysql_store_result(conn->get_mysql());
        if (!id_res) {
            connection_pool_->return_connection(conn);
            return false;
        }
        MYSQL_ROW id_row = mysql_fetch_row(id_res);
        if (!id_row || !id_row[0]) {
            mysql_free_result(id_res);
            connection_pool_->return_connection(conn);
            return false;
        }
        int torrent_id = std::stoi(id_row[0]);
        mysql_free_result(id_res);

        std::ostringstream peer_sql;
        const std::string peer_esc = sql_util::escape(peer.peer_id);
        const std::string ip_esc = sql_util::escape(peer.ip);
        const std::string event_esc = sql_util::escape(peer.event);
        peer_sql << "INSERT INTO peers (peer_id, torrent_id, info_hash, ip, port, uploaded, downloaded, "
                 << "left_to_download, event) VALUES ('"
                 << peer_esc << "', " << torrent_id << ", '" << hash_esc << "', '"
                 << ip_esc << "', " << peer.port << ", " << peer.uploaded << ", "
                 << peer.downloaded << ", " << peer.left << ", '" << event_esc << "') "
                 << "ON DUPLICATE KEY UPDATE ip='" << ip_esc << "', port=" << peer.port
                 << ", uploaded=" << peer.uploaded << ", downloaded=" << peer.downloaded
                 << ", left_to_download=" << peer.left << ", event='" << event_esc
                 << "', last_seen=NOW()";

        if (mysql_query(conn->get_mysql(), peer_sql.str().c_str()) != 0) {
            LOG_ERROR("Failed to update peer: %s", mysql_error(conn->get_mysql()));
            connection_pool_->return_connection(conn);
            return false;
        }

        refresh_torrent_stats(peer.info_hash);
        connection_pool_->return_connection(conn);
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("Exception in update_peer: %s", e.what());
        connection_pool_->return_connection(conn);
        return false;
    }
}

std::vector<PeerInfo> DBManager::get_peers(const std::string& info_hash,
                                           const std::string& exclude_peer_id,
                                           int limit) {
    std::vector<PeerInfo> peers;
    if (!sql_util::is_valid_info_hash(info_hash)) {
        return peers;
    }
    auto conn = connection_pool_->get_connection();
    if (!conn) return peers;

    const std::string hash_esc = sql_util::escape(info_hash);
    const std::string exclude_esc = sql_util::escape(exclude_peer_id);

    try {
        std::ostringstream query;
        query << "SELECT peer_id, ip, port FROM peers WHERE info_hash='" << hash_esc
              << "' AND peer_id != '" << exclude_esc
              << "' ORDER BY last_seen DESC LIMIT " << std::max(1, std::min(limit, 500));

        if (mysql_query(conn->get_mysql(), query.str().c_str()) != 0) {
            LOG_ERROR("Failed to get peers: %s", mysql_error(conn->get_mysql()));
            connection_pool_->return_connection(conn);
            return peers;
        }

        MYSQL_RES* result = mysql_store_result(conn->get_mysql());
        if (!result) {
            connection_pool_->return_connection(conn);
            return peers;
        }

        MYSQL_ROW row;
        while ((row = mysql_fetch_row(result))) {
            PeerInfo peer;
            peer.peer_id = row[0] ? row[0] : "";
            peer.ip = row[1] ? row[1] : "";
            peer.port = row[2] ? std::stoi(row[2]) : 0;
            peers.push_back(peer);
        }

        mysql_free_result(result);
        connection_pool_->return_connection(conn);

    } catch (const std::exception& e) {
        LOG_ERROR("Exception in get_peers: %s", e.what());
        connection_pool_->return_connection(conn);
    }

    return peers;
}

bool DBManager::remove_peer(const std::string& peer_id, const std::string& info_hash) {
    if (!sql_util::is_valid_info_hash(info_hash) || !sql_util::is_valid_peer_id(peer_id)) {
        return false;
    }
    auto conn = connection_pool_->get_connection();
    if (!conn) return false;

    std::ostringstream query;
    query << "DELETE FROM peers WHERE peer_id='" << sql_util::escape(peer_id)
          << "' AND info_hash='" << sql_util::escape(info_hash) << "'";
    bool ok = mysql_query(conn->get_mysql(), query.str().c_str()) == 0;
    if (ok) {
        refresh_torrent_stats(info_hash);
    }
    connection_pool_->return_connection(conn);
    return ok;
}

bool DBManager::refresh_torrent_stats(const std::string& info_hash) {
    if (!sql_util::is_valid_info_hash(info_hash)) {
        return false;
    }
    auto conn = connection_pool_->get_connection();
    if (!conn) return false;

    const std::string hash_esc = sql_util::escape(info_hash);
    std::ostringstream query;
    query << "UPDATE torrents SET "
          << "complete = (SELECT COUNT(*) FROM peers WHERE info_hash='" << hash_esc
          << "' AND left_to_download = 0 AND last_seen > DATE_SUB(NOW(), INTERVAL 2 HOUR)), "
          << "incomplete = (SELECT COUNT(*) FROM peers WHERE info_hash='" << hash_esc
          << "' AND left_to_download > 0 AND last_seen > DATE_SUB(NOW(), INTERVAL 2 HOUR)) "
          << "WHERE info_hash='" << hash_esc << "'";

    bool ok = mysql_query(conn->get_mysql(), query.str().c_str()) == 0;
    connection_pool_->return_connection(conn);
    return ok;
}

int DBManager::get_torrent_count() {
    auto conn = connection_pool_->get_connection();
    if (!conn) return 0;
    int count = 0;
    if (mysql_query(conn->get_mysql(), "SELECT COUNT(*) FROM torrents") == 0) {
        MYSQL_RES* result = mysql_store_result(conn->get_mysql());
        if (result) {
            MYSQL_ROW row = mysql_fetch_row(result);
            if (row && row[0]) count = std::stoi(row[0]);
            mysql_free_result(result);
        }
    }
    connection_pool_->return_connection(conn);
    return count;
}

int DBManager::get_peer_count(int active_within_seconds) {
    auto conn = connection_pool_->get_connection();
    if (!conn) return 0;
    int count = 0;
    std::ostringstream query;
    query << "SELECT COUNT(*) FROM peers WHERE last_seen > DATE_SUB(NOW(), INTERVAL "
          << active_within_seconds << " SECOND)";
    if (mysql_query(conn->get_mysql(), query.str().c_str()) == 0) {
        MYSQL_RES* result = mysql_store_result(conn->get_mysql());
        if (result) {
            MYSQL_ROW row = mysql_fetch_row(result);
            if (row && row[0]) count = std::stoi(row[0]);
            mysql_free_result(result);
        }
    }
    connection_pool_->return_connection(conn);
    return count;
}

int DBManager::get_active_seeder_count(int active_within_seconds) {
    auto conn = connection_pool_->get_connection();
    if (!conn) return 0;
    int count = 0;
    std::ostringstream query;
    query << "SELECT COUNT(*) FROM peers WHERE left_to_download = 0 AND last_seen > DATE_SUB(NOW(), INTERVAL "
          << active_within_seconds << " SECOND)";
    if (mysql_query(conn->get_mysql(), query.str().c_str()) == 0) {
        MYSQL_RES* result = mysql_store_result(conn->get_mysql());
        if (result) {
            MYSQL_ROW row = mysql_fetch_row(result);
            if (row && row[0]) count = std::stoi(row[0]);
            mysql_free_result(result);
        }
    }
    connection_pool_->return_connection(conn);
    return count;
}

int DBManager::get_active_leecher_count(int active_within_seconds) {
    auto conn = connection_pool_->get_connection();
    if (!conn) return 0;
    int count = 0;
    std::ostringstream query;
    query << "SELECT COUNT(*) FROM peers WHERE left_to_download > 0 AND last_seen > DATE_SUB(NOW(), INTERVAL "
          << active_within_seconds << " SECOND)";
    if (mysql_query(conn->get_mysql(), query.str().c_str()) == 0) {
        MYSQL_RES* result = mysql_store_result(conn->get_mysql());
        if (result) {
            MYSQL_ROW row = mysql_fetch_row(result);
            if (row && row[0]) count = std::stoi(row[0]);
            mysql_free_result(result);
        }
    }
    connection_pool_->return_connection(conn);
    return count;
}

std::string DBManager::get_torrents_list_json(int limit, int offset) {
    auto conn = connection_pool_->get_connection();
    if (!conn) return "{\"torrents\":[],\"total\":0}";

    // Count total
    int total = 0;
    if (mysql_query(conn->get_mysql(), "SELECT COUNT(*) FROM torrents") == 0) {
        MYSQL_RES* cr = mysql_store_result(conn->get_mysql());
        if (cr) {
            MYSQL_ROW row = mysql_fetch_row(cr);
            if (row && row[0]) total = std::stoi(row[0]);
            mysql_free_result(cr);
        }
    }

    int safe_limit  = std::max(1, std::min(limit, 200));
    int safe_offset = std::max(0, offset);

    std::ostringstream query;
    query << "SELECT info_hash, COALESCE(name,'') AS name, complete, incomplete, downloaded, "
          << "DATE_FORMAT(updated_at,'%Y-%m-%d %H:%i:%s') AS updated_at "
          << "FROM torrents ORDER BY updated_at DESC "
          << "LIMIT " << safe_limit << " OFFSET " << safe_offset;

    std::ostringstream json;
    json << "{\"total\":" << total << ",\"limit\":" << safe_limit
         << ",\"offset\":" << safe_offset << ",\"torrents\":[";

    if (mysql_query(conn->get_mysql(), query.str().c_str()) == 0) {
        MYSQL_RES* result = mysql_store_result(conn->get_mysql());
        if (result) {
            bool first = true;
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(result))) {
                if (!first) json << ",";
                first = false;
                // simple JSON-escape name
                std::string name_str = row[1] ? row[1] : "";
                std::string name_esc;
                for (char c : name_str) {
                    if (c == '"') name_esc += "\\\"";
                    else if (c == '\\') name_esc += "\\\\";
                    else name_esc += c;
                }
                json << "{"
                     << "\"info_hash\":\""  << (row[0] ? row[0] : "") << "\","
                     << "\"name\":\""       << name_esc << "\","
                     << "\"complete\":"     << (row[2] ? row[2] : "0") << ","
                     << "\"incomplete\":"   << (row[3] ? row[3] : "0") << ","
                     << "\"downloaded\":"   << (row[4] ? row[4] : "0") << ","
                     << "\"updated_at\":\"" << (row[5] ? row[5] : "") << "\""
                     << "}";
            }
            mysql_free_result(result);
        }
    }

    json << "]}";
    connection_pool_->return_connection(conn);
    return json.str();
}

int DBManager::get_complete_count() {
    return get_active_seeder_count();
}

int DBManager::get_incomplete_count() {
    return get_active_leecher_count();
}

int DBManager::get_downloaded_count() {
    auto conn = connection_pool_->get_connection();
    if (!conn) return 0;

    int count = 0;
    const char* query = "SELECT SUM(downloaded) FROM torrents";

    if (mysql_query(conn->get_mysql(), query) == 0) {
        MYSQL_RES* result = mysql_store_result(conn->get_mysql());
        if (result) {
            MYSQL_ROW row = mysql_fetch_row(result);
            if (row && row[0]) {
                count = std::stoi(row[0]);
            }
            mysql_free_result(result);
        }
    }

    connection_pool_->return_connection(conn);
    return count;
}

TorrentInfo DBManager::get_torrent_info(const std::string& info_hash) {
    TorrentInfo info;
    info.info_hash = info_hash;
    if (!sql_util::is_valid_info_hash(info_hash)) {
        return info;
    }
    auto conn = connection_pool_->get_connection();
    if (!conn) return info;

    std::ostringstream query;
    query << "SELECT complete, incomplete, downloaded, COALESCE(name,''), "
          << "DATE_FORMAT(updated_at,'%Y-%m-%d %H:%i:%s') "
          << "FROM torrents WHERE info_hash = '"
          << sql_util::escape(info_hash) << "'";

    if (mysql_query(conn->get_mysql(), query.str().c_str()) == 0) {
        MYSQL_RES* result = mysql_store_result(conn->get_mysql());
        if (result) {
            MYSQL_ROW row = mysql_fetch_row(result);
            if (row) {
                info.complete   = row[0] ? std::stoi(row[0]) : 0;
                info.incomplete = row[1] ? std::stoi(row[1]) : 0;
                info.downloaded = row[2] ? std::stoi(row[2]) : 0;
                info.name       = row[3] ? row[3] : "";
                info.updated_at = row[4] ? row[4] : "";
            }
            mysql_free_result(result);
        }
    }

    connection_pool_->return_connection(conn);
    return info;
}

bool DBManager::cleanup_old_peers(int max_age_seconds) {
    auto conn = connection_pool_->get_connection();
    if (!conn) return false;

    std::ostringstream query;
    query << "DELETE FROM peers WHERE last_seen < DATE_SUB(NOW(), INTERVAL "
          << std::max(60, max_age_seconds) << " SECOND)";

    bool ok = mysql_query(conn->get_mysql(), query.str().c_str()) == 0;
    if (ok) {
        unsigned long long affected = mysql_affected_rows(conn->get_mysql());
        if (affected > 0) {
            LOG_INFO("Cleaned up %llu stale peers", (unsigned long long)affected);
        }
    }
    connection_pool_->return_connection(conn);
    return ok;
}

bool DBManager::increment_downloaded(const std::string& info_hash) {
    if (!sql_util::is_valid_info_hash(info_hash)) return false;
    auto conn = connection_pool_->get_connection();
    if (!conn) return false;

    std::ostringstream query;
    query << "UPDATE torrents SET downloaded = downloaded + 1 WHERE info_hash = '"
          << sql_util::escape(info_hash) << "'";
    bool ok = mysql_query(conn->get_mysql(), query.str().c_str()) == 0;
    connection_pool_->return_connection(conn);
    return ok;
}

bool DBManager::update_torrent_stats(const std::string& info_hash, int complete, int incomplete) {
    if (!sql_util::is_valid_info_hash(info_hash)) return false;
    auto conn = connection_pool_->get_connection();
    if (!conn) return false;

    std::ostringstream query;
    query << "UPDATE torrents SET complete=" << complete
          << ", incomplete=" << incomplete
          << " WHERE info_hash='" << sql_util::escape(info_hash) << "'";
    bool ok = mysql_query(conn->get_mysql(), query.str().c_str()) == 0;
    connection_pool_->return_connection(conn);
    return ok;
}