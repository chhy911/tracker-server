#include "connection_pool.hpp"
#include "../utils/logger.hpp"

DBConnection::DBConnection() : mysql_(nullptr) {
    mysql_ = mysql_init(nullptr);
}

DBConnection::~DBConnection() {
    disconnect();
    if (mysql_) {
        mysql_close(mysql_);
        mysql_ = nullptr;
    }
}

bool DBConnection::connect(const std::string& host, int port, const std::string& user,
                           const std::string& password, const std::string& database) {
    if (!mysql_) {
        LOG_ERROR("MySQL not initialized");
        return false;
    }

    // Set connection timeout
    unsigned int timeout = 30;
    mysql_options(mysql_, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);

    if (!mysql_real_connect(mysql_, host.c_str(), user.c_str(), password.c_str(),
                           database.c_str(), port, nullptr, 0)) {
        LOG_ERROR("Failed to connect to MySQL: %s", mysql_error(mysql_));
        return false;
    }

    // Set character set to utf8mb4
    if (mysql_set_character_set(mysql_, "utf8mb4") != 0) {
        LOG_WARN("Failed to set character set: %s", mysql_error(mysql_));
    }

    return true;
}

void DBConnection::disconnect() {
    if (mysql_) {
        mysql_close(mysql_);
        mysql_ = nullptr;
    }
}

bool DBConnection::is_connected() const {
    if (!mysql_) return false;
    
    // Try a simple ping to check connection
    return mysql_ping(mysql_) == 0;
}

ConnectionPool::ConnectionPool(int pool_size) 
    : pool_size_(pool_size), port_(3306) {}

ConnectionPool::~ConnectionPool() {
    close_all();
}

bool ConnectionPool::initialize(const std::string& host, int port, const std::string& user,
                                const std::string& password, const std::string& database) {
    host_ = host;
    port_ = port;
    user_ = user;
    password_ = password;
    database_ = database;

    // Create initial connections
    for (int i = 0; i < pool_size_; ++i) {
        auto conn = std::make_shared<DBConnection>();
        if (!conn->connect(host, port, user, password, database)) {
            LOG_ERROR("Failed to create connection %d/%d", i + 1, pool_size_);
            return false;
        }
        available_connections_.push(conn);
    }

    LOG_INFO("Connection pool initialized with %d connections", pool_size_);
    return true;
}

std::shared_ptr<DBConnection> ConnectionPool::get_connection() {
    std::unique_lock<std::mutex> lock(mutex_);
    
    // Wait if no connections available
    while (available_connections_.empty()) {
        condition_.wait(lock);
    }

    auto conn = available_connections_.front();
    available_connections_.pop();

    // Check if connection is still alive, reconnect if needed
    if (!conn->is_connected()) {
        LOG_WARN("Connection lost, reconnecting...");
        if (!conn->connect(host_, port_, user_, password_, database_)) {
            LOG_ERROR("Failed to reconnect");
            return nullptr;
        }
    }

    return conn;
}

void ConnectionPool::return_connection(std::shared_ptr<DBConnection> conn) {
    std::unique_lock<std::mutex> lock(mutex_);
    available_connections_.push(conn);
    lock.unlock();
    condition_.notify_one();
}

void ConnectionPool::close_all() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (!available_connections_.empty()) {
        auto conn = available_connections_.front();
        available_connections_.pop();
        conn->disconnect();
    }
}