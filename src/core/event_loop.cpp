#include "core/event_loop.hpp"
#include <stdexcept>
#include <iostream>
#include <unistd.h> // For close

EventLoop::EventLoop() {
#ifdef __linux__
    event_fd_ = epoll_create1(0);
    if (event_fd_ == -1) {
        throw std::runtime_error("Failed to create epoll instance");
    }
#elif defined(__APPLE__) || defined(__FreeBSD__)
    event_fd_ = kqueue();
    if (event_fd_ == -1) {
        throw std::runtime_error("Failed to create kqueue instance");
    }
#endif
}

EventLoop::~EventLoop() {
    close(event_fd_);
}

void EventLoop::addFd(int fd, uint32_t events, std::function<void(int, uint32_t)> callback) {
#ifdef __linux__
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    if (epoll_ctl(event_fd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
        throw std::runtime_error("Failed to add fd to epoll");
    }
#elif defined(__APPLE__) || defined(__FreeBSD__)
    struct kevent ev;
    EV_SET(&ev, fd, EVFILT_READ, EV_ADD, 0, 0, nullptr);
    if (kevent(event_fd_, &ev, 1, nullptr, 0, nullptr) == -1) {
        throw std::runtime_error("Failed to add fd to kqueue");
    }
#endif
    callbacks_.push_back(std::move(callback));
}

void EventLoop::run() {
    const int MAX_EVENTS = 100;
#ifdef __linux__
    struct epoll_event events[MAX_EVENTS];
    while (true) {
        int nfds = epoll_wait(event_fd_, events, MAX_EVENTS, -1);
        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;
            uint32_t ev = events[i].events;
            if (fd < callbacks_.size()) {
                callbacks_[fd](fd, ev);
            }
        }
    }
#elif defined(__APPLE__) || defined(__FreeBSD__)
    struct kevent events[MAX_EVENTS];
    while (true) {
        int nfds = kevent(event_fd_, nullptr, 0, events, MAX_EVENTS, nullptr);
        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].ident;
            uint32_t ev = events[i].filter;
            if (fd < callbacks_.size()) {
                callbacks_[fd](fd, ev);
            }
        }
    }
#endif
}