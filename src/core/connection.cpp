#include "core/connection.hpp"
#include <stdexcept>
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <openssl/err.h> // Added for ERR_print_errors_fp

Connection::Connection(int fd, bool use_tls, const std::string& cert_file, const std::string& key_file)
    : fd_(fd), use_tls_(use_tls), ssl_ctx_(nullptr), ssl_(nullptr), is_http2_(false) {
    if (use_tls_) {
        SSL_load_error_strings();
        OpenSSL_add_ssl_algorithms();
        ssl_ctx_ = SSL_CTX_new(TLS_server_method());
        if (!ssl_ctx_) {
            ERR_print_errors_fp(stderr);
            throw std::runtime_error("Failed to create SSL context");
        }

        if (SSL_CTX_use_certificate_file(ssl_ctx_, cert_file.c_str(), SSL_FILETYPE_PEM) <= 0) {
            ERR_print_errors_fp(stderr);
            throw std::runtime_error("Failed to load certificate file: " + cert_file);
        }
        if (SSL_CTX_use_PrivateKey_file(ssl_ctx_, key_file.c_str(), SSL_FILETYPE_PEM) <= 0) {
            ERR_print_errors_fp(stderr);
            throw std::runtime_error("Failed to load private key file: " + key_file);
        }

        if (SSL_CTX_check_private_key(ssl_ctx_) != 1) {
            ERR_print_errors_fp(stderr);
            throw std::runtime_error("Private key does not match certificate");
        }

        // Enable ALPN with HTTP/2 and HTTP/1.1
        const unsigned char *alpn = (const unsigned char *)"\x02h2\x08http/1.1";
        unsigned int alpnlen = 11; // Length of "\x02h2\x08http/1.1"
        SSL_CTX_set_alpn_protos(ssl_ctx_, alpn, alpnlen);

        ssl_ = SSL_new(ssl_ctx_);
        if (!ssl_) {
            ERR_print_errors_fp(stderr);
            throw std::runtime_error("Failed to create SSL object");
        }

        SSL_set_fd(ssl_, fd_);
        int ssl_err = SSL_accept(ssl_);
        if (ssl_err <= 0) {
            ERR_print_errors_fp(stderr);
            int err_code = SSL_get_error(ssl_, ssl_err);
            throw std::runtime_error("SSL handshake failed with error code: " + std::to_string(err_code));
        }

        // Check negotiated protocol
        const unsigned char *negotiated_proto;
        unsigned int proto_len;
        SSL_get0_alpn_selected(ssl_, &negotiated_proto, &proto_len);
        if (negotiated_proto) {
            std::string proto(reinterpret_cast<const char*>(negotiated_proto), proto_len);
            std::cout << "Negotiated ALPN protocol: " << proto << std::endl;
            is_http2_ = (proto == "h2");
        } else {
            std::cout << "No ALPN protocol negotiated, defaulting to HTTP/1.1" << std::endl;
            is_http2_ = false;
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
        ssize_t bytes_read = SSL_read(ssl_, buffer, len);
        if (bytes_read <= 0) {
            int ssl_err = SSL_get_error(ssl_, bytes_read);
            ERR_print_errors_fp(stderr);
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
            ERR_print_errors_fp(stderr);
            std::cerr << "SSL_write failed with error code: " << ssl_err << std::endl;
        }
        return bytes_written;
    }
    return ::write(fd_, buffer, len);
}

int Connection::accept() {
    return ::accept(fd_, nullptr, nullptr);
}

bool Connection::is_http2() const {
    return is_http2_;
}