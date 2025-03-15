#include "http/static_file.hpp"
#include <fstream>
#include <stdexcept>
#include <iostream>

StaticFile::StaticFile(const std::string& root_dir) : root_dir_(root_dir) {}

Response StaticFile::serve(const std::string& path) {
    std::cout << "Serving static file for path: " << path << std::endl;

    std::string requested_path = path;
    if (requested_path == "/" || requested_path.empty()) {
        requested_path = "/index.html"; 
    }

    std::string full_path = root_dir_ + requested_path;
    std::ifstream file(full_path, std::ios::binary);
    if (!file.is_open()) {
        std::cout << "File not found: " << full_path << std::endl;
        return Response{404, "Not Found", "HTTP/1.1", {}, "File not found"};
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    std::cout << "Serving file content: " << content << std::endl;
    return Response{200, "OK", "HTTP/1.1", {{"Content-Type", "text/html"}}, content};
}