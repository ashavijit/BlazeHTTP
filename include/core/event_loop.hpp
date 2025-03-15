#ifndef EVENT_LOOP_HPP
#define EVENT_LOOP_HPP

#include <functional>
#include <memory>
#include <unordered_map>

#ifdef __linux__
#include <sys/epoll.h>
#elif defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/event.h>
#endif

class EventLoop {
public:
    EventLoop();
    ~EventLoop();

    // Add a file descriptor to monitor
    void addFd(int fd, uint32_t events, std::function<void(int, uint32_t)> callback);

    // Remove a file descriptor from monitoring
    void removeFd(int fd);

    // Run the event loop
    void run();

private:
    int event_fd_; // epoll or kqueue file descriptor
    std::unordered_map<int, std::function<void(int, uint32_t)>> callbacks_; // Map fd to callback
};

#endif // EVENT_LOOP_HPP