#ifndef CACHE_HPP
#define CACHE_HPP

#include <unordered_map>
#include <string>
#include <mutex>

class Cache {
public:
    Cache(size_t max_size);
    bool get(const std::string& key, std::string& value);
    void put(const std::string& key, const std::string& value);

private:
    std::unordered_map<std::string, std::string> cache_;
    std::mutex mutex_;
    size_t max_size_;
};

#endif // CACHE_HPP