/**
 * @file ilog_formatter.hpp
 * @brief Определение абстрактного интерфейса форматтера логирования.
 * @version 3.0.0
 * @author Artem Ulyanov (aka s21::provemet)
 * @date 2026-07-17
 */

#pragma once

#include <string>

#include "stc/logger/core/log_record.hpp"

namespace stc::logger {

/**
 * @class ILogFormatter
 * @brief Абстрактный базовый класс, определяющий контракт для всех реализаций
 * форматтеров.
 */
class ILogFormatter {
 public:
  virtual ~ILogFormatter() = default;

  /**
   * @brief Форматирует запись лога в строковое представление.
   * @param record Константная ссылка на анализируемую запись лога.
   * @return Строка, содержащая отформатированное сообщение, готовое к записи в
   * Sink.
   */
  virtual std::string Format(const LogRecord& record) const = 0;
};

}  // namespace stc::logger