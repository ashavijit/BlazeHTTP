#include "http/http2_framer.hpp"
#include <stdexcept>

void Http2Framer::parseFrames(const std::string& data) {
    // Placeholder for HTTP/2 frame parsing
    // This would involve decoding the binary frame format per RFC 7540
    throw std::runtime_error("HTTP/2 frame parsing not implemented");
}

std::string Http2Framer::generateFrame(const std::string& payload, uint8_t type, uint32_t stream_id) {
    // Placeholder for HTTP/2 frame generation
    // This would involve creating a binary frame per RFC 7540
    throw std::runtime_error("HTTP/2 frame generation not implemented");
}