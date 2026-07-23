/**
@file polling_monitor.cpp
@brief Реализация универсального монитора на базе опроса.
@version 1.1.1
@date 2026-07-22
*/
#include "polling_monitor.hpp"

#include <filesystem>
#include <stdexcept>

namespace stc::fs {

PollingMonitor::PollingMonitor(const std::string& path, Callback callback,
                               std::chrono::seconds polling_interval)
    : path_(std::filesystem::absolute(path).string()),
      callback_(std::move(callback)),
      polling_interval_(polling_interval) {
  if (!std::filesystem::exists(path_) ||
      !std::filesystem::is_directory(path_)) {
    throw std::runtime_error(
        "PollingMonitor: Path does not exist or is not a directory: " +
        path_);  // LCOV_EXCL_LINE
  }
}

PollingMonitor::~PollingMonitor() { PollingMonitor::Stop(); }

void PollingMonitor::Start() {
  if (worker_thread_.joinable()) return;

  for (const auto& entry : std::filesystem::directory_iterator(path_)) {
    if (entry.is_regular_file()) {
      known_files_[entry.path().string()] = entry.last_write_time();
    }
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

void PollingMonitor::Stop() {
  if (!worker_thread_.joinable()) return;

  worker_thread_.request_stop();
  worker_thread_.join();
}

void PollingMonitor::Run(std::stop_token stoken) {
  while (!stoken.stop_requested()) {
    // Использование deadline вместо ручного подсчета elapsed устраняет
    // артефакты покрытия кода и повышает устойчивость к задержкам планировщика
    // ОС.
    auto deadline = std::chrono::steady_clock::now() + polling_interval_;
    while (std::chrono::steady_clock::now() < deadline &&
           !stoken.stop_requested()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (stoken.stop_requested()) break;

    if (!std::filesystem::exists(path_)) {
      throw std::runtime_error("PollingMonitor: Path disappeared: " + path_);
    }

    try {
      std::unordered_map<std::string, std::filesystem::file_time_type>
          current_files;
      for (const auto& entry : std::filesystem::directory_iterator(path_)) {
        if (entry.is_regular_file()) {
          current_files[entry.path().string()] = entry.last_write_time();
        }
      }

      for (const auto& [file, time] : current_files) {
        auto it = known_files_.find(file);
        if (it == known_files_.end()) {
          callback_(Event::Created, file);
        } else if (it->second != time) {
          callback_(Event::Modified, file);  // <-- Вероятно, это была строка 64
        }
      }

      for (const auto& [file, time] : known_files_) {
        if (current_files.find(file) == current_files.end()) {
          callback_(Event::Deleted, file);
        }
      }

      known_files_ = std::move(current_files);

    } catch (const std::filesystem::filesystem_error& e) {
      throw std::runtime_error("PollingMonitor filesystem error: " +
                               std::string(e.what()));
    }
  }
}

std::exception_ptr PollingMonitor::GetException() const noexcept {
  std::lock_guard<std::mutex> lock(exception_mutex_);
  return exception_;
}

}  // namespace stc::fs