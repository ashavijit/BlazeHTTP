#!/bin/bash

PORT=8080
USE_TLS=true
CERT_FILE="server.crt"
KEY_FILE="server.key"
STATIC_ROOT="./static"
BACKEND_HOST="127.0.0.1"
BACKEND_PORT=8081
NUM_WORKERS=$(nproc)  
CACHE_SIZE=100

BUILD_DIR="./build"
SOURCE_FILE="src/main.cpp"
EXECUTABLE="$BUILD_DIR/http_server"

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

generate_certificates() {
    if [ ! -f "$CERT_FILE" ] || [ ! -f "$KEY_FILE" ]; then
        echo "Certificates not found. Generating self-signed certificate and key..."
        openssl req -x509 -newkey rsa:2048 -keyout "$KEY_FILE" -out "$CERT_FILE" -days 365 -nodes \
            -subj "/C=IN/ST=WB/L=Ok/O=test/OU=test/CN=blaze" 2>/dev/null
        if [ $? -eq 0 ]; then
            echo -e "${GREEN}Certificates generated successfully: $CERT_FILE, $KEY_FILE${NC}"
        else
            echo -e "${RED}Failed to generate certificates. Ensure OpenSSL is installed.${NC}"
            exit 1
        fi
    else
        echo "Certificates already exist: $CERT_FILE, $KEY_FILE"
    fi
}

update_main_cpp() {
    echo "Updating $SOURCE_FILE with configuration..."
    cat > "$SOURCE_FILE" << EOL
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
        // Configuration
        const int PORT = $PORT;
        const bool USE_TLS = $USE_TLS;
        const std::string CERT_FILE = "$CERT_FILE";
        const std::string KEY_FILE = "$KEY_FILE";
        const std::string STATIC_ROOT = "$STATIC_ROOT";
        const std::string BACKEND_HOST = "$BACKEND_HOST";
        const int BACKEND_PORT = $BACKEND_PORT;
        const size_t NUM_WORKERS = $NUM_WORKERS;
        const size_t CACHE_SIZE = $CACHE_SIZE;

        // Initialize components
        EventLoop event_loop;
        WorkerPool worker_pool(NUM_WORKERS);
        HttpParser http_parser;
        StaticFile static_file(STATIC_ROOT);
        L7Proxy proxy(BACKEND_HOST, BACKEND_PORT);
        Cache cache(CACHE_SIZE);

        // Set up listening socket
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

        // Event loop callback for new connections
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
                    Request request = http_parser.parseRequest(request_data);

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

                    std::string response_data = http_parser.generateResponse(response);
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
EOL
}

build_server() {
    echo "Building server..."
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR" || exit 1
    cmake .. || { echo -e "${RED}CMake failed${NC}"; exit 1; }
    make || { echo -e "${RED}Make failed${NC}"; exit 1; }
    cd - || exit 1
    echo -e "${GREEN}Build completed successfully${NC}"
}

run_server() {
    if [ ! -f "$EXECUTABLE" ]; then
        echo -e "${RED}Executable not found: $EXECUTABLE. Please build the server first.${NC}"
        exit 1
    fi
    echo "Starting server with the following configuration:"
    echo "  PORT: $PORT"
    echo "  USE_TLS: $USE_TLS"
    echo "  CERT_FILE: $CERT_FILE"
    echo "  KEY_FILE: $KEY_FILE"
    echo "  STATIC_ROOT: $STATIC_ROOT"
    echo "  BACKEND_HOST: $BACKEND_HOST"
    echo "  BACKEND_PORT: $BACKEND_PORT"
    echo "  NUM_WORKERS: $NUM_WORKERS"
    echo "  CACHE_SIZE: $CACHE_SIZE"
    echo "Running server..."
    "$EXECUTABLE"
}

echo "Configuring and starting BlazeHTTP server..."

if [ ! -d "$STATIC_ROOT" ]; then
    echo "Creating static directory: $STATIC_ROOT"
    mkdir -p "$STATIC_ROOT"
    echo "Hello, World!" > "$STATIC_ROOT/index.html"
fi

if [ "$USE_TLS" = true ]; then
    generate_certificates
fi

update_main_cpp


build_server
run_server