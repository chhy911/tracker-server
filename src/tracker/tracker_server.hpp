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
#include <atomic>
#include <boost/asio/thread_pool.hpp>
#include "bep_handler.hpp"
#include "../database/db_manager.hpp"

using boost::asio::ip::tcp;

class TrackerServer;

class TrackerSession : public boost::enable_shared_from_this<TrackerSession> {
public:
    typedef boost::shared_ptr<TrackerSession> pointer;

    static pointer create(boost::asio::io_context& io_context, DBManager* db, TrackerServer* server) {
        return pointer(new TrackerSession(io_context, db, server));
    }

    tcp::socket& socket() { return socket_; }

    void start();

private:
    TrackerSession(boost::asio::io_context& io_context, DBManager* db, TrackerServer* server)
        : socket_(io_context), db_manager_(db), server_(server) {}

    void handle_read(const boost::system::error_code& error, size_t bytes_transferred);
    void handle_write(const boost::system::error_code& error);
    void close_session();

    tcp::socket socket_;
    enum { MAX_LENGTH = 8192 };
    char data_[MAX_LENGTH];
    DBManager* db_manager_;
    TrackerServer* server_;
    BEPHandler bep_handler_;
    bool connection_released_{false};
};

class TrackerServer {
public:
    TrackerServer();
    ~TrackerServer();

    void configure(const std::string& host, int port, int worker_threads, int max_connections);
    bool start(DBManager* db_manager);
    void start_workers();
    void run();
    void stop();

    bool is_running() const { return running_; }

    boost::asio::io_context& io_context() { return io_context_; }
    boost::asio::thread_pool& task_pool() { return *task_pool_; }

    void record_request();
    void release_connection();

    int get_active_connections() const;
    long long get_total_requests() const;
    double get_requests_per_second() const;

private:
    void start_accept();
    void handle_accept(TrackerSession::pointer new_session, const boost::system::error_code& error);

    boost::asio::io_context io_context_;
    std::unique_ptr<tcp::acceptor> acceptor_;
    DBManager* db_manager_{nullptr};

    std::string host_;
    int port_;
    int worker_threads_;
    int max_connections_;

    std::vector<std::thread> worker_threads_vec_;
    std::atomic<int> active_connections_{0};
    std::atomic<long long> total_requests_{0};

    mutable std::mutex rps_mutex_;
    mutable std::chrono::steady_clock::time_point last_rps_time_{std::chrono::steady_clock::now()};
    mutable long long last_rps_requests_{0};

    std::unique_ptr<boost::asio::thread_pool> task_pool_;

    bool running_;
};

#endif // TRACKER_SERVER_HPP
