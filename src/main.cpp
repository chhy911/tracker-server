#include <iostream>
#include <cstdlib>
#include <memory>
#include <signal.h>
#include "tracker/tracker_server.hpp"
#include "database/db_manager.hpp"
#include "api/rest_api.hpp"
#include "api/http_server.hpp"
#include "utils/config.hpp"
#include "utils/logger.hpp"

TrackerServer* g_server = nullptr;
std::unique_ptr<HttpServer> g_api_server;
std::unique_ptr<HttpServer> g_dashboard_server;
std::unique_ptr<DBManager> g_db_manager;
std::unique_ptr<RESTApi> g_rest_api;

void signal_handler(int signal) {
    LOG_INFO("Received signal %d, shutting down...", signal);
    if (g_dashboard_server) g_dashboard_server->stop();
    if (g_api_server) g_api_server->stop();
    if (g_server) {
        g_server->stop();
    }
    exit(0);
}

static std::string env_or(const char* name, const std::string& fallback) {
    const char* value = std::getenv(name);
    return value ? std::string(value) : fallback;
}

int main(int argc, char* argv[]) {
    try {
        Config config;
        if (argc > 1) {
            if (!config.load(argv[1])) {
                std::cerr << "Failed to load config file: " << argv[1] << std::endl;
                return 1;
            }
        } else {
            if (!config.load("config/tracker.conf")) {
                std::cerr << "Failed to load default config file" << std::endl;
                return 1;
            }
        }

        Logger::getInstance().init(
            config.get<std::string>("logging.file", "logs/tracker.log"),
            config.get<std::string>("logging.level", "info")
        );

        LOG_INFO("=== BitTorrent Tracker Server Starting ===");
        LOG_INFO("Version: 1.0.0");

        std::string db_host = env_or("TRACKER_DB_HOST",
            config.get<std::string>("database.host", "localhost"));
        int db_port = config.get<int>("database.port", 3306);
        const char* db_port_env = std::getenv("TRACKER_DB_PORT");
        if (db_port_env) {
            db_port = std::stoi(db_port_env);
        }
        std::string db_user = env_or("TRACKER_DB_USER",
            config.get<std::string>("database.user", "tracker"));
        std::string db_password = env_or("TRACKER_DB_PASSWORD",
            config.get<std::string>("database.password", "tracker_password"));
        std::string db_name = env_or("TRACKER_DB_NAME",
            config.get<std::string>("database.database", "tracker_db"));
        int pool_size = config.get<int>("database.pool_size", 50);

        g_db_manager = std::make_unique<DBManager>();
        g_db_manager->configure(db_host, db_port, db_user, db_password, db_name, pool_size);
        if (!g_db_manager->connect()) {
            LOG_ERROR("Failed to connect to database");
            return 1;
        }

        g_server = new TrackerServer();

        std::string host = config.get<std::string>("server.host", "0.0.0.0");
        int port = config.get<int>("server.port", 6969);
        int worker_threads = config.get<int>("server.worker_threads", 8);
        int max_connections = config.get<int>("server.max_connections", 300);

        g_server->configure(host, port, worker_threads, max_connections);

        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);

        if (!g_server->start(g_db_manager.get())) {
            LOG_ERROR("Failed to start tracker server");
            return 1;
        }

        g_rest_api = std::make_unique<RESTApi>(g_db_manager.get());

        std::string api_host = config.get<std::string>("api.host", "0.0.0.0");
        int api_port = config.get<int>("api.port", 8080);
        std::string dashboard_host = config.get<std::string>("dashboard.host", "0.0.0.0");
        int dashboard_port = config.get<int>("dashboard.port", 3000);
        std::string static_path = config.get<std::string>("dashboard.static_path", "dashboard/dist");

        g_api_server = std::make_unique<HttpServer>(
            g_server->io_context(), HttpServerMode::API,
            api_host, api_port, g_rest_api.get(), "", g_server);

        g_dashboard_server = std::make_unique<HttpServer>(
            g_server->io_context(), HttpServerMode::STATIC,
            dashboard_host, dashboard_port, nullptr, static_path, nullptr);

        if (!g_api_server->start()) {
            LOG_ERROR("Failed to start API HTTP server");
            return 1;
        }
        if (!g_dashboard_server->start()) {
            LOG_WARN("Dashboard static server failed (build dashboard first)");
        }

        g_server->start_workers();

        LOG_INFO("Tracker server started successfully");
        LOG_INFO("Worker threads: %d", worker_threads);
        LOG_INFO("Max connections: %d", max_connections);
        LOG_INFO("REST API: %s:%d", api_host.c_str(), api_port);
        LOG_INFO("Dashboard: %s:%d", dashboard_host.c_str(), dashboard_port);

        g_server->run();

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
