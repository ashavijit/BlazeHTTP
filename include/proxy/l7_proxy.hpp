#ifndef L7_PROXY_HPP
#define L7_PROXY_HPP

#include "http/request.hpp"
#include "http/response.hpp"
#include <string>

class L7Proxy {
public:
    L7Proxy(const std::string& backend_host, int backend_port);
    Response forward(const Request& request);

private:
    std::string backend_host_;
    int backend_port_;
};

#endif // L7_PROXY_HPP