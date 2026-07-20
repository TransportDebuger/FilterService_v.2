/**
 * @file rotation_policy.hpp
 * @brief Объявление абстрактного интерфейса стратегии ротации лог-файлов.
 * @version 3.0.0
 * @author Artem Ulyanov (aka s21::provemet)
 * @date 2026-07-17
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace stc::logger {

/**
 * @class IRotationPolicy
 * @brief Абстрактный интерфейс стратегии ротации лог-файлов.
 */
class IRotationPolicy {
 public:
  virtual ~IRotationPolicy() = default;

  /**
   * @brief Проверяет, необходима ли ротация на основе текущего состояния файла.
   * @param current_file_size_bytes Текущий размер файла в байтах.
   * @param current_time Текущее системное время.
   * @return true если файл должен быть ротирован, иначе false.
   */
  virtual bool ShouldRotate(
      std::uint64_t current_file_size_bytes,
      std::chrono::system_clock::time_point current_time) const = 0;

  /**
   * @brief Генерирует путь для архивируемого (ротированного) файла.
   * @param original_file_path Исходный путь к лог-файлу.
   * @param rotation_time Время, в которое инициирована ротация.
   * @return Путь для переименованного файла.
   * @note Используется только если RequiresArchiving() == true.
   */
  virtual std::string GenerateRotatedFileName(
      const std::string& original_file_path,
      std::chrono::system_clock::time_point rotation_time) const = 0;

  /**
   * @brief Вызывается FileSink после успешного переименования файла.
   * @param original_file_path Путь к новому (созданному) лог-файлу.
   * @param rotated_file_path Путь к старому (переименованному) файлу.
   */
  virtual void OnRotationCompleted(const std::string& original_file_path,
                                   const std::string& rotated_file_path) = 0;

  /**
   * @brief Определяет тип физического действия при ротации.
   * @return true для политик архивации (файл переименовывается).
   * @return false для циклических политик (файл перезаписывается или
   * усекается).
   */
  virtual bool RequiresArchiving() const noexcept = 0;
};

}  // namespace stc::logger