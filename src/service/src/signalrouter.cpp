#include "../include/signalrouter.hpp"

#include <unistd.h>

#include <csignal>

SignalRouter::SignalRouter() {
  sigemptyset(&signal_mask_);
  epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
  if (epoll_fd_ == -1) {
    throw std::system_error(errno, std::system_category(), "epoll_create1");
  }
}

SignalRouter::~SignalRouter() {
  stop();
  close(epoll_fd_);
  if (signal_fd_ != -1) close(signal_fd_);
}

void SignalRouter::setupSignalFD() {
  if (signal_fd_ != -1) close(signal_fd_);

  signal_fd_ = signalfd(-1, &signal_mask_, SFD_NONBLOCK | SFD_CLOEXEC);
  if (signal_fd_ == -1) {
    throw std::system_error(errno, std::system_category(), "signalfd");
  }

  epoll_event event{};
  event.events = EPOLLIN;
  event.data.fd = signal_fd_;
  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, signal_fd_, &event) == -1) {
    throw std::system_error(errno, std::system_category(), "epoll_ctl");
  }
}

void SignalRouter::registerHandler(int signum, std::function<void()> handler) {
  std::lock_guard lock(mutex_);

  if (sigaddset(&signal_mask_, signum) == -1) {
    throw std::system_error(errno, std::system_category(), "sigaddset");
  }

  handlers_[signum] = std::move(handler);
  updateSignalMask();
}

void SignalRouter::updateSignalMask() {
  if (sigprocmask(SIG_BLOCK, &signal_mask_, nullptr) == -1) {
    throw std::system_error(errno, std::system_category(), "sigprocmask");
  }
  setupSignalFD();
}

void SignalRouter::start() {
  if (running_) return;

  running_ = true;
  worker_thread_ = std::thread([this] {
    constexpr int max_events = 10;
    epoll_event events[max_events];

    while (running_) {
      int num_events = epoll_wait(epoll_fd_, events, max_events, 100);
      if (num_events == -1 && errno != EINTR) {
        break;
      }

      for (int i = 0; i < num_events; ++i) {
        if (events[i].data.fd == signal_fd_) {
          signalfd_siginfo info;
          while (read(signal_fd_, &info, sizeof(info)) == sizeof(info)) {
            std::lock_guard lock(mutex_);
            if (auto it = handlers_.find(info.ssi_signo);
                it != handlers_.end()) {
              it->second();
            }
          }
        }
      }
    }
  });
}

void SignalRouter::stop() {
  running_ = false;
  if (worker_thread_.joinable()) {
    worker_thread_.join();
  }
}