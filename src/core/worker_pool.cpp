#include "core/worker_pool.hpp"
#include <stdexcept>
#include <iostream>

WorkerPool::WorkerPool(size_t num_workers) : stop_(false) {
    std::cout << "Starting worker pool with " << num_workers << " workers" << std::endl;
    for (size_t i = 0; i < num_workers; ++i) {
        workers_.emplace_back([this, i] {
            // std::cout << "Worker thread " << i << " started" << std::endl;
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queue_mutex_);
                    condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                    if (stop_ && tasks_.empty()) {
                        // std::cout << "Worker thread " << i << " stopping" << std::endl;
                        return;
                    }
                    task = std::move(tasks_.front());
                    tasks_.pop();
                }
                // std::cout << "Worker thread " << i << " processing task" << std::endl;
                task();
            }
        });
    }
}

WorkerPool::~WorkerPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        stop_ = true;
    }
    condition_.notify_all();
    for (auto& worker : workers_) {
        worker.join();
    }
    std::cout << "Worker pool stopped" << std::endl;
}

void WorkerPool::submit(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        tasks_.emplace(std::move(task));
    }
    std::cout << "Task submitted to worker pool" << std::endl;
    condition_.notify_one();
}