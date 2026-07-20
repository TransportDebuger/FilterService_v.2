/**
 * @file size_rotation_policy.hpp
 * @brief Объявление стратегии ротации лог-файлов по достижении заданного
 * размера.
 * @version 3.0.0
 * @author Artem Ulyanov (aka s21::provemet)
 * @date 2026-07-17
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "stc/logger/sinks/file/rotation_policy.hpp"

namespace stc::logger {

/**
 * @class SizeRotationPolicy
 * @brief Стратегия ротации лог-файлов по достижении заданного размера.
 */
class SizeRotationPolicy final : public IRotationPolicy {
 public:
  /**
   * @brief Конструктор политики ротации по размеру.
   * @param max_size_bytes Максимальный размер файла в байтах перед ротацией.
   * @param max_archives Максимальное количество хранимых архивов.
   * @throw std::invalid_argument Если max_size_bytes == 0.
   */
  explicit SizeRotationPolicy(std::uint64_t max_size_bytes,
                              std::size_t max_archives = 5);

  ~SizeRotationPolicy() override = default;

  SizeRotationPolicy(const SizeRotationPolicy&) = delete;
  SizeRotationPolicy& operator=(const SizeRotationPolicy&) = delete;

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
   * @brief Генерирует имя для нового архива, находя следующий свободный индекс.
   * @param original_file_path Исходный путь к лог-файлу.
   * @param rotation_time Время ротации (игнорируется, используется индексный
   * суффикс).
   * @return Путь вида "original_file_path.N", где N — наименьший свободный
   * индекс >= 1.
   */
  std::string GenerateRotatedFileName(
      const std::string& original_file_path,
      std::chrono::system_clock::time_point rotation_time) const override;

  /**
   * @brief Удаляет устаревшие архивы, если их количество превышает лимит.
   * @param original_file_path Путь к новому (созданному) лог-файлу.
   * @param rotated_file_path Путь к старому (переименованному) файлу.
   */
  void OnRotationCompleted(const std::string& original_file_path,
                           const std::string& rotated_file_path) override;

  /**
   * @brief Возвращает true, так как файл должен быть переименован
   * (архивирован).
   * @return Всегда true.
   */
  bool RequiresArchiving() const noexcept override;

 private:
  std::uint64_t max_size_bytes_;
  std::size_t max_archives_;
};

}  // namespace stc::logger