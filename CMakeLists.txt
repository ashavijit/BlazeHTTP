cmake_minimum_required(VERSION 3.10)
project(BlazeHTTP)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)

find_library(NGHTTP2_LIBRARY NAMES nghttp2)
if(NOT NGHTTP2_LIBRARY)
    message(FATAL_ERROR "nghttp2 library not found. Please install libnghttp2-dev.")
endif()

include_directories(include)

add_executable(http_server
    src/main.cpp
    src/core/event_loop.cpp
    src/core/worker_pool.cpp
    src/core/connection.cpp
    src/http/http_parser.cpp
    src/http/static_file.cpp
    src/proxy/l7_proxy.cpp
    src/http/cache.cpp
)

target_link_libraries(http_server PRIVATE OpenSSL::SSL OpenSSL::Crypto ${NGHTTP2_LIBRARY})