#ifndef HTTP_REQUEST_HPP
#define HTTP_REQUEST_HPP

#include <string>
#include <map>
#include "http_parser.hpp"

class Request {
public:
    std::string method;                     // e.g., "GET", "POST"
    std::string path;                       // e.g., "/index.html"
    std::string version;                    // e.g., "HTTP/1.1"
    std::map<std::string, std::string> headers; // e.g., {"Host": "localhost"}
    std::string body;                       // Request body (if any)
};

#endif // HTTP_REQUEST_HPP