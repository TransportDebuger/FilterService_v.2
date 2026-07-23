/**
@file inotify_monitor.cpp
@brief Реализация событийного монитора на базе inotify.
@version 1.1.2
@date 2026-07-22
*/
#include "inotify_monitor.hpp"

#include <sys/inotify.h>

#include <cerrno>
#include <filesystem>
#include <stdexcept>
#include <system_error>

namespace stc::fs {

InotifyMonitor::InotifyMonitor(
    const std::string& path, Callback callback,
    std::shared_ptr<IFileSystemSystemCalls> sys_calls)
    : path_(std::filesystem::absolute(path).string()),
      callback_(std::move(callback)),
      sys_calls_(std::move(sys_calls)) {
  if (!sys_calls_) {
    throw std::invalid_argument(
        "InotifyMonitor: System calls interface cannot be null");
  }
}

InotifyMonitor::~InotifyMonitor() { InotifyMonitor::Stop(); }

void InotifyMonitor::Start() {
  if (inotify_fd_ != -1) return;

  inotify_fd_ = sys_calls_->Init();
  if (inotify_fd_ < 0) {
    throw std::system_error(
        errno, std::system_category(),
        "InotifyMonitor: Failed to initialize inotify for " + path_);
  }

  uint32_t mask =
      IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO;
  watch_descriptor_ = sys_calls_->AddWatch(inotify_fd_, path_, mask);
  if (watch_descriptor_ < 0) {
    sys_calls_->Close(inotify_fd_);
    inotify_fd_ = -1;
    throw std::system_error(errno, std::system_category(),
                            "InotifyMonitor: Failed to add watch for " + path_);
  }

  // Перехватываем исключения на границе потока, чтобы избежать std::terminate
  worker_thread_ = std::jthread([this](std::stop_token stoken) {
    try {
      Run(stoken);
    } catch (...) {
      std::lock_guard<std::mutex> lock(exception_mutex_);
      exception_ = std::current_exception();
    }
  });
}

void InotifyMonitor::Stop() {
  if (inotify_fd_ == -1) return;

  worker_thread_.request_stop();

  if (watch_descriptor_ >= 0) {
    sys_calls_->RemoveWatch(inotify_fd_, watch_descriptor_);
    watch_descriptor_ = -1;
  }
  sys_calls_->Close(inotify_fd_);
  inotify_fd_ = -1;

  if (worker_thread_.joinable()) {
    worker_thread_.join();
  }
}

void InotifyMonitor::Run(std::stop_token stoken) {
  constexpr std::size_t kBufferSize = 4096;
  alignas(struct inotify_event) char buffer[kBufferSize];

  while (!stoken.stop_requested()) {
    ssize_t length = sys_calls_->Read(inotify_fd_, buffer, kBufferSize);

    if (length < 0) {
      if (stoken.stop_requested()) break;
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        continue;
      }
      throw std::system_error(errno, std::system_category(),
                              "InotifyMonitor: read failed");
    }

    std::size_t offset = 0;
    while (offset < static_cast<std::size_t>(length)) {
      const struct inotify_event* event =
          reinterpret_cast<const struct inotify_event*>(&buffer[offset]);

      if (event->len > 0) {
        std::string full_path =
            (std::filesystem::path(path_) / event->name).string();

        if (event->mask & (IN_CREATE | IN_MOVED_TO)) {
          callback_(Event::Created, full_path);
        }
        if (event->mask & (IN_DELETE | IN_MOVED_FROM)) {
          callback_(Event::Deleted, full_path);
        }
        if (event->mask & IN_MODIFY) {
          callback_(Event::Modified, full_path);
        }
      }
      offset += sizeof(struct inotify_event) + event->len;
    }
  }
}

std::exception_ptr InotifyMonitor::GetException() const noexcept {
  std::lock_guard<std::mutex> lock(exception_mutex_);
  return exception_;
}

}  // namespace stc::fs