/**
 * @file circular_rotation_policy.hpp
 * @brief Объявление стратегии циклической (кольцевой) ротации лог-файлов.
 * @version 3.0.0
 * @author Artem Ulyanov (aka s21::provemet)
 * @date 2026-07-17
 */

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "stc/logger/sinks/file/rotation_policy.hpp"

namespace stc::logger {

/**
 * @class CircularRotationPolicy
 * @brief Стратегия циклической (кольцевой) ротации лог-файлов.
 */
class CircularRotationPolicy final : public IRotationPolicy {
 public:
  /**
   * @brief Конструктор политики циклической ротации.
   * @param max_size_bytes Максимальный размер файла в байтах перед ротацией.
   * @param max_archives Количество слотов для хранения архивов (должно быть >
   * 0).
   * @throw std::invalid_argument Если max_size_bytes == 0 или max_archives ==
   * 0.
   */
  explicit CircularRotationPolicy(std::uint64_t max_size_bytes,
                                  std::size_t max_archives);

  ~CircularRotationPolicy() override = default;

  CircularRotationPolicy(const CircularRotationPolicy&) = delete;
  CircularRotationPolicy& operator=(const CircularRotationPolicy&) = delete;

  /**
   * @brief Проверяет, превышает ли текущий размер файла заданный лимит.
   * @param current_file_size_bytes Текущий размер файла в байтах.
   * @param current_time Текущее системное время (игнорируется в данной
   * реализации).
   * @return true если current_file_size_bytes >= max_size_bytes_, иначе false.
   */
  bool ShouldRotate(
      std::uint64_t current_file_size_bytes,
      std::chrono::system_clock::time_point current_time) const override;

  /**
   * @brief Генерирует имя для следующего архива по модулю max_archives.
   * @param original_file_path Исходный путь к лог-файлу.
   * @param rotation_time Время ротации (игнорируется, используется атомарный
   * счетчик).
   * @return Путь вида "original_file_path.N", где N = next_index_ %
   * max_archives.
   */
  std::string GenerateRotatedFileName(
      const std::string& original_file_path,
      std::chrono::system_clock::time_point rotation_time) const override;

  /**
   * @brief Инкрементирует индекс следующего слота для перезаписи.
   * @param original_file_path Путь к новому (созданному) лог-файлу
   * (игнорируется).
   * @param rotated_file_path Путь к старому (переименованному) файлу
   * (игнорируется).
   */
  void OnRotationCompleted(const std::string& original_file_path,
                           const std::string& rotated_file_path) override;

  /**
   * @brief Возвращает true, так как файл должен быть переименован в
   * фиксированный слот.
   * @return Всегда true.
   */
  bool RequiresArchiving() const noexcept override;

 private:
  /// @private
  std::uint64_t max_size_bytes_;  ///< Максимальный размер файла в байтах.

  /// @private
  std::size_t max_archives_;  ///< Количество слотов для хранения архивов.

  /// @private
  std::atomic<std::size_t> next_index_{
      0};  ///< Атомарный счетчик для определения следующего слота перезаписи.
};

}  // namespace stc::logger