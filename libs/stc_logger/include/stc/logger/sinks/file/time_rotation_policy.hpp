/**
 * @file time_rotation_policy.hpp
 * @brief Объявление стратегии ротации лог-файлов по достижении заданного
 * временного интервала.
 * @version 3.0.0
 * @author Artem Ulyanov (aka s21::provemet)
 * @date 2026-07-17
 */

#pragma once

#include <chrono>
#include <mutex>
#include <string>

#include "stc/logger/sinks/file/rotation_policy.hpp"

namespace stc::logger {

/**
 * @class TimeRotationPolicy
 * @brief Стратегия ротации лог-файлов по достижении заданного временного
 * интервала.
 */
class TimeRotationPolicy final : public IRotationPolicy {
 public:
  /**
   * @brief Конструктор политики ротации по времени.
   * @param interval Временной интервал между ротациями (например,
   * std::chrono::hours(24)).
   * @param time_format Формат временной метки для имени архива (совместим с
   * std::strftime).
   * @throw std::invalid_argument Если interval <= 0.
   */
  explicit TimeRotationPolicy(std::chrono::seconds interval,
                              std::string time_format = "%Y%m%d_%H%M%S");

  ~TimeRotationPolicy() override = default;

  TimeRotationPolicy(const TimeRotationPolicy&) = delete;
  TimeRotationPolicy& operator=(const TimeRotationPolicy&) = delete;

  /**
   * @brief Проверяет, наступило ли время следующей запланированной ротации.
   * @param current_file_size_bytes Текущий размер файла в байтах (игнорируется
   * в данной реализации).
   * @param current_time Текущее системное время для сравнения с
   * next_rotation_time_.
   * @return true если current_time >= next_rotation_time_, иначе false.
   */
  bool ShouldRotate(
      std::uint64_t current_file_size_bytes,
      std::chrono::system_clock::time_point current_time) const override;

  /**
   * @brief Генерирует имя для нового архива, добавляя временную метку.
   * @param original_file_path Исходный путь к лог-файлу.
   * @param rotation_time Время ротации, используемое для формирования суффикса.
   * @return Путь вида "original_file_path.YYYYMMDD_HHMMSS".
   */
  std::string GenerateRotatedFileName(
      const std::string& original_file_path,
      std::chrono::system_clock::time_point rotation_time) const override;

  /**
   * @brief Обновляет время следующей ротации после успешного переименования.
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
  /// @private
  std::chrono::seconds interval_;  ///< Временной интервал между ротациями.

  /// @private
  std::string time_format_;  ///< Формат временной метки для имени архива.

  /// @private
  std::chrono::system_clock::time_point
      next_rotation_time_;  ///< Время следующей запланированной ротации.

  /// @private
  mutable std::mutex
      mutex_;  ///< Мьютекс для защиты next_rotation_time_ от гонок данных.
};

}  // namespace stc::logger