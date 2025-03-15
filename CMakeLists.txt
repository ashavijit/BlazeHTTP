cmake_minimum_required(VERSION 3.10)
project(HttpServer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)

# Add the include directory to the include path
include_directories(${CMAKE_SOURCE_DIR}/include)

file(GLOB_RECURSE SOURCES src/*.cpp)

add_executable(http_server ${SOURCES})

target_link_libraries(http_server OpenSSL::SSL OpenSSL::Crypto pthread)