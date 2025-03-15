#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <string>
#include <functional>

class Connection {
public:
    Connection(int fd, bool use_tls, const std::string& cert_file, const std::string& key_file);
    ~Connection();

    ssize_t read(char* buffer, size_t len);

    ssize_t write(const char* buffer, size_t len);

    int accept();

private:
    int fd_;
    bool use_tls_;
    SSL_CTX* ssl_ctx_;
    SSL* ssl_;
};

#endif // CONNECTION_HPP