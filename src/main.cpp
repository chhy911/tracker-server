#include <iostream>
#include <cstdlib>
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>
#include <signal.h>
#include "tracker/tracker_server.hpp"
#include "database/db_manager.hpp"
#include "api/rest_api.hpp"
#include "api/http_server.hpp"
#include "utils/config.hpp"
#include "utils/logger.hpp"

TrackerServer*             g_server           = nullptr;
std::unique_ptr<HttpServer> g_api_server;
std::unique_ptr<HttpServer> g_dashboard_server;
std::unique_ptr<DBManager>  g_db_manager;
std::unique_ptr<RESTApi>    g_rest_api;
std::atomic<bool>           g_running{true};

void signal_handler(int sig) {
    LOG_INFO("Received signal %d, shutting down...", sig);
    g_running = false;
    if (g_dashboard_server) g_dashboard_server->stop();
    if (g_api_server)       g_api_server->stop();
    if (g_server)           g_server->stop();
}

static std::string env_or(const char* name, const std::string& fallback) {
    const char* v = std::getenv(name);
    return v ? std::string(v) : fallback;
}

int main(int argc, char* argv[]) {
    try {
        Config config;
        const char* cfg_path = (argc > 1) ? argv[1] : "config/tracker.conf";
        if (!config.load(cfg_path)) {
            std::cerr << "Failed to load config file: " << cfg_path << std::endl;
            return 1;
        }

        // Logging
        Logger::getInstance().init(
            config.get<std::string>("logging.file", "logs/tracker.log"),
            config.get<std::string>("logging.level", "info"),
            config.get<int>("logging.max_size", 100),
            config.get<int>("logging.backup_count", 5)
        );

        LOG_INFO("=== BitTorrent Tracker Server Starting ===");
        LOG_INFO("Version: 1.1.0");

        // Database
        std::string db_host = env_or("TRACKER_DB_HOST",
            config.get<std::string>("database.host", "localhost"));
        int db_port = config.get<int>("database.port", 3306);
        if (const char* e = std::getenv("TRACKER_DB_PORT")) db_port = std::stoi(e);
        std::string db_user     = env_or("TRACKER_DB_USER",
            config.get<std::string>("database.user", "tracker"));
        std::string db_password = env_or("TRACKER_DB_PASSWORD",
            config.get<std::string>("database.password", "tracker_password"));
        std::string db_name     = env_or("TRACKER_DB_NAME",
            config.get<std::string>("database.database", "tracker_db"));
        int pool_size = config.get<int>("database.pool_size", 20);

        g_db_manager = std::make_unique<DBManager>();
        g_db_manager->configure(db_host, db_port, db_user, db_password, db_name, pool_size);
        if (!g_db_manager->connect()) {
            LOG_ERROR("Failed to connect to database");
            return 1;
        }

        // [tracker] parameters
        int announce_interval     = config.get<int>("tracker.announce_interval",     1800);
        int min_announce_interval = config.get<int>("tracker.min_announce_interval",  600);
        int num_want              = config.get<int>("tracker.num_want",                50);
        int cleanup_interval      = config.get<int>("tracker.cleanup_interval",      3600);
        int max_peer_age          = config.get<int>("tracker.max_peer_age",          3600);

        // Server
        g_server = new TrackerServer();
        std::string host   = config.get<std::string>("server.host", "0.0.0.0");
        int port           = config.get<int>("server.port", 6969);
        int worker_threads = config.get<int>("server.worker_threads", 4);
        int max_connections = config.get<int>("server.max_connections", 200);
        g_server->configure(host, port, worker_threads, max_connections);

        // Pass [tracker] config into BEPHandler
        g_server->configure_bep(announce_interval, min_announce_interval,
                                 num_want, max_peer_age, cleanup_interval);

        signal(SIGINT,  signal_handler);
        signal(SIGTERM, signal_handler);

        if (!g_server->start(g_db_manager.get())) {
            LOG_ERROR("Failed to start tracker server");
            return 1;
        }

        // REST API
        g_rest_api = std::make_unique<RESTApi>(g_db_manager.get());
        std::string api_host = config.get<std::string>("api.host", "0.0.0.0");
        int api_port         = config.get<int>("api.port", 8081);
        g_api_server = std::make_unique<HttpServer>(
            g_server->io_context(), HttpServerMode::API,
            api_host, api_port, g_rest_api.get(), "", g_server);
        if (!g_api_server->start()) {
            LOG_ERROR("Failed to start API HTTP server");
            return 1;
        }

        // Optional built-in dashboard static server
        bool dashboard_enabled = config.get<bool>("dashboard.enabled", false);
        if (dashboard_enabled) {
            std::string dash_host  = config.get<std::string>("dashboard.host", "0.0.0.0");
            int dash_port          = config.get<int>("dashboard.port", 3000);
            std::string static_path = config.get<std::string>("dashboard.static_path", "dashboard/dist");
            g_dashboard_server = std::make_unique<HttpServer>(
                g_server->io_context(), HttpServerMode::STATIC,
                dash_host, dash_port, nullptr, static_path, nullptr);
            if (!g_dashboard_server->start()) {
                LOG_WARN("Dashboard static server failed (build dashboard first)");
            } else {
                LOG_INFO("Dashboard: %s:%d", dash_host.c_str(), dash_port);
            }
        } else {
            LOG_INFO("Dashboard built-in server disabled (serve via Nginx :8888)");
        }

        // Cleanup timer thread: periodically remove stale peers
        std::thread cleanup_thread([&]() {
            while (g_running) {
                for (int s = 0; s < cleanup_interval && g_running; ++s) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
                if (!g_running) break;
                LOG_INFO("Running scheduled peer cleanup (max_age=%ds)", max_peer_age);
                if (g_db_manager) {
                    g_db_manager->cleanup_old_peers(max_peer_age);
                }
            }
        });
        cleanup_thread.detach();

        g_server->start_workers();

        LOG_INFO("Tracker server started successfully");
        LOG_INFO("Worker threads: %d | Max connections: %d", worker_threads, max_connections);
        LOG_INFO("Announce interval: %ds | Num-want: %d", announce_interval, num_want);
        LOG_INFO("Cleanup interval: %ds | Max peer age: %ds", cleanup_interval, max_peer_age);
        LOG_INFO("REST API: %s:%d", api_host.c_str(), api_port);

        g_server->run();

        g_running = false;
        g_dashboard_server.reset();
        g_api_server.reset();
        g_rest_api.reset();
        g_db_manager.reset();
        delete g_server;
        g_server = nullptr;

        LOG_INFO("Tracker server stopped");
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
