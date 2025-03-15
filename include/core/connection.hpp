#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <string>
#include <openssl/ssl.h>

class Connection
{
public:
    Connection(int fd, bool use_tls, const std::string &cert_file, const std::string &key_file);
    ~Connection();

    ssize_t read(char *buffer, size_t len);
    ssize_t write(const char *buffer, size_t len);
    int accept();
    bool is_http2() const;
    bool is_websocket() const;
    void set_websocket(bool value);

private:
    int fd_;
    bool use_tls_;
    SSL_CTX *ssl_ctx_;
    SSL *ssl_;
    bool is_http2_;
    bool is_websocket_ = false;
};

#endif // CONNECTION_HPP