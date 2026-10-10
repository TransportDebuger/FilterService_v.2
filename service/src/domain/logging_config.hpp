/**
 * @file logging_config.hpp
 * @brief Строгие структуры данных (DTO) для конфигурации
 *        подсистемы логирования.
 * @version 1.0.0
 * @date 2026-07-20
 */
#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace stc {

/**
 * @struct RotationConfig
 * @brief Параметры стратегии ротации лог-файлов.
 */
struct RotationConfig {
  /// @brief Тип ротации: "size", "time" или "circular".
  std::string type;
  /// @brief Максимальный размер файла в байтах (для "size" и "circular").
  std::uint64_t max_size_bytes{0};
  /// @brief Максимальное число хранимых архивов (для "size" и "circular").
  std::size_t max_archives{0};
  /// @brief Интервал ротации в секундах (для "time").
  std::chrono::seconds interval_sec{0};
  /// @brief Формат времени имени архива, совместимый со strftime (для "time").
  std::string time_format;
};

/**
 * @struct LoggerSinkConfig
 * @brief Параметры конфигурации одного приемника (Sink) логирования.
 */
struct LoggerSinkConfig {
  /// @brief Тип приемника: "console", "sync_file" или "async_file".
  std::string type;
  /** 
   * @brief Уровень логирования. Допустимые значения: "trace",
   *         "debug", "info", "warning", "error", "critical".
   */
  std::string level;
  /// @brief Тип форматтера: "text", "json" или "xml".
  std::string formatter;
  /// @brief Путь к лог-файлу (обязателен для "sync_file" и "async_file").
  std::string file_path;
  /// @brief Опциональные параметры ротации.
  std::optional<RotationConfig> rotation;
};

}  // namespace stc