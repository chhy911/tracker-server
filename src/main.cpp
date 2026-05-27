#include <iostream>
#include <signal.h>
#include "tracker/tracker_server.hpp"
#include "utils/config.hpp"
#include "utils/logger.hpp"

// Global server instance
TrackerServer* g_server = nullptr;

// Signal handler
void signal_handler(int signal) {
    LOG_INFO("Received signal %d, shutting down...", signal);
    if (g_server) {
        g_server->stop();
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    try {
        // Load configuration
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

        // Initialize logger
        Logger::getInstance().init(
            config.get<std::string>("logging.file", "logs/tracker.log"),
            config.get<std::string>("logging.level", "info")
        );

        LOG_INFO("=== BitTorrent Tracker Server Starting ===");
        LOG_INFO("Version: 1.0.0");

        // Create and configure tracker server
        g_server = new TrackerServer();
        
        std::string host = config.get<std::string>("server.host", "0.0.0.0");
        int port = config.get<int>("server.port", 6969);
        int worker_threads = config.get<int>("server.worker_threads", 8);
        int max_connections = config.get<int>("server.max_connections", 300);

        g_server->configure(host, port, worker_threads, max_connections);

        // Setup signal handlers
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);

        // Start server
        LOG_INFO("Starting tracker server on %s:%d", host.c_str(), port);
        if (!g_server->start()) {
            LOG_ERROR("Failed to start tracker server");
            return 1;
        }

        LOG_INFO("Tracker server started successfully");
        LOG_INFO("Worker threads: %d", worker_threads);
        LOG_INFO("Max connections: %d", max_connections);

        // Run server (blocking call)
        g_server->run();

        // Cleanup
        delete g_server;
        g_server = nullptr;

        LOG_INFO("Tracker server stopped");
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}