#include "stc/logger/sinks/file/time_rotation_policy.hpp"

#include <ctime>
#include <stdexcept>

namespace stc::logger {

TimeRotationPolicy::TimeRotationPolicy(std::chrono::seconds interval,
                                       std::string time_format)
    : interval_(interval),
      time_format_(std::move(time_format)),
      // Инициализируем время первой ротации: текущее время + интервал
      next_rotation_time_(std::chrono::system_clock::now() + interval_) {
  if (interval_.count() <= 0) {
    throw std::invalid_argument(
        "TimeRotationPolicy: interval must be greater than 0");
  }
  if (time_format_.empty()) {
    throw std::invalid_argument(
        "TimeRotationPolicy: time_format cannot be empty");
  }
}

bool TimeRotationPolicy::ShouldRotate(
    std::uint64_t /*current_file_size_bytes*/,
    std::chrono::system_clock::time_point current_time) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return current_time >= next_rotation_time_;
}

std::string TimeRotationPolicy::GenerateRotatedFileName(
    const std::string& original_file_path,
    std::chrono::system_clock::time_point rotation_time) const {
  std::time_t t = std::chrono::system_clock::to_time_t(rotation_time);
  std::tm tm{};

  // Потокобезопасная версия localtime для POSIX (Linux)
  localtime_r(&t, &tm);

  char buffer[64];
  std::strftime(buffer, sizeof(buffer), time_format_.c_str(), &tm);

  // Формируем имя: original_path + "." + formatted_time
  return original_file_path + "." + buffer;
}

void TimeRotationPolicy::OnRotationCompleted(
    const std::string& /*original_file_path*/,
    const std::string& /*rotated_file_path*/) {
  std::lock_guard<std::mutex> lock(mutex_);

  // Предотвращение дрейфа: прибавляем интервал к ЗАПЛАНИРОВАННОМУ времени,
  // а не к текущему. Это гарантирует строгую периодичность (например, ровно в
  // 00:00).
  next_rotation_time_ += interval_;
}

bool TimeRotationPolicy::RequiresArchiving() const noexcept { return true; }

}  // namespace stc::logger