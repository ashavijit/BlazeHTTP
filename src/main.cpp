#include "core/event_loop.hpp"
#include "core/worker_pool.hpp"
#include "core/connection.hpp"
#include "http/http_parser.hpp"
#include "http/static_file.hpp"
#include "proxy/l7_proxy.hpp"
#include "http/cache.hpp"
#include <iostream>
#include <stdexcept>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

int main() {
    try {
        const int PORT = 8080;
        const bool USE_TLS = true;
        const std::string CERT_FILE = "server.crt";
        const std::string KEY_FILE = "server.key";
        const std::string STATIC_ROOT = "./static";
        const std::string BACKEND_HOST = "127.0.0.1";
        const int BACKEND_PORT = 8081;
        const size_t NUM_WORKERS = std::thread::hardware_concurrency();
        const size_t CACHE_SIZE = 100;

        EventLoop event_loop;
        WorkerPool worker_pool(NUM_WORKERS);
        HttpParser http_parser;
        StaticFile static_file(STATIC_ROOT);
        L7Proxy proxy(BACKEND_HOST, BACKEND_PORT);
        Cache cache(CACHE_SIZE);

        int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd == -1) {
            throw std::runtime_error("Failed to create socket: " + std::string(strerror(errno)));
        }

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(PORT);

        int optval = 1;
        if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
            throw std::runtime_error("Failed to set SO_REUSEADDR: " + std::string(strerror(errno)));
        }

        if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
            throw std::runtime_error("Failed to bind socket: " + std::string(strerror(errno)));
        }

        if (listen(listen_fd, 10) == -1) {
            throw std::runtime_error("Failed to listen on socket: " + std::string(strerror(errno)));
        }

        std::cout << "Server listening on port " << PORT << " with TLS: " << (USE_TLS ? "true" : "false") << std::endl;

        event_loop.addFd(listen_fd, EPOLLIN, [&](int fd, uint32_t events) {
            std::cout << "Event loop triggered for fd " << fd << " with events " << events << std::endl;

            int client_fd = accept(listen_fd, nullptr, nullptr);
            if (client_fd == -1) {
                std::cerr << "Failed to accept connection: " << strerror(errno) << std::endl;
                return;
            }

            std::cout << "Accepted new connection on fd " << client_fd << std::endl;

            worker_pool.submit([client_fd, USE_TLS, CERT_FILE, KEY_FILE, &http_parser, &static_file, &proxy, &cache] {
                try {
                    std::cout << "Worker thread processing connection on fd " << client_fd << std::endl;

                    Connection conn(client_fd, USE_TLS, CERT_FILE, KEY_FILE);
                    char buffer[4096];
                    ssize_t bytes_read = conn.read(buffer, sizeof(buffer));
                    if (bytes_read <= 0) {
                        std::cerr << "Failed to read from connection on fd " << client_fd << ": " << strerror(errno) << std::endl;
                        close(client_fd);
                        return;
                    }

                    std::cout << "Read " << bytes_read << " bytes from fd " << client_fd << std::endl;

                    std::string request_data(buffer, bytes_read);
                    Request request = http_parser.parseRequest(request_data, conn);

                    std::cout << "Parsed request: " << request.method << " " << request.path << " " << request.version << std::endl;

                    std::string cached_response;
                    if (cache.get(request.path, cached_response)) {
                        std::cout << "Serving cached response for " << request.path << std::endl;
                        conn.write(cached_response.c_str(), cached_response.size());
                        close(client_fd);
                        return;
                    }

                    Response response;
                    if (request.path == "/proxy") {
                        std::cout << "Forwarding request to proxy" << std::endl;
                        response = proxy.forward(request);
                    } else {
                        std::cout << "Serving static file for " << request.path << std::endl;
                        response = static_file.serve(request.path);
                    }

                    std::string response_data = http_parser.generateResponse(response, conn);
                    cache.put(request.path, response_data);

                    std::cout << "Sending response for " << request.path << std::endl;
                    ssize_t bytes_written = conn.write(response_data.c_str(), response_data.size());
                    if (bytes_written <= 0) {
                        std::cerr << "Failed to write response on fd " << client_fd << ": " << strerror(errno) << std::endl;
                    } else {
                        std::cout << "Wrote " << bytes_written << " bytes to fd " << client_fd << std::endl;
                    }

                    close(client_fd);
                } catch (const std::exception& e) {
                    std::cerr << "Error handling connection on fd " << client_fd << ": " << e.what() << std::endl;
                    close(client_fd);
                }
            });

            std::cout << "Submitted task to worker pool for fd " << client_fd << std::endl;
        });

        std::cout << "Starting event loop" << std::endl;
        event_loop.run();
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}