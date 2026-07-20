/**
 * @file log_record.hpp
 * @brief Определение структуры данных, представляющей атомарную запись лога.
 * @version 3.0.0
 * @author Artem Ulyanov (aka s21::provemet)
 * @date 2026-07-17
 */

#pragma once

#include <chrono>
#include <source_location>
#include <string>

#include "stc/logger/core/log_level.hpp"

namespace stc::logger {

/**
 * @struct stc::logger::LogRecord
 * @brief Контейнер метаданных и содержимого одного события логирования.
 */
struct LogRecord {
  std::chrono::system_clock::time_point
      timestamp;  ///< Точное время генерации события.
  LogLevel level;       ///< Уровень важности сообщения.
  std::string message;  ///< Текстовое содержимое лога.
  std::source_location location;  ///< Контекст вызова (файл, функция, строка).
};

}  // namespace stc::logger