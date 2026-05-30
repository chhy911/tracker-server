#include "tracker_server.hpp"
#include "../utils/logger.hpp"
#include <chrono>

TrackerServer::TrackerServer() : port_(6969), worker_threads_(8), max_connections_(300), running_(false) {}

TrackerServer::~TrackerServer() {
    stop();
}

void TrackerServer::configure(const std::string& host, int port, int worker_threads, int max_connections) {
    host_ = host;
    port_ = port;
    worker_threads_ = worker_threads;
    max_connections_ = max_connections;
}

bool TrackerServer::start(DBManager* db_manager) {
    if (!db_manager) {
        LOG_ERROR("Database manager is required");
        return false;
    }

    try {
        db_manager_ = db_manager;

        tcp::endpoint endpoint(boost::asio::ip::address::from_string(host_), port_);
        acceptor_ = std::make_unique<tcp::acceptor>(io_context_, endpoint);

        LOG_INFO("Tracker server listening on %s:%d", host_.c_str(), port_);
        running_ = true;

        for (int i = 0; i < worker_threads_; ++i) {
            worker_threads_vec_.emplace_back([this]() {
                io_context_.run();
            });
        }

        start_accept();
        return true;

    } catch (std::exception& e) {
        LOG_ERROR("Exception in start(): %s", e.what());
        return false;
    }
}

void TrackerServer::run() {
    io_context_.run();
}

void TrackerServer::stop() {
    running_ = false;
    io_context_.stop();

    for (auto& t : worker_threads_vec_) {
        if (t.joinable()) {
            t.join();
        }
    }
    worker_threads_vec_.clear();
}

void TrackerServer::record_request() {
    total_requests_++;
}

void TrackerServer::release_connection() {
    if (active_connections_.load() > 0) {
        active_connections_--;
    }
}

void TrackerServer::start_accept() {
    if (!running_) return;

    TrackerSession::pointer new_session =
        TrackerSession::create(io_context_, db_manager_, this);

    acceptor_->async_accept(new_session->socket(),
        [this, new_session](const boost::system::error_code& error) {
            handle_accept(new_session, error);
        });
}

void TrackerServer::handle_accept(TrackerSession::pointer new_session,
                                   const boost::system::error_code& error) {
    if (!error && active_connections_ < max_connections_) {
        active_connections_++;
        new_session->start();
        LOG_DEBUG("New connection accepted. Active: %d", active_connections_.load());
    } else if (error) {
        LOG_ERROR("Accept error: %s", error.message().c_str());
    } else {
        LOG_WARN("Max connections reached, rejecting new connection");
    }

    start_accept();
}

void TrackerSession::start() {
    socket_.async_read_some(boost::asio::buffer(data_, MAX_LENGTH),
        [this, self = shared_from_this()](const boost::system::error_code& error, size_t bytes_transferred) {
            handle_read(error, bytes_transferred);
        });
}

void TrackerSession::handle_read(const boost::system::error_code& error, size_t bytes_transferred) {
    if (!error) {
        try {
            std::string request(data_, bytes_transferred);
            std::string response = bep_handler_.handle_request(request, db_manager_);

            if (server_) {
                server_->record_request();
            }

            boost::asio::async_write(socket_, boost::asio::buffer(response),
                [this, self = shared_from_this()](const boost::system::error_code& write_error) {
                    handle_write(write_error);
                });
        } catch (const std::exception& e) {
            LOG_ERROR("Error handling request: %s", e.what());
            close_session();
        }
    } else {
        LOG_DEBUG("Read error: %s", error.message().c_str());
        close_session();
    }
}

void TrackerSession::handle_write(const boost::system::error_code& error) {
    if (error) {
        LOG_DEBUG("Write error: %s", error.message().c_str());
    }
    close_session();
}

void TrackerSession::close_session() {
    if (connection_released_) return;
    connection_released_ = true;

    boost::system::error_code ec;
    socket_.close(ec);

    if (server_) {
        server_->release_connection();
    }
}

int TrackerServer::get_active_connections() const {
    return active_connections_.load();
}

long long TrackerServer::get_total_requests() const {
    return total_requests_.load();
}

double TrackerServer::get_requests_per_second() const {
    static auto last_time = std::chrono::steady_clock::now();
    static long long last_requests = 0;

    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_time).count();

    if (duration > 0) {
        double rps = static_cast<double>(total_requests_.load() - last_requests) / duration;
        last_time = now;
        last_requests = total_requests_.load();
        return rps;
    }
    return 0.0;
}
