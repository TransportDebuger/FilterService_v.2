#include "stc/SignalRouter.hpp"
#include <sys/epoll.h>
#include <iostream>

SignalRouter::SignalRouter() {
    sigset_t mask;
    sigemptyset(&mask);
    
    // Блокируем все сигналы по умолчанию
    if (sigprocmask(SIG_BLOCK, &mask, &original_mask_) == -1) {
        throw std::system_error(errno, std::system_category(), "sigprocmask failed");
    }
    
    // Создаем signalfd
    if ((signal_fd_ = signalfd(-1, &mask, SFD_NONBLOCK)) == -1) {
        throw std::system_error(errno, std::system_category(), "signalfd failed");
    }
}

void SignalRouter::registerHandler(int signum, Handler handler) {
    if (signum <= 0 || signum >= NSIG) {
        throw std::invalid_argument("Invalid signal number");
    }
    
    std::lock_guard lock(handlers_mutex_);
    
    // Добавляем сигнал в маску
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, signum);
    
    if (sigprocmask(SIG_BLOCK, &mask, nullptr) == -1) {
        throw std::system_error(errno, std::system_category(), "sigprocmask failed");
    }
    
    // Обновляем signalfd
    if (signalfd(signal_fd_, &mask, 0) == -1) {
        throw std::system_error(errno, std::system_category(), "signalfd configure failed");
    }
    
    handlers_[signum].push_back(handler);
}

void SignalRouter::unregisterHandler(int signum) {
    std::lock_guard lock(handlers_mutex_);
    handlers_.erase(signum);
}

void SignalRouter::start() {
    if (running_.exchange(true)) return;
    
    worker_thread_ = std::thread([this] {
        constexpr int MAX_EVENTS = 10;
        struct epoll_event ev, events[MAX_EVENTS];
        
        int epoll_fd = epoll_create1(0);
        if (epoll_fd == -1) {
            throw std::system_error(errno, std::system_category(), "epoll_create1 failed");
        }
        
        ev.events = EPOLLIN;
        ev.data.fd = signal_fd_;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, signal_fd_, &ev) == -1) {
            close(epoll_fd);
            throw std::system_error(errno, std::system_category(), "epoll_ctl failed");
        }
        
        while (running_) {
            int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, 500);
            if (nfds == -1) {
                if (errno == EINTR) continue;
                break;
            }
            
            for (int i = 0; i < nfds; ++i) {
                if (events[i].data.fd == signal_fd_) {
                    struct signalfd_siginfo fdsi;
                    ssize_t bytes = read(signal_fd_, &fdsi, sizeof(fdsi));
                    if (bytes != sizeof(fdsi)) continue;
                    
                    std::lock_guard lock(handlers_mutex_);
                    if (auto it = handlers_.find(fdsi.ssi_signo); it != handlers_.end()) {
                        for (auto& handler : it->second) {
                            handler(fdsi.ssi_signo);
                        }
                    }
                }
            }
        }
        
        close(epoll_fd);
    });
}

void SignalRouter::stop() noexcept {
    running_ = false;
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

SignalRouter::~SignalRouter() {
    stop();
    close(signal_fd_);
    sigprocmask(SIG_SETMASK, &original_mask_, nullptr);
}