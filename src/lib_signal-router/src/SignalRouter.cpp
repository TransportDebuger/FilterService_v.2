#include "stc/SignalRouter.hpp"

#include <sys/epoll.h>

#include <iostream>

namespace stc {

SignalRouter::SignalRouter() {
  sigemptyset(&blocked_mask_);
  if (sigprocmask(SIG_SETMASK, nullptr, &original_mask_) == -1) {
    throw std::system_error(errno, std::system_category(),
                            "sigprocmask(GET) failed");
  }
  if ((signal_fd_ = signalfd(-1, &blocked_mask_, SFD_NONBLOCK | SFD_CLOEXEC)) ==
      -1)
    throw std::system_error(errno, std::system_category(),
                            "signalfd create failed");
}

void SignalRouter::registerHandler(int signum, Handler handler) {
  if (signum <= 0 || signum >= NSIG) {
    throw std::invalid_argument("Invalid signal number");
  }

  std::lock_guard<std::mutex> lock(handlers_mutex_);

  sigset_t single_mask;
  sigemptyset(&single_mask);
  sigaddset(&single_mask, signum);
  pthread_sigmask(SIG_BLOCK, &single_mask, nullptr);

  sigaddset(&blocked_mask_, signum);

  pthread_sigmask(SIG_BLOCK, &blocked_mask_, nullptr);

  // Обновляем signalfd
  if (signalfd(signal_fd_, &blocked_mask_, 0) == -1) {
    throw std::system_error(errno, std::system_category(),
                            "signalfd configure failed");
  }

  handlers_[signum].push_back(std::move(handler));
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
      throw std::system_error(errno, std::system_category(),
                              "epoll_create1 failed");
    }

    ev.events = EPOLLIN;
    ev.data.fd = signal_fd_;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, signal_fd_, &ev) == -1) {
      close(epoll_fd);
      throw std::system_error(errno, std::system_category(),
                              "epoll_ctl failed");
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

}  // namespace stc