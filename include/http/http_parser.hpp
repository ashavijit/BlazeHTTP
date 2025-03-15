#ifndef HTTP_PARSER_HPP
#define HTTP_PARSER_HPP

#include <string>
#include <unordered_map>
#include <memory>

struct Request {
    std::string method;
    std::string path;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
};

struct Response {
    int status_code;
    std::string status_message;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
};

class Connection; // Forward declaration

class HttpParser {
public:
    HttpParser();
    ~HttpParser();

    Request parseRequest(const std::string& data, const Connection& conn);
    std::string generateResponse(const Response& response, const Connection& conn);

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_; // Pimpl idiom for nghttp2 session
};

#endif // HTTP_PARSER_HPP