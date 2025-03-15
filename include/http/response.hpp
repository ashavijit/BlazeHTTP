#ifndef HTTP_RESPONSE_HPP
#define HTTP_RESPONSE_HPP

#include <string>
#include <map>

class Response {
public:
    int status_code;                        // e.g., 200, 404
    std::string status_message;             // e.g., "OK", "Not Found"
    std::string version;                    // e.g., "HTTP/1.1"
    std::map<std::string, std::string> headers; // e.g., {"Content-Type": "text/html"}
    std::string body;                       // Response body
};

#endif // HTTP_RESPONSE_HPP