#include "tracker_server.hpp"
#include "../utils/logger.hpp"
#include <chrono>

TrackerServer::TrackerServer()
    : port_(6969), worker_threads_(8), max_connections_(300), task_pool_(nullptr), running_(false) {}

TrackerServer::~TrackerServer() {
    stop();
}

void TrackerServer::configure(const std::string& host, int port, int worker_threads, int max_connections) {
    host_ = host;
    port_ = port;
    worker_threads_ = worker_threads;
    max_connections_ = max_connections;
    task_pool_ = std::make_unique<boost::asio::thread_pool>(worker_threads);
}

void TrackerServer::configure_bep(int announce_interval, int min_announce_interval,
                                   int num_want, int max_peer_age, int cleanup_interval) {
    bep_config_announce_interval_     = announce_interval;
    bep_config_min_announce_interval_ = min_announce_interval;
    bep_config_num_want_              = num_want;
    bep_config_max_peer_age_          = max_peer_age;
    bep_config_cleanup_interval_      = cleanup_interval;
}

bool TrackerServer::start(DBManager* db_manager) {
    if (!db_manager) {
        LOG_ERROR("Database manager is required");
        return false;
    }

    try {
        db_manager_ = db_manager;

        tcp::endpoint endpoint(boost::asio::ip::make_address(host_), port_);
        acceptor_ = std::make_unique<tcp::acceptor>(io_context_, endpoint);

        LOG_INFO("Tracker server listening on %s:%d", host_.c_str(), port_);
        running_ = true;
        start_accept();
        return true;

    } catch (std::exception& e) {
        LOG_ERROR("Exception in start(): %s", e.what());
        return false;
    }
}

void TrackerServer::start_workers() {
    for (int i = 0; i < worker_threads_; ++i) {
        worker_threads_vec_.emplace_back([this]() {
            io_context_.run();
        });
    }
}

void TrackerServer::run() {
    io_context_.run();
}

void TrackerServer::stop() {
    if (!running_) {
        return;
    }
    running_ = false;

    if (acceptor_) {
        boost::system::error_code ec;
        acceptor_->close(ec);
        acceptor_.reset();
    }

    if (task_pool_) {
        task_pool_->stop();
    }

    io_context_.stop();

    for (auto& t : worker_threads_vec_) {
        if (t.joinable()) {
            t.join();
        }
    }
    worker_threads_vec_.clear();

    if (task_pool_) {
        task_pool_->join();
        task_pool_.reset();
    }
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
    if (!running_) {
        return;
    }
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
            
            std::string client_ip = "0.0.0.0";
            try {
                client_ip = socket_.remote_endpoint().address().to_string();
            } catch (...) {}

            if (server_ && server_->is_running()) {
                boost::asio::post(server_->task_pool(), [this, self = shared_from_this(), request, client_ip]() {
                    try {
                        // Apply server-level BEP config each time (lightweight value copy)
                        bep_handler_.configure(
                            server_->bep_announce_interval(),
                            server_->bep_min_announce_interval(),
                            server_->bep_num_want(),
                            server_->bep_max_peer_age(),
                            server_->bep_cleanup_interval());
                        std::string response = bep_handler_.handle_request(request, db_manager_, client_ip);

                        if (server_) {
                            server_->record_request();
                        }

                        // Write the response back on the I/O context (network threads)
                        boost::asio::post(server_->io_context(), [this, self, response]() {
                            boost::asio::async_write(socket_, boost::asio::buffer(response),
                                [this, self](const boost::system::error_code& write_error,
                                              std::size_t /*bytes_transferred*/) {
                                    handle_write(write_error);
                                });
                        });
                    } catch (const std::exception& e) {
                        LOG_ERROR("Error handling request in task pool: %s", e.what());
                        boost::asio::post(server_->io_context(), [this, self]() {
                            close_session();
                        });
                    }
                });
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Error preparing request: %s", e.what());
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
    std::lock_guard<std::mutex> lock(rps_mutex_);
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_rps_time_).count();

    if (duration > 0) {
        double rps = static_cast<double>(total_requests_.load() - last_rps_requests_) / duration;
        last_rps_time_ = now;
        last_rps_requests_ = total_requests_.load();
        return rps;
    }
    return 0.0;
}
