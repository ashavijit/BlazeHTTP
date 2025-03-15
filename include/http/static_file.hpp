#ifndef STATIC_FILE_HPP
#define STATIC_FILE_HPP

#include "http/response.hpp"
#include <string>

class StaticFile {
public:
    StaticFile(const std::string& root_dir);
    Response serve(const std::string& path);

private:
    std::string root_dir_;
};

#endif // STATIC_FILE_HPP