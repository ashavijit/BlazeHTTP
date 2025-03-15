#include "http/static_file.hpp"
#include <fstream>
#include <stdexcept>

StaticFile::StaticFile(const std::string& root_dir) : root_dir_(root_dir) {}

Response StaticFile::serve(const std::string& path) {
    std::string full_path = root_dir_ + path;
    std::ifstream file(full_path, std::ios::binary);
    if (!file.is_open()) {
        return Response{404, "Not Found", "HTTP/1.1", {}, "File not found"};
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return Response{200, "OK", "HTTP/1.1", {{"Content-Type", "text/plain"}}, content};
}