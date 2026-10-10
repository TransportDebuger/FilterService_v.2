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
        auto deadline = std::chrono::steady_clock::now() + polling_interval_;
        while (std::chrono::steady_clock::now() < deadline && !stoken.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (stoken.stop_requested()) break;

        if (!std::filesystem::exists(path_)) {
            throw std::runtime_error("PollingMonitor: Path disappeared: " + path_);
        }

        try {
            // 1. Формируем текущий снимок директории
            std::unordered_map<std::string, FileState> current_files;
            for (const auto& entry : std::filesystem::directory_iterator(path_)) {
                if (entry.is_regular_file()) {
                    try {
                        current_files[entry.path().string()] = {
                            entry.file_size(),
                            entry.last_write_time()
                        };
                    } catch (const std::filesystem::filesystem_error&) {
                        // Файл мог быть удален между итерациями iterator
                        continue;
                    }
                }
            }

            // 2. Обработка известных (уже стабилизированных) файлов
            for (auto it = known_files_.begin(); it != known_files_.end(); ) {
                auto curr_it = current_files.find(it->first);
                if (curr_it == current_files.end()) {
                    callback_(Event::Deleted, it->first);
                    it = known_files_.erase(it);
                } else {
                    if (curr_it->second.mtime != it->second) {
                        callback_(Event::Modified, it->first);
                        it->second = curr_it->second.mtime; // Обновляем mtime
                    }
                    ++it;
                }
            }

            // 3. Обработка новых и ожидающих (pending) файлов
            for (const auto& [file, state] : current_files) {
                if (known_files_.find(file) != known_files_.end()) {
                    continue; // Файл уже стабилен и обработан
                }

                auto pending_it = pending_files_.find(file);
                if (pending_it == pending_files_.end()) {
                    // Первый раз видим файл — добавляем в pending
                    pending_files_[file] = state;
                } else {
                    // Второй раз видим файл — сравниваем состояние
                    if (pending_it->second.size == state.size && 
                        pending_it->second.mtime == state.mtime) {
                        // Состояние не изменилось за интервал polling_interval — файл готов
                        callback_(Event::Created, file);
                        known_files_[file] = state.mtime;
                        pending_files_.erase(pending_it);
                    } else {
                        // Файл все еще пишется — обновляем pending-состояние
                        pending_it->second = state;
                    }
                }
            }

            // 4. Очистка pending-файлов, которые исчезли из директории
            for (auto it = pending_files_.begin(); it != pending_files_.end(); ) {
                if (current_files.find(it->first) == current_files.end()) {
                    it = pending_files_.erase(it);
                } else {
                    ++it;
                }
            }

        } catch (const std::filesystem::filesystem_error& e) {
            throw std::runtime_error("PollingMonitor filesystem error: " + std::string(e.what()));
        }
    }
}

std::exception_ptr PollingMonitor::GetException() const noexcept {
  std::lock_guard<std::mutex> lock(exception_mutex_);
  return exception_;
}

}  // namespace stc::fs