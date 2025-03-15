#ifndef EVENT_LOOP_HPP
#define EVENT_LOOP_HPP

#include <functional>
#include <memory>
#include <vector>

#ifdef __linux__
#include <sys/epoll.h>
#elif defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/event.h>
#endif

class EventLoop {
public:
    EventLoop();
    ~EventLoop();

    void addFd(int fd, uint32_t events, std::function<void(int, uint32_t)> callback);

    void removeFd(int fd);

    void run();

private:
    int event_fd_; 
    std::vector<std::function<void(int, uint32_t)>> callbacks_;
};

#endif // EVENT_LOOP_HPP