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
            ERR_print_errors_fp(stderr); // Print OpenSSL errors
            throw std::runtime_error("Failed to create SSL context");
        }

        if (SSL_CTX_use_certificate_file(ssl_ctx_, cert_file.c_str(), SSL_FILETYPE_PEM) <= 0) {
            ERR_print_errors_fp(stderr); // Print OpenSSL errors
            throw std::runtime_error("Failed to load certificate file: " + cert_file);
        }
        if (SSL_CTX_use_PrivateKey_file(ssl_ctx_, key_file.c_str(), SSL_FILETYPE_PEM) <= 0) {
            ERR_print_errors_fp(stderr); // Print OpenSSL errors
            throw std::runtime_error("Failed to load private key file: " + key_file);
        }

        // Verify that the certificate and key match
        if (SSL_CTX_check_private_key(ssl_ctx_) != 1) {
            ERR_print_errors_fp(stderr); // Print OpenSSL errors
            throw std::runtime_error("Private key does not match certificate");
        }

        ssl_ = SSL_new(ssl_ctx_);
        if (!ssl_) {
            ERR_print_errors_fp(stderr); // Print OpenSSL errors
            throw std::runtime_error("Failed to create SSL object");
        }

        SSL_set_fd(ssl_, fd_);
        int ssl_err = SSL_accept(ssl_);
        if (ssl_err <= 0) {
            ERR_print_errors_fp(stderr); // Print OpenSSL errors
            int err_code = SSL_get_error(ssl_, ssl_err);
            std::string err_msg = "SSL handshake failed with error code: " + std::to_string(err_code);
            throw std::runtime_error(err_msg);
        }

        std::cout << "TLS handshake completed successfully for fd " << fd_ << std::endl;
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
        ssize_t bytes_read = SSL_read(ssl_, buffer, len);
        if (bytes_read <= 0) {
            int ssl_err = SSL_get_error(ssl_, bytes_read);
            ERR_print_errors_fp(stderr); // Print OpenSSL errors
            std::cerr << "SSL_read failed with error code: " << ssl_err << std::endl;
        }
        return bytes_read;
    }
    return ::read(fd_, buffer, len);
}

ssize_t Connection::write(const char* buffer, size_t len) {
    if (use_tls_) {
        ssize_t bytes_written = SSL_write(ssl_, buffer, len);
        if (bytes_written <= 0) {
            int ssl_err = SSL_get_error(ssl_, bytes_written);
            ERR_print_errors_fp(stderr); // Print OpenSSL errors
            std::cerr << "SSL_write failed with error code: " << ssl_err << std::endl;
        }
        return bytes_written;
    }
    return ::write(fd_, buffer, len);
}

int Connection::accept() {
    return ::accept(fd_, nullptr, nullptr);
}