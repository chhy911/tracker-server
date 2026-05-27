#ifndef CONNECTION_POOL_HPP
#define CONNECTION_POOL_HPP

#include <mysql/mysql.h>
#include <queue>
#include <mutex>
#include <memory>
#include <condition_variable>
#include <string>

class DBConnection {
public:
    DBConnection();
    ~DBConnection();

    MYSQL* get_mysql() { return mysql_; }
    bool connect(const std::string& host, int port, const std::string& user,
                 const std::string& password, const std::string& database);
    void disconnect();
    bool is_connected() const;

private:
    MYSQL* mysql_;
};

class ConnectionPool {
public:
    ConnectionPool(int pool_size);
    ~ConnectionPool();

    bool initialize(const std::string& host, int port, const std::string& user,
                    const std::string& password, const std::string& database);
    
    std::shared_ptr<DBConnection> get_connection();
    void return_connection(std::shared_ptr<DBConnection> conn);
    void close_all();

private:
    int pool_size_;
    std::queue<std::shared_ptr<DBConnection>> available_connections_;
    std::mutex mutex_;
    std::condition_variable condition_;
    
    std::string host_;
    int port_;
    std::string user_;
    std::string password_;
    std::string database_;
};

#endif // CONNECTION_POOL_HPP