#include "http/http_parser.hpp"
#include <sstream>
#include <stdexcept>

Request HttpParser::parseRequest(const std::string& data) {
    std::istringstream stream(data);
    std::string line;
    std::getline(stream, line);

    std::istringstream request_line(line);
    std::string method, path, version;
    request_line >> method >> path >> version;

    if (method.empty() || path.empty() || version.empty()) {
        throw std::runtime_error("Invalid HTTP request line");
    }

    Request request;
    request.method = method;
    request.path = path;
    request.version = version;

    while (std::getline(stream, line) && line != "\r") {
        auto pos = line.find(':');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 2, line.size() - pos - 3); // Remove \r\n
            request.headers[key] = value;
        }
    }

    std::string body;
    while (std::getline(stream, line)) {
        body += line + "\n";
    }
    request.body = body;

    return request;
}

std::string HttpParser::generateResponse(const Response& response) {
    std::ostringstream stream;
    stream << response.version << " " << response.status_code << " " << response.status_message << "\r\n";
    for (const auto& header : response.headers) {
        stream << header.first << ": " << header.second << "\r\n";
    }
    stream << "\r\n" << response.body;
    return stream.str();
}