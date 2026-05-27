#ifndef TRACKER_SERVER_HPP
#define TRACKER_SERVER_HPP

#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <vector>
#include "bep_handler.hpp"
#include "../database/db_manager.hpp"

using boost::asio::ip::tcp;

class TrackerSession : public boost::enable_shared_from_this<TrackerSession> {
public:
    typedef boost::shared_ptr<TrackerSession> pointer;

    static pointer create(boost::asio::io_context& io_context, DBManager* db) {
        return pointer(new TrackerSession(io_context, db));
    }

    tcp::socket& socket() { return socket_; }

    void start();

private:
    TrackerSession(boost::asio::io_context& io_context, DBManager* db)
        : socket_(io_context), db_manager_(db) {}

    void handle_read(const boost::system::error_code& error, size_t bytes_transferred);
    void handle_write(const boost::system::error_code& error);

    tcp::socket socket_;
    enum { MAX_LENGTH = 8192 };
    char data_[MAX_LENGTH];
    DBManager* db_manager_;
    BEPHandler bep_handler_;
};

class TrackerServer {
public:
    TrackerServer();
    ~TrackerServer();

    void configure(const std::string& host, int port, int worker_threads, int max_connections);
    bool start();
    void run();
    void stop();

    // Statistics
    int get_active_connections() const;
    long long get_total_requests() const;
    double get_requests_per_second() const;

private:
    void start_accept();
    void handle_accept(TrackerSession::pointer new_session, const boost::system::error_code& error);

    boost::asio::io_context io_context_;
    std::unique_ptr<tcp::acceptor> acceptor_;
    std::unique_ptr<DBManager> db_manager_;
    
    std::string host_;
    int port_;
    int worker_threads_;
    int max_connections_;
    
    std::vector<std::thread> worker_threads_vec_;
    std::atomic<int> active_connections_{0};
    std::atomic<long long> total_requests_{0};
    
    bool running_;
    mutable std::mutex stats_mutex_;
};

#endif // TRACKER_SERVER_HPP