#ifndef WORKER_POOL_HPP
#define WORKER_POOL_HPP

#include <thread>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <vector>

class WorkerPool {
public:
    WorkerPool(size_t num_workers);
    ~WorkerPool();

    void submit(std::function<void()> task);

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    bool stop_;
};

#endif // WORKER_POOL_HPP