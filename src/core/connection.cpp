#include "core/connection.hpp"
#include <stdexcept>
#include <iostream>
#include <unistd.h>        // For close, read, write
#include <sys/socket.h>    // For accept

Connection::Connection(int fd, bool use_tls, const std::string& cert_file, const std::string& key_file)
    : fd_(fd), use_tls_(use_tls), ssl_ctx_(nullptr), ssl_(nullptr) {
    if (use_tls_) {
        SSL_load_error_strings();
        OpenSSL_add_ssl_algorithms();
        ssl_ctx_ = SSL_CTX_new(TLS_server_method());
        if (!ssl_ctx_) {
            throw std::runtime_error("Failed to create SSL context");
        }

        if (SSL_CTX_use_certificate_file(ssl_ctx_, cert_file.c_str(), SSL_FILETYPE_PEM) <= 0 ||
            SSL_CTX_use_PrivateKey_file(ssl_ctx_, key_file.c_str(), SSL_FILETYPE_PEM) <= 0) {
            throw std::runtime_error("Failed to load certificate or key");
        }

        ssl_ = SSL_new(ssl_ctx_);
        SSL_set_fd(ssl_, fd_);
        if (SSL_accept(ssl_) <= 0) {
            throw std::runtime_error("SSL handshake failed");
        }
    }
}

Connection::~Connection() {
    if (use_tls_) {
        if (ssl_) SSL_free(ssl_);
        if (ssl_ctx_) SSL_CTX_free(ssl_ctx_);
    }
    close(fd_);
}

ssize_t Connection::read(char* buffer, size_t len) {
    if (use_tls_) {
        return SSL_read(ssl_, buffer, len);
    }
    return ::read(fd_, buffer, len);
}

ssize_t Connection::write(const char* buffer, size_t len) {
    if (use_tls_) {
        return SSL_write(ssl_, buffer, len);
    }
    return ::write(fd_, buffer, len);
}

int Connection::accept() {
    return ::accept(fd_, nullptr, nullptr);
}