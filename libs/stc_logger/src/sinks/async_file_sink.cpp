#include "stc/logger/sinks/file/async_file_sink.hpp"

#include <filesystem>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace fs = std::filesystem;

namespace stc::logger {

AsyncFileSink::AsyncFileSink(std::string file_path,
                             std::shared_ptr<ILogFormatter> formatter,
                             std::shared_ptr<ILogFilter> filter,
                             std::shared_ptr<IRotationPolicy> rotation_policy,
                             std::size_t max_batch_size,
                             std::chrono::milliseconds flush_interval)
    : file_path_(std::move(file_path)),
      formatter_(std::move(formatter)),
      filter_(std::move(filter)),
      rotation_policy_(std::move(rotation_policy)),
      max_batch_size_(max_batch_size),
      flush_interval_(flush_interval) {
  if (!formatter_) {
    throw std::invalid_argument("AsyncFileSink: formatter cannot be null");
  }

  OpenFile();

  // Запуск фонового потока. std::jthread автоматически остановится и join-ится
  // при уничтожении объекта, но мы должны корректно обработать stop_token.
  worker_thread_ =
      std::jthread([this](std::stop_token stoken) { WorkerLoop(stoken); });
}

AsyncFileSink::~AsyncFileSink() {
  // request_stop() вызывается автоматически деструктором std::jthread,
  // но мы должны разбудить поток, чтобы он мог выйти из ожидания CV.
  queue_cv_.notify_one();

  // std::jthread дожидается завершения потока (join) здесь.
  // WorkerLoop гарантирует, что вся очередь будет записана (drained) перед
  // выходом.

  if (file_stream_.is_open()) {
    file_stream_.flush();
    file_stream_.close();
  }
}

void AsyncFileSink::Write(const LogRecord& /*record*/,
                          std::string_view formatted_message) {
  {
    std::lock_guard lock(queue_mutex_);
    // Копируем string_view в std::string для безопасной передачи в очередь,
    // так как исходная строка может быть уничтожена вызывающим потоком.
    queue_.emplace(formatted_message);
  }
  queue_cv_.notify_one();
}

void AsyncFileSink::Flush() {
  {
    std::lock_guard lock(queue_mutex_);
    flush_requested_ = true;
  }
  queue_cv_.notify_one();

  // Блокируем вызывающий поток до завершения сброса
  std::unique_lock lock(flush_mutex_);
  flush_cv_.wait(lock, [this] { return !flush_requested_.load(); });
}

std::shared_ptr<ILogFormatter> AsyncFileSink::GetFormatter() const noexcept {
  return formatter_;
}

std::shared_ptr<ILogFilter> AsyncFileSink::GetFilter() const noexcept {
  return filter_;
}

void AsyncFileSink::WorkerLoop(std::stop_token stoken) {
  // Буфер для пакетной записи (Batching)
  std::string batch_buffer;
  batch_buffer.reserve(max_batch_size_);

  while (!stoken.stop_requested() || !queue_.empty()) {
    batch_buffer.clear();
    auto now = std::chrono::system_clock::now();

    // 1. Сборка пакета из очереди
    {
      std::unique_lock lock(queue_mutex_);

      // Ждем появления данных или сигнала остановки/flush
      if (queue_.empty() && !stoken.stop_requested() && !flush_requested_) {
        queue_cv_.wait_for(lock, flush_interval_, [&] {
          return !queue_.empty() || stoken.stop_requested() || flush_requested_;
        });
      }

      // Извлекаем сообщения из очереди в локальный буфер
      while (!queue_.empty() && batch_buffer.size() < max_batch_size_) {
        batch_buffer += std::move(queue_.front());
        queue_.pop();
      }
    }

    // 2. Физическая запись пакета на диск (вне блокировки очереди!)
    if (!batch_buffer.empty()) {
      if (!file_stream_.is_open() || !file_stream_.good()) {
        file_stream_.clear();
        if (file_stream_.is_open()) file_stream_.close();
        OpenFile();
      }

      if (file_stream_.is_open()) {
        RotateIfNeeded(now);

        file_stream_.write(batch_buffer.data(),
                           static_cast<std::streamsize>(batch_buffer.size()));
        current_size_ += batch_buffer.size();
      } else {
        std::cerr << "[AsyncFileSink ERROR] Cannot write, file is not open: "
                  << file_path_ << "\n";
      }
    }

    // 3. Обработка запроса на принудительный Flush
    if (flush_requested_) {
      if (file_stream_.is_open()) {
        file_stream_.flush();
      }
      flush_requested_ = false;
      flush_cv_.notify_one();  // Разблокируем поток, вызвавший Flush()
    }
  }
}

void AsyncFileSink::OpenFile() {
  file_stream_.open(file_path_, std::ios::app | std::ios::binary);
  if (!file_stream_.is_open()) {
    std::cerr << "[AsyncFileSink ERROR] Failed to open file: " << file_path_
              << "\n";
    return;
  }

  std::error_code ec;
  auto size = fs::file_size(file_path_, ec);
  current_size_ = ec ? 0 : size;
}

void AsyncFileSink::RotateIfNeeded(std::chrono::system_clock::time_point now) {
  if (!rotation_policy_) return;
  if (!rotation_policy_->ShouldRotate(current_size_, now)) return;

  file_stream_.flush();
  file_stream_.close();

  std::string rotated_path;
  if (rotation_policy_->RequiresArchiving()) {
    rotated_path = rotation_policy_->GenerateRotatedFileName(file_path_, now);
    std::error_code ec;
    fs::rename(file_path_, rotated_path, ec);
    if (ec) {
      std::cerr << "[AsyncFileSink ERROR] Failed to rename file: "
                << ec.message() << "\n";
    }
  }

  OpenFile();
  rotation_policy_->OnRotationCompleted(file_path_, rotated_path);
}

}  // namespace stc::logger