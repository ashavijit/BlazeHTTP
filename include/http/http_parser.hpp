#ifndef HTTP_PARSER_HPP
#define HTTP_PARSER_HPP

#include "http/request.hpp"
#include "http/response.hpp"
#include <string>

class HttpParser {
public:
    HttpParser() = default;

    Request parseRequest(const std::string& data);

    std::string generateResponse(const Response& response);
};

#endif // HTTP_PARSER_HPP