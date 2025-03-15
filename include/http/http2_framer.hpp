#ifndef HTTP2_FRAMER_HPP
#define HTTP2_FRAMER_HPP

#include <string>
#include <vector>

class Http2Framer {
public:
    Http2Framer() = default;

    void parseFrames(const std::string& data);

    std::string generateFrame(const std::string& payload, uint8_t type, uint32_t stream_id);
};

#endif // HTTP2_FRAMER_HPP