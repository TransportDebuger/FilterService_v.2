#include "stc/logger/sinks/file/async_file_sink.hpp"

#include <filesystem>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace fs = std::filesystem;

namespace {
void ReportError(const std::function<void(const std::error_code&, std::string_view)>& callback,
                 const std::error_code& ec, std::string_view context) {
  if (callback) {
    callback(ec, context);
  } else {
    std::cerr << "[AsyncFileSink ERROR] " << context << ": " << ec.message() << "\n";
  }
}
}  // namespace

namespace stc::logger {

AsyncFileSink::AsyncFileSink(
    std::string file_path, std::shared_ptr<ILogFormatter> formatter,
    std::shared_ptr<ILogFilter> filter,
    std::shared_ptr<IRotationPolicy> rotation_policy,
    std::size_t max_batch_size, std::chrono::milliseconds flush_interval,
    std::size_t max_queue_size, OverflowPolicy overflow_policy,
    ErrorCallback error_callback)
    : file_path_(std::move(file_path)),
      formatter_(std::move(formatter)),
      filter_(std::move(filter)),
      rotation_policy_(std::move(rotation_policy)),
      max_batch_size_(max_batch_size),
      flush_interval_(flush_interval),
      max_queue_size_(max_queue_size),
      overflow_policy_(overflow_policy),
      error_callback_(std::move(error_callback)) {
  if (!formatter_) {
    throw std::invalid_argument("AsyncFileSink: formatter cannot be null");
  }

  OpenFile();
  worker_thread_ = std::jthread([this](std::stop_token stoken) { WorkerLoop(stoken); });
}

AsyncFileSink::~AsyncFileSink() {
  worker_thread_.request_stop();
  queue_cv_.notify_all();
  flush_cv_.notify_all();

  if (worker_thread_.joinable()) {
    worker_thread_.join();
  }

  if (file_stream_.is_open()) {
    file_stream_.flush();
    file_stream_.close();
  }
}

void AsyncFileSink::Write(const LogRecord& /*record*/,
                          std::string_view formatted_message) {
  std::unique_lock lock(queue_mutex_);

  if (max_queue_size_ > 0 && queue_.size() >= max_queue_size_) {
    switch (overflow_policy_) {
      case OverflowPolicy::kDrop:
        dropped_records_.fetch_add(1, std::memory_order_relaxed);
        return;
      case OverflowPolicy::kFailFast:
        throw std::overflow_error("AsyncFileSink: queue overflow");
      case OverflowPolicy::kBlock:
        queue_cv_.wait(lock, [this] {
          return queue_.size() < max_queue_size_ ||
                 worker_thread_.get_stop_token().stop_requested();
        });
        if (queue_.size() >= max_queue_size_) {
          dropped_records_.fetch_add(1, std::memory_order_relaxed);
          return;
        }
        break;
    }
  }

  queue_.emplace(formatted_message);
  lock.unlock();
  queue_cv_.notify_one();
}

void AsyncFileSink::Flush() {
  {
    std::lock_guard lock(queue_mutex_);
    flush_requested_ = true;
  }
  // Пробуждаем фоновый поток для обработки очереди
  queue_cv_.notify_one();

  std::unique_lock lock(flush_mutex_);
  flush_cv_.wait(lock, [this] { return !flush_requested_.load(); });
}

std::shared_ptr<ILogFormatter> AsyncFileSink::GetFormatter() const noexcept {
  return formatter_;
}

std::shared_ptr<ILogFilter> AsyncFileSink::GetFilter() const noexcept {
  return filter_;
}

std::uint64_t AsyncFileSink::GetDroppedRecordsCount() const noexcept {
  return dropped_records_.load(std::memory_order_relaxed);
}

void AsyncFileSink::WorkerLoop(std::stop_token stoken) {
  std::string batch_buffer;
  batch_buffer.reserve(max_batch_size_);

  while (!stoken.stop_requested() || !queue_.empty()) {
    batch_buffer.clear();
    auto now = std::chrono::system_clock::now();

    {
      std::unique_lock lock(queue_mutex_);
      if (queue_.empty() && !stoken.stop_requested() && !flush_requested_) {
        queue_cv_.wait_for(lock, flush_interval_, [&] {
          return !queue_.empty() || stoken.stop_requested() || flush_requested_;
        });
      }

      bool space_freed = false;
      while (!queue_.empty() && batch_buffer.size() < max_batch_size_) {
        batch_buffer += std::move(queue_.front());
        queue_.pop();
        space_freed = true;
      }

      if (space_freed && max_queue_size_ > 0) {
        queue_cv_.notify_all();
      }
    }

    if (!batch_buffer.empty()) {
      if (!file_stream_.is_open() || !file_stream_.good()) { // LCOV_EXCL_LINE
        file_stream_.clear();                                // LCOV_EXCL_LINE
        if (file_stream_.is_open()) file_stream_.close();    // LCOV_EXCL_LINE
        OpenFile();                                          // LCOV_EXCL_LINE
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

    if (flush_requested_) {
      if (file_stream_.is_open()) {
        file_stream_.flush();
      }
      flush_requested_ = false;
      flush_cv_.notify_all();
    }
  }
}

void AsyncFileSink::OpenFile() {
  file_stream_.open(file_path_, std::ios::app | std::ios::binary);

  if (!file_stream_.is_open()) {
    ReportError(error_callback_, std::make_error_code(std::io_errc::stream),
                "Failed to open file: " + file_path_);
    return;
  }

  std::error_code ec;
  auto size = fs::file_size(file_path_, ec);

  if (ec) {  // LCOV_EXCL_LINE
    ReportError(error_callback_, ec, "Failed to get file size: " + file_path_);  // LCOV_EXCL_LINE
    current_size_ = 0;  // LCOV_EXCL_LINE
  } else {  // LCOV_EXCL_LINE
    current_size_ = size;  // LCOV_EXCL_LINE
  }  // LCOV_EXCL_LINE
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
      ReportError(error_callback_, ec, "Failed to rename file for rotation");
    }
  }
  OpenFile();
  rotation_policy_->OnRotationCompleted(file_path_, rotated_path);
}

}  // namespace stc::logger