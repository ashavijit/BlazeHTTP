#include "proxy/l7_proxy.hpp"
#include <stdexcept>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

L7Proxy::L7Proxy(const std::string& backend_host, int backend_port)
    : backend_host_(backend_host), backend_port_(backend_port) {}

Response L7Proxy::forward(const Request& request) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        throw std::runtime_error("Failed to create socket for proxy");
    }

    struct sockaddr_in backend_addr;
    backend_addr.sin_family = AF_INET;
    backend_addr.sin_port = htons(backend_port_);
    inet_pton(AF_INET, backend_host_.c_str(), &backend_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&backend_addr, sizeof(backend_addr)) == -1) {
        close(sock);
        throw std::runtime_error("Failed to connect to backend");
    }

    std::string request_data = request.method + " " + request.path + " " + request.version + "\r\n";
    for (const auto& header : request.headers) {
        request_data += header.first + ": " + header.second + "\r\n";
    }
    request_data += "\r\n" + request.body;
    write(sock, request_data.c_str(), request_data.size());

    char buffer[4096];
    ssize_t bytes_read = read(sock, buffer, sizeof(buffer));
    close(sock);

    if (bytes_read <= 0) {
        throw std::runtime_error("Failed to read response from backend");
    }

    return Response{200, "OK", "HTTP/1.1", {}, std::string(buffer, bytes_read)};
}