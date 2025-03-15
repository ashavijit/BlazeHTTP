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
            throw std::runtime_error("Failed to create socket");
        }

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(PORT);

        if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
            throw std::runtime_error("Failed to bind socket");
        }

        if (listen(listen_fd, 10) == -1) {
            throw std::runtime_error("Failed to listen on socket");
        }

        std::cout << "Server listening on port " << PORT << " with TLS: " << USE_TLS << std::endl;

        event_loop.addFd(listen_fd, EPOLLIN, [&](int fd, uint32_t events) {
            int client_fd = accept(listen_fd, nullptr, nullptr);
            if (client_fd == -1) {
                std::cerr << "Failed to accept connection" << std::endl;
                return;
            }

            worker_pool.submit([client_fd, USE_TLS, CERT_FILE, KEY_FILE, &http_parser, &static_file, &proxy, &cache] {
                try {
                    Connection conn(client_fd, USE_TLS, CERT_FILE, KEY_FILE);
                    char buffer[4096];
                    ssize_t bytes_read = conn.read(buffer, sizeof(buffer));
                    if (bytes_read <= 0) {
                        std::cerr << "Failed to read from connection" << std::endl;
                        return;
                    }

                    std::string request_data(buffer, bytes_read);
                    Request request = http_parser.parseRequest(request_data);

                    std::string cached_response;
                    if (cache.get(request.path, cached_response)) {
                        conn.write(cached_response.c_str(), cached_response.size());
                        return;
                    }

                    Response response;
                    if (request.path == "/proxy") {
                        response = proxy.forward(request);
                    } else {
                        response = static_file.serve(request.path);
                    }

                    std::string response_data = http_parser.generateResponse(response);
                    cache.put(request.path, response_data);

                    conn.write(response_data.c_str(), response_data.size());
                } catch (const std::exception& e) {
                    std::cerr << "Error handling connection: " << e.what() << std::endl;
                }
            });
        });

        event_loop.run();
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}