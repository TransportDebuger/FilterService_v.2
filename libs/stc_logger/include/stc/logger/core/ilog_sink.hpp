/**
 * @file ilog_sink.hpp
 * @brief Определение абстрактного интерфейса приемника (Sink) логирования.
 * @version 3.0.0
 * @author Artem Ulyanov (aka s21::provemet)
 * @date 2026-07-17
 */

#pragma once

#include <memory>
#include <string_view>

#include "stc/logger/core/ilog_filter.hpp"
#include "stc/logger/core/ilog_formatter.hpp"
#include "stc/logger/core/log_record.hpp"

namespace stc::logger {

/**
 * @class ILogSink
 * @brief Абстрактный базовый класс, определяющий контракт для всех
 *        приемников логов.
 */
class ILogSink {
 public:
  virtual ~ILogSink() = default;

  /**
   * @brief Записывает отформатированное сообщение в целевое хранилище.
   * @param record Оригинальная запись (для доступа к метаданным, если
   * необходимо).
   * @param formatted_message Уже отформатированная строка, готовая к записи.
   */
  virtual void Write(const LogRecord& record,
                     std::string_view formatted_message) = 0;

  /// @brief Принудительно сбрасывает внутренние буферы на диск или в сеть.
  virtual void Flush() = 0;

  /**
   * @brief Возвращает форматтер, связанный с этим приемником.
   * @return Умный указатель на реализацию ILogFormatter.
   */
  virtual std::shared_ptr<ILogFormatter> GetFormatter() const noexcept = 0;

  /**
   * @brief Возвращает локальный фильтр приемника.
   * @return Умный указатель на реализацию ILogFilter или nullptr, если фильтр
   *         не задан.
   */
  virtual std::shared_ptr<ILogFilter> GetFilter() const noexcept = 0;
};

}  // namespace stc::logger