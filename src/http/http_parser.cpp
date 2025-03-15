#include "http/http_parser.hpp"
#include "core/connection.hpp"
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <nghttp2/nghttp2.h>
#include <cstring> // For strlen

struct HttpParser::Impl
{
    nghttp2_session *session = nullptr;
    Request current_request;

    Impl()
    {
        nghttp2_session_callbacks *callbacks;
        nghttp2_session_callbacks_new(&callbacks);
        nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks, on_frame_recv);
        nghttp2_session_callbacks_set_on_header_callback(callbacks, on_header);
        nghttp2_session_callbacks_set_on_data_chunk_recv_callback(callbacks, on_data_chunk);
        nghttp2_session_callbacks_set_on_begin_headers_callback(callbacks, on_begin_headers);
        nghttp2_session_server_new(&session, callbacks, this);
        nghttp2_session_callbacks_del(callbacks);
    }

    ~Impl()
    {
        if (session)
            nghttp2_session_del(session);
    }

    static int on_begin_headers(nghttp2_session *session, const nghttp2_frame *frame, void *user_data)
    {
        auto *impl = static_cast<Impl *>(user_data);
        impl->current_request = Request{}; // Reset for new request
        return 0;
    }

    static int on_frame_recv(nghttp2_session *session, const nghttp2_frame *frame, void *user_data)
    {
        auto *impl = static_cast<Impl *>(user_data);
        if (frame->hd.type == NGHTTP2_HEADERS && frame->headers.cat == NGHTTP2_HCAT_REQUEST)
        {
            std::cout << "HTTP/2 HEADERS frame received" << std::endl;
        }
        return 0;
    }

    static int on_header(nghttp2_session *session, const nghttp2_frame *frame,
                         const uint8_t *name, size_t namelen,
                         const uint8_t *value, size_t valuelen,
                         uint8_t flags, void *user_data)
    {
        auto *impl = static_cast<Impl *>(user_data);
        std::string header_name(reinterpret_cast<const char *>(name), namelen);
        std::string header_value(reinterpret_cast<const char *>(value), valuelen);

        if (header_name == ":method")
            impl->current_request.method = header_value;
        else if (header_name == ":path")
            impl->current_request.path = header_value;
        else if (header_name == ":scheme")
        {
        } // Ignore for now
        else if (header_name == ":authority")
            impl->current_request.headers["Host"] = header_value;
        else
            impl->current_request.headers[header_name] = header_value;

        std::cout << "HTTP/2 Header: " << header_name << ": " << header_value << std::endl;
        return 0;
    }

    static int on_data_chunk(nghttp2_session *session, uint8_t flags, int32_t stream_id,
                             const uint8_t *data, size_t len, void *user_data)
    {
        auto *impl = static_cast<Impl *>(user_data);
        impl->current_request.body.append(reinterpret_cast<const char *>(data), len);
        std::cout << "HTTP/2 Data chunk: " << std::string(reinterpret_cast<const char *>(data), len) << std::endl;
        return 0;
    }
};

HttpParser::HttpParser() : pimpl_(std::make_unique<Impl>()) {}

HttpParser::~HttpParser() = default;

Request HttpParser::parseRequest(const std::string &data)
{
    // HTTP/1.1 parsing only
    std::istringstream stream(data);
    std::string line;
    std::getline(stream, line);

    std::istringstream request_line(line);
    std::string method, path, version;
    request_line >> method >> path >> version;

    if (method.empty() || path.empty() || version.empty())
    {
        throw std::runtime_error("Invalid HTTP/1.1 request line");
    }

    Request request;
    request.method = method;
    request.path = path;
    request.version = version;

    while (std::getline(stream, line) && line != "\r")
    {
        auto pos = line.find(':');
        if (pos != std::string::npos)
        {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 2, line.size() - pos - 3); // Remove \r\n
            request.headers[key] = value;
        }
    }

    std::string body;
    while (std::getline(stream, line))
    {
        body += line + "\n";
    }
    request.body = body;

    std::cout << "Parsed HTTP/1.1 request: " << method << " " << path << " " << version << std::endl;
    return request;
}

std::string HttpParser::generateResponse(const Response &response)
{
    // HTTP/1.1 response generation only
    std::stringstream ss;
    ss << response.version << " " << response.status_code << " " << response.status_message << "\r\n";

    for (const auto &header : response.headers)
    {
        ss << header.first << ": " << header.second << "\r\n";
    }

    ss << "\r\n"
       << response.body;
    return ss.str();
}

Request HttpParser::parseRequest(const std::string &data, const Connection &conn)
{
    if (conn.is_http2())
    {
        ssize_t readlen = nghttp2_session_mem_recv(pimpl_->session,
                                                   reinterpret_cast<const uint8_t *>(data.data()),
                                                   data.size());
        if (readlen < 0)
        {
            throw std::runtime_error("nghttp2_session_mem_recv failed: " +
                                     std::string(nghttp2_strerror(readlen)));
        }
        return pimpl_->current_request;
    }
    else
    {
        std::istringstream stream(data);
        std::string line;
        std::getline(stream, line);

        std::istringstream request_line(line);
        std::string method, path, version;
        request_line >> method >> path >> version;

        if (method.empty() || path.empty() || version.empty())
        {
            throw std::runtime_error("Invalid HTTP/1.1 request line");
        }

        Request request;
        request.method = method;
        request.path = path;
        request.version = version;

        while (std::getline(stream, line) && line != "\r")
        {
            auto pos = line.find(':');
            if (pos != std::string::npos)
            {
                std::string key = line.substr(0, pos);
                std::string value = line.substr(pos + 2, line.size() - pos - 3); // Remove \r\n
                request.headers[key] = value;
            }
        }

        std::string body;
        while (std::getline(stream, line))
        {
            body += line + "\n";
        }
        request.body = body;

        std::cout << "Parsed HTTP/1.1 request: " << method << " " << path << " " << version << std::endl;
        return request;
    }
}

std::string HttpParser::generateResponse(const Response &response, const Connection &conn)
{
    if (conn.is_http2())
    {
        std::string status_str = std::to_string(response.status_code);
        std::string content_type;
        try
        {
            content_type = response.headers.at("Content-Type");
        }
        catch (const std::out_of_range &)
        {
            content_type = "text/html";
        }

        nghttp2_nv hdrs[] = {
            {
                (uint8_t *)":status",          // name
                (uint8_t *)status_str.c_str(), // value
                strlen(":status"),             // namelen
                status_str.size(),             // valuelen
                NGHTTP2_NV_FLAG_NONE           // flags
            },
            {
                (uint8_t *)"content-type",       // name
                (uint8_t *)content_type.c_str(), // value
                strlen("content-type"),          // namelen
                content_type.size(),             // valuelen
                NGHTTP2_NV_FLAG_NONE             // flags
            }};

        int stream_id = 1;
        nghttp2_data_provider data_prd{};
        std::string response_body = response.body;
        data_prd.source.ptr = &response_body;
        data_prd.read_callback = [](nghttp2_session *session, int32_t stream_id, uint8_t *buf,
                                    size_t length, uint32_t *data_flags, nghttp2_data_source *source,
                                    void *user_data) -> ssize_t
        {
            std::string *body = static_cast<std::string *>(source->ptr);
            size_t len = std::min(length, body->size());
            if (len == 0)
            {
                *data_flags |= NGHTTP2_DATA_FLAG_EOF;
                return 0;
            }
            memcpy(buf, body->c_str(), len);
            body->erase(0, len);
            if (body->empty())
                *data_flags |= NGHTTP2_DATA_FLAG_EOF;
            return static_cast<ssize_t>(len);
        };

        int rv = nghttp2_submit_response(pimpl_->session, stream_id, hdrs, 2, &data_prd);
        if (rv != 0)
        {
            throw std::runtime_error("nghttp2_submit_response failed: " + std::string(nghttp2_strerror(rv)));
        }

        std::string response_data;
        while (nghttp2_session_want_write(pimpl_->session))
        {
            const uint8_t *data_ptr;
            ssize_t data_len = nghttp2_session_mem_send(pimpl_->session, &data_ptr);
            if (data_len < 0)
            {
                throw std::runtime_error("nghttp2_session_mem_send failed: " + std::string(nghttp2_strerror(data_len)));
            }
            if (data_len == 0)
                break;
            response_data.append(reinterpret_cast<const char *>(data_ptr), data_len);
        }

        std::cout << "Generated HTTP/2 response" << std::endl;
        return response_data;
    }
    else
    {
        std::ostringstream stream;
        stream << response.version << " " << response.status_code << " " << response.status_message << "\r\n";
        for (const auto &header : response.headers)
        {
            stream << header.first << ": " << header.second << "\r\n";
        }
        stream << "Content-Length: " << response.body.size() << "\r\n";
        stream << "\r\n"
               << response.body;

        std::string response_data = stream.str();
        std::cout << "Generated HTTP/1.1 response: " << response_data << std::endl;
        return response_data;
    }
}