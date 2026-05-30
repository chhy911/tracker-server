#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP

#include <boost/asio.hpp>
#include <memory>
#include <string>
#include "rest_api.hpp"

class TrackerServer;

enum class HttpServerMode {
    API,
    STATIC
};

class HttpServer {
public:
    HttpServer(boost::asio::io_context& io_context,
               HttpServerMode mode,
               const std::string& host,
               int port,
               RESTApi* rest_api,
               const std::string& static_root,
               TrackerServer* tracker = nullptr);

    bool start();
    void stop();

private:
    void do_accept();
    void handle_request(boost::asio::ip::tcp::socket socket);

    std::string build_http_response(int status,
                                    const std::string& content_type,
                                    const std::string& body,
                                    bool cors = false) const;
    std::string serve_static_file(const std::string& path) const;
    std::string guess_content_type(const std::string& path) const;

    boost::asio::io_context& io_context_;
    HttpServerMode mode_;
    std::string host_;
    int port_;
    RESTApi* rest_api_;
    std::string static_root_;
    TrackerServer* tracker_;

    std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
    bool running_{false};
};

#endif // HTTP_SERVER_HPP
