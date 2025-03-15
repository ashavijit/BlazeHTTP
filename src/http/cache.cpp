#include "http/cache.hpp"
#include <stdexcept>

Cache::Cache(size_t max_size) : max_size_(max_size) {}

bool Cache::get(const std::string& key, std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        value = it->second;
        return true;
    }
    return false;
}

void Cache::put(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (cache_.size() >= max_size_) {
        cache_.erase(cache_.begin());
    }
    cache_[key] = value;
}