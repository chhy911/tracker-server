#include "http_server.hpp"
#include "../tracker/tracker_server.hpp"
#include "../utils/logger.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace {
std::string status_text(int code) {
    switch (code) {
        case 200: return "OK";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        default: return "Error";
    }
}
}

HttpServer::HttpServer(boost::asio::io_context& io_context,
                       HttpServerMode mode,
                       const std::string& host,
                       int port,
                       RESTApi* rest_api,
                       const std::string& static_root,
                       TrackerServer* tracker)
    : io_context_(io_context),
      mode_(mode),
      host_(host),
      port_(port),
      rest_api_(rest_api),
      static_root_(static_root),
      tracker_(tracker) {}

bool HttpServer::start() {
    try {
        boost::asio::ip::tcp::endpoint endpoint(
            boost::asio::ip::make_address(host_), port_);
        acceptor_ = std::make_unique<boost::asio::ip::tcp::acceptor>(io_context_, endpoint);
        running_ = true;
        do_accept();
        LOG_INFO("HTTP server (%s) listening on %s:%d",
                 mode_ == HttpServerMode::API ? "API" : "static",
                 host_.c_str(), port_);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("HTTP server start failed: %s", e.what());
        return false;
    }
}

void HttpServer::stop() {
    running_ = false;
    if (acceptor_) {
        boost::system::error_code ec;
        acceptor_->close(ec);
    }
}

void HttpServer::do_accept() {
    if (!running_) return;

    acceptor_->async_accept(
        [this](const boost::system::error_code& error, boost::asio::ip::tcp::socket socket) {
            if (!error) {
                handle_request(std::move(socket));
            } else if (running_) {
                LOG_ERROR("HTTP accept error: %s", error.message().c_str());
            }
            do_accept();
        });
}

void HttpServer::handle_request(boost::asio::ip::tcp::socket socket) {
    try {
        boost::asio::streambuf buffer;
        boost::asio::read_until(socket, buffer, "\r\n\r\n");

        std::istream is(&buffer);
        std::string request_line;
        std::getline(is, request_line);
        if (!request_line.empty() && request_line.back() == '\r') {
            request_line.pop_back();
        }

        std::string method;
        std::string path = "/";
        size_t sp1 = request_line.find(' ');
        size_t sp2 = request_line.find(' ', sp1 + 1);
        if (sp1 != std::string::npos) {
            method = request_line.substr(0, sp1);
            if (sp2 != std::string::npos) {
                path = request_line.substr(sp1 + 1, sp2 - sp1 - 1);
            } else {
                path = request_line.substr(sp1 + 1);
            }
        }

        std::string response_body;
        std::string content_type = "application/json";
        int status = 200;
        bool cors = false;

        if (mode_ == HttpServerMode::API) {
            cors = true;
            if (method == "OPTIONS") {
                response_body = "";
                status = 204;
            } else if (method != "GET") {
                response_body = "{\"error\":\"Method not allowed\"}";
                status = 405;
            } else if (path == "/" || path == "/api" || path == "/api/") {
                response_body =
                    "{\"service\":\"tracker-api\","
                    "\"endpoints\":[\"/api/health\",\"/api/stats\",\"/api/metrics\","
                    "\"/api/torrent/:info_hash\",\"/api/peers/:info_hash\"],"
                    "\"dashboard\":\"use port 8888 for web UI\"}";
            } else if (path == "/api/torrents" && rest_api_) {
                response_body = rest_api_->get_torrents();
            } else if (path == "/api/metrics" && tracker_) {
                std::ostringstream json;
                json << "{"
                     << "\"active_connections\":" << tracker_->get_active_connections() << ","
                     << "\"total_requests\":" << tracker_->get_total_requests() << ","
                     << "\"requests_per_second\":" << tracker_->get_requests_per_second()
                     << "}";
                response_body = json.str();
            } else if (rest_api_) {
                size_t qpos = path.find('?');
                std::string route = qpos == std::string::npos ? path : path.substr(0, qpos);
                std::string query = qpos == std::string::npos ? "" : path.substr(qpos + 1);
                std::string full_path = query.empty() ? route : route + "?" + query;
                response_body = rest_api_->handle_request(method, full_path, "");
                if (response_body.find("\"error\"") != std::string::npos &&
                    response_body.find("Not found") != std::string::npos) {
                    status = 404;
                }
            } else {
                response_body = "{\"error\":\"API unavailable\"}";
                status = 503;
            }
        } else {
            // 3000 端口仅静态资源；/api 请走 8888(Nginx) 或 8081
            if (path.size() >= 4 && path.compare(0, 4, "/api") == 0) {
                status = 404;
                content_type = "application/json; charset=utf-8";
                response_body =
                    "{\"error\":\"API is not on this port. Use :8888/api/... or :8081/api/...\"}";
            } else {
            // path "/" 无扩展名时须用 text/html，否则浏览器会当下载文件
            std::string mime_path = path;
            if (mime_path.empty() || mime_path == "/" ||
                mime_path.find('.') == std::string::npos) {
                mime_path = "/index.html";
            }
            content_type = guess_content_type(mime_path);
            response_body = serve_static_file(path);
            if (response_body.empty()) {
                status = 404;
                content_type = "text/plain; charset=utf-8";
                response_body = "Not Found";
            }
            }
        }

        std::string http_response = build_http_response(status, content_type, response_body, cors);
        boost::asio::write(socket, boost::asio::buffer(http_response));
    } catch (const std::exception& e) {
        LOG_DEBUG("HTTP session error: %s", e.what());
    }

    boost::system::error_code ec;
    socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
}

std::string HttpServer::build_http_response(int status,
                                           const std::string& content_type,
                                           const std::string& body,
                                           bool cors) const {
    std::ostringstream out;
    out << "HTTP/1.1 " << status << " " << status_text(status) << "\r\n";
    out << "Content-Type: " << content_type << "\r\n";
    out << "Content-Length: " << body.size() << "\r\n";
    out << "Connection: close\r\n";
    if (cors) {
        out << "Access-Control-Allow-Origin: *\r\n";
        out << "Access-Control-Allow-Methods: GET, OPTIONS\r\n";
        out << "Access-Control-Allow-Headers: Content-Type\r\n";
    }
    out << "\r\n";
    out << body;
    return out.str();
}

std::string HttpServer::guess_content_type(const std::string& path) const {
    if (path.size() >= 5 && path.substr(path.size() - 5) == ".html") {
        return "text/html; charset=utf-8";
    }
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".css") {
        return "text/css; charset=utf-8";
    }
    if (path.size() >= 3 && path.substr(path.size() - 3) == ".js") {
        return "application/javascript; charset=utf-8";
    }
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".svg") {
        return "image/svg+xml";
    }
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".ico") {
        return "image/x-icon";
    }
    if (path.size() >= 5 && path.substr(path.size() - 5) == ".json") {
        return "application/json; charset=utf-8";
    }
    return "application/octet-stream";
}

std::string HttpServer::serve_static_file(const std::string& path) const {
    std::string rel = path;
    if (rel.empty() || rel == "/") {
        rel = "index.html";
    } else if (rel[0] == '/') {
        rel = rel.substr(1);
    }
    if (rel.find("..") != std::string::npos) {
        return "";
    }

    std::string file_path = static_root_;
    if (!file_path.empty() && file_path.back() != '/' && file_path.back() != '\\') {
        file_path += "/";
    }
    file_path += rel;

    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open() && path.find('.') == std::string::npos) {
        file.open(static_root_ + "/index.html", std::ios::binary);
    }
    if (!file.is_open()) {
        return "";
    }

    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}
