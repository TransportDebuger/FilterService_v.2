#include "stc/logger/sinks/file/circular_rotation_policy.hpp"

#include <stdexcept>
#include <string>

namespace stc::logger {

CircularRotationPolicy::CircularRotationPolicy(std::uint64_t max_size_bytes,
                                               std::size_t max_archives)
    : max_size_bytes_(max_size_bytes), max_archives_(max_archives) {
  if (max_size_bytes_ == 0) {
    throw std::invalid_argument(
        "CircularRotationPolicy: max_size_bytes must be greater than 0");
  }
  if (max_archives_ == 0) {
    throw std::invalid_argument(
        "CircularRotationPolicy: max_archives must be greater than 0");
  }
}

bool CircularRotationPolicy::ShouldRotate(
    std::uint64_t current_file_size_bytes,
    std::chrono::system_clock::time_point /*current_time*/) const {
  return current_file_size_bytes >= max_size_bytes_;
}

std::string CircularRotationPolicy::GenerateRotatedFileName(
    const std::string& original_file_path,
    std::chrono::system_clock::time_point /*rotation_time*/) const {
  // Загружаем текущий индекс атомарно (lock-free)
  std::size_t current_idx = next_index_.load(std::memory_order_relaxed);

  // Формируем имя файла с суффиксом индекса слота
  return original_file_path + "." + std::to_string(current_idx);
}

void CircularRotationPolicy::OnRotationCompleted(
    const std::string& /*original_file_path*/,
    const std::string& /*rotated_file_path*/) {
  // Вычисляем следующий индекс по модулю количества слотов
  std::size_t current_idx = next_index_.load(std::memory_order_relaxed);
  std::size_t next_idx = (current_idx + 1) % max_archives_;

  // Атомарно сохраняем новый индекс
  next_index_.store(next_idx, std::memory_order_relaxed);
}

bool CircularRotationPolicy::RequiresArchiving() const noexcept { return true; }

}  // namespace stc::logger